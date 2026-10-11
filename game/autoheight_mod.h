/* =====================================================================================
 *  autoheight_mod.h -- the auto heightmap generator, for the map editor.
 *
 *  A heightmap for a map that has none, from its tiles alone, in the style of the 55
 *  hand-made heightmaps the cartridge ships. This is a port of
 *  tools/autoheight/generate.py, which is where the method is developed, scored and
 *  explained (docs/auto-heightmap.md is the account). Everything the generator LEARNS
 *  from those 55 maps and every number it is TUNED by comes from autoheight_tables.h,
 *  which tools/autoheight/export_tables.py writes; this file carries only the arithmetic,
 *  stage for stage in the same order, so the two give the same ground. The one thing
 *  that cannot match byte for byte is the rolling field the swells are cut from: the
 *  Python draws it from numpy's generator, this from its own, so the swells sit in
 *  different places with the same size, shape and slopes.
 *
 *  Self-contained on purpose: no editor state is read here. The caller hands in the
 *  tiles, the playable rectangle, the buildings and the starts, and gets corners back,
 *  which is also what lets tools/autoheight/port_check.cpp hold it against the Python.
 *
 *  THE FOUR STAGES.
 *    1. LEVELS. Every cliff and waterfall placement is one equation: the ground two
 *       cells beyond its high side is one rung above the ground two beyond its low side.
 *       Neighbouring open cells weakly say "same level". Solved robustly (iteratively
 *       reweighted toward a least-absolute fit), so gaps in cliff lines close with the
 *       shortest boundary and contradictory art is out-voted. Playable rectangle only.
 *    2. RUNGS. Rounded onto the ladder, the lowest substantial land on rung 1; each body
 *       of water settles onto the lowest terrace that makes up a real share of its bank.
 *    3. SHAPE. Each cell's corners from what its icon looked like on the cartridge: a
 *       cliff icon at its measured fraction from the low rung to the high, anything else
 *       at its measured offset from its terrace. Bridges level at their banks, buildings
 *       on the highest terrace they stand on, walkable slopes spread into ramps.
 *    4. SWELLS. Broad, rounded, flat-crowned rises in big open patches, taller with
 *       more room, never steeper than a ramp.
 * ===================================================================================== */
#pragma once
#include <vector>
#include <map>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include "edit_tables.h"
#include "autoheight_tables.h"

struct AutoHeightIn {
    int W = 64, H = 64;                       /* cells                                 */
    const unsigned char* tmpl = nullptr;      /* template id per cell, row stride W   */
    const unsigned char* icon = nullptr;      /* icon per cell                         */
    int px = 0, py = 0, pw = 64, ph = 64;     /* the playable rectangle as the INI says */
    std::vector<std::vector<std::pair<int, int>>> foot;   /* pre-placed building cells */
    std::vector<std::pair<int, int>> starts;  /* multiplayer starts (empty otherwise)  */
    uint32_t seed = 0;                        /* the swells' field                     */
    bool swells = true;
};

enum { AHG_OK = 0, AHG_OUTVOTED = 1, AHG_CLAMPED = 2, AHG_ORPHAN = 3 };

struct AutoHeightOut {
    std::vector<unsigned char> corner;        /* (W+1)*(H+1), row major                */
    std::vector<unsigned char> mark;          /* W*H: where the fit overruled the art  */
    int places = 0, outvoted = 0, clamped = 0, orphans = 0;
};

namespace ahg {

/* not M_PI: the Windows cross build compiles in strict mode, where math.h need not have it */
static const double AHG_PI = 3.14159265358979323846;

struct Place { int t, ox, oy, w, h; std::vector<int> cells, icons; };
struct Step  { int t; std::vector<int> cells, icons; std::vector<int> hi, lo; int ux, uy; };

static inline int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }

static inline int prior_index(int t, int i)
{
    int slot = (t >= 0 && t < AHT_N_TEMPLATES) ? t : AHT_N_TEMPLATES;
    const int n = AHT_ICON_BASE[slot + 1] - AHT_ICON_BASE[slot];
    if (slot == AHT_N_TEMPLATES || i < 0 || i >= n) i = 0;
    return AHT_ICON_BASE[slot] + i;
}

static double median(std::vector<double> v)
{
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    return (n & 1) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

static inline double ladder_interp(double r)
{
    if (r <= 0) return AHT_LADDER[0];
    if (r >= 4) return AHT_LADDER[4];
    const int i = (int)std::floor(r);
    const double f = r - i;
    return AHT_LADDER[i] * (1.0 - f) + AHT_LADDER[std::min(4, i + 1)] * f;
}

static inline double smoothstep(double a, double b, double x)
{
    double t = (x - a) / (b - a);
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return t * t * (3.0 - 2.0 * t);
}

/* A mask grown by `it` steps of the four-neighbour cross (scipy's default structure). */
static void dilate(std::vector<unsigned char>& m, int W, int H, int it)
{
    std::vector<unsigned char> n(m.size());
    for (int k = 0; k < it; k++) {
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                const int i = y * W + x;
                n[i] = m[i] || (x > 0 && m[i - 1]) || (x < W - 1 && m[i + 1]) ||
                       (y > 0 && m[i - W]) || (y < H - 1 && m[i + W]);
            }
        m.swap(n);
    }
}

/* Exact Euclidean distance from every point to the nearest set point (Felzenszwalb's
   two-pass squared-distance transform). Unset grids answer a large number. */
static std::vector<double> edt(const std::vector<unsigned char>& set, int W, int H)
{
    const double INF = 1e18;
    std::vector<double> g(W * H);
    for (int i = 0; i < W * H; i++) g[i] = set[i] ? 0.0 : INF;
    auto pass = [](const double* f, double* d, int n, std::vector<int>& v, std::vector<double>& z) {
        int k = 0;
        v[0] = 0; z[0] = -1e30; z[1] = 1e30;
        for (int q = 1; q < n; q++) {
            double s;
            for (;;) {
                const int p = v[k];
                s = ((f[q] + (double)q * q) - (f[p] + (double)p * p)) / (2.0 * q - 2.0 * p);
                if (s <= z[k] && k > 0) { k--; continue; }
                break;
            }
            if (s <= z[k]) { v[k] = q; z[k] = -1e30; z[k + 1] = 1e30; continue; }
            k++; v[k] = q; z[k] = s; z[k + 1] = 1e30;
        }
        k = 0;
        for (int q = 0; q < n; q++) {
            while (z[k + 1] < q) k++;
            const double dq = q - v[k];
            d[q] = dq * dq + f[v[k]];
        }
    };
    const int M = std::max(W, H);
    std::vector<int> v(M + 1);
    std::vector<double> z(M + 2), f(M), d(M);
    for (int x = 0; x < W; x++) {
        for (int y = 0; y < H; y++) f[y] = g[y * W + x] >= INF ? 1e12 : g[y * W + x];
        pass(f.data(), d.data(), H, v, z);
        for (int y = 0; y < H; y++) g[y * W + x] = d[y];
    }
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) f[x] = g[y * W + x];
        pass(f.data(), d.data(), W, v, z);
        for (int x = 0; x < W; x++) g[y * W + x] = std::sqrt(d[x]);
    }
    return g;
}

/* scipy.ndimage.gaussian_filter, truncate 4: separable, `reflect` (half-sample
   symmetric) or `nearest` edges. */
static void gauss(std::vector<double>& a, int W, int H, double sigma, bool reflect)
{
    const int r = (int)(4.0 * sigma + 0.5);
    std::vector<double> k(2 * r + 1);
    double s = 0;
    for (int i = -r; i <= r; i++) { k[i + r] = std::exp(-0.5 * i * i / (sigma * sigma)); s += k[i + r]; }
    for (double& v : k) v /= s;
    auto at = [&](int i, int n) {
        if (!reflect) return clampi(i, 0, n - 1);
        while (i < 0 || i >= n) i = (i < 0) ? -i - 1 : 2 * n - i - 1;
        return i;
    };
    std::vector<double> t(a.size());
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            double acc = 0;
            for (int i = -r; i <= r; i++) acc += k[i + r] * a[y * W + at(x + i, W)];
            t[y * W + x] = acc;
        }
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            double acc = 0;
            for (int i = -r; i <= r; i++) acc += k[i + r] * t[at(y + i, H) * W + x];
            a[y * W + x] = acc;
        }
}

/* The swells' random numbers: splitmix64, normals by Box-Muller. */
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed * 0x9E3779B97F4A7C15ULL + 1) {}
    uint64_t next() { uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31); }
    double uni() { return ((next() >> 11) + 0.5) * (1.0 / 9007199254740992.0); }
    double normal() { return std::sqrt(-2.0 * std::log(uni())) * std::cos(6.283185307179586 * uni()); }
};

/* ---- the sparse least squares of stage 1 ------------------------------------------ */
struct Rows {
    std::vector<int> start{0}, col;
    std::vector<double> val, b, w0;
    void add(std::initializer_list<std::pair<int, double>> e, double target, double w) {
        for (auto& p : e) { col.push_back(p.first); val.push_back(p.second); }
        start.push_back((int)col.size()); b.push_back(target); w0.push_back(w);
    }
    void add(const std::vector<std::pair<int, double>>& e, double target, double w) {
        for (auto& p : e) { col.push_back(p.first); val.push_back(p.second); }
        start.push_back((int)col.size()); b.push_back(target); w0.push_back(w);
    }
    int n() const { return (int)b.size(); }
};

/* (A^T W A + reg I) x = A^T W b by Jacobi-preconditioned conjugate gradients, warm
   started from x. The system has a few thousand unknowns and the matrix is never formed. */
static void solve_normal(const Rows& R, const std::vector<double>& w, std::vector<double>& x, int n)
{
    const double reg = 1e-6;
    std::vector<double> diag(n, reg), rhs(n, 0.0), Ax(R.n()), q(n), r(n), z(n), p(n);
    for (int k = 0; k < R.n(); k++)
        for (int j = R.start[k]; j < R.start[k + 1]; j++) {
            diag[R.col[j]] += w[k] * R.val[j] * R.val[j];
            rhs[R.col[j]] += w[k] * R.val[j] * R.b[k];
        }
    auto mul = [&](const std::vector<double>& v, std::vector<double>& out) {
        for (int k = 0; k < R.n(); k++) {
            double s = 0;
            for (int j = R.start[k]; j < R.start[k + 1]; j++) s += R.val[j] * v[R.col[j]];
            Ax[k] = w[k] * s;
        }
        for (int i = 0; i < n; i++) out[i] = reg * v[i];
        for (int k = 0; k < R.n(); k++)
            for (int j = R.start[k]; j < R.start[k + 1]; j++) out[R.col[j]] += R.val[j] * Ax[k];
    };
    mul(x, q);
    double bn = 0;
    for (int i = 0; i < n; i++) { r[i] = rhs[i] - q[i]; bn += rhs[i] * rhs[i]; }
    bn = std::sqrt(bn) + 1e-30;
    for (int i = 0; i < n; i++) { z[i] = r[i] / diag[i]; p[i] = z[i]; }
    double rz = 0;
    for (int i = 0; i < n; i++) rz += r[i] * z[i];
    for (int it = 0; it < 4000; it++) {
        mul(p, q);
        double pq = 0;
        for (int i = 0; i < n; i++) pq += p[i] * q[i];
        if (pq <= 0) break;
        const double a = rz / pq;
        double rn = 0;
        for (int i = 0; i < n; i++) { x[i] += a * p[i]; r[i] -= a * q[i]; rn += r[i] * r[i]; }
        if (std::sqrt(rn) < 1e-10 * bn) break;
        double rz2 = 0;
        for (int i = 0; i < n; i++) { z[i] = r[i] / diag[i]; rz2 += r[i] * z[i]; }
        const double beta = rz2 / rz;
        rz = rz2;
        for (int i = 0; i < n; i++) p[i] = z[i] + beta * p[i];
    }
}

}  /* namespace ahg */

static void autoheight_generate(const AutoHeightIn& in, AutoHeightOut& out)
{
    using namespace ahg;
    const int W = in.W, H = in.H, NC = W * H, CW = W + 1, CH = H + 1;
    auto T_ = [&](int x, int y) { return (int)in.tmpl[y * W + x]; };
    auto I_ = [&](int x, int y) { return (int)in.icon[y * W + x]; };
    std::vector<int> land(NC), fam(NC);
    std::vector<unsigned char> pl(NC, 0), wet(NC), beach(NC);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const int i = y * W + x, t = T_(x, y);
            land[i] = edit_land_of(t, I_(x, y));
            fam[i] = AHT_FAMILY[t & 255];
            wet[i] = land[i] == EDIT_LAND_WATER;
            beach[i] = land[i] == EDIT_LAND_BEACH;
            pl[i] = x >= in.px && x < in.px + in.pw && y >= in.py && y < in.py + in.ph;
        }
    out.corner.assign(CW * CH, 0);
    out.mark.assign(NC, AHG_OK);

    /* placements, recovered from any of their cells */
    std::map<long long, Place> pm;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const int t = T_(x, y);
            if (t == 0 || t >= AHT_N_TEMPLATES) continue;
            const int tw = EDIT_TEMPLATES[t].w, th = EDIT_TEMPLATES[t].h, i = I_(x, y);
            if (i >= tw * th) continue;
            const int ox = x - i % tw, oy = y - i / tw;
            const long long key = ((long long)(oy + 512) << 32) | ((long long)(ox + 512) << 12) | t;
            Place& p = pm[key];
            if (p.cells.empty()) { p.t = t; p.ox = ox; p.oy = oy; p.w = tw; p.h = th; }
            p.cells.push_back(y * W + x); p.icons.push_back(i);
        }
    std::vector<unsigned char> step(NC, 0);
    std::vector<Step> pls;
    for (auto& kv : pm) {
        const Place& p = kv.second;
        if (!(AHT_STEP_DX[p.t] || AHT_STEP_DY[p.t])) continue;
        if ((double)p.cells.size() < AHT_STEP_MIN_SHARE * p.w * p.h) continue;
        Step s; s.t = p.t; s.cells = p.cells; s.icons = p.icons;
        s.ux = AHT_STEP_DX[p.t]; s.uy = AHT_STEP_DY[p.t];
        for (int c : p.cells) step[c] = 1;
        pls.push_back(s);
    }
    for (Step& s : pls) {
        std::vector<unsigned char> box(NC, 0);
        for (int c : s.cells) box[c] = 1;
        auto inbox = [&](int x, int y) { return x >= 0 && y >= 0 && x < W && y < H && box[y * W + x]; };
        std::vector<unsigned char> hi(NC, 0), lo(NC, 0);
        for (int c : s.cells) {
            const int x = c % W, y = c / W;
            for (int sgn = 1; sgn >= -1; sgn -= 2) {
                int k = 1;
                while (inbox(x + sgn * s.ux * k, y + sgn * s.uy * k)) k++;
                for (int d = 0; d < 2; d++) {
                    const int qx = x + sgn * s.ux * (k + d), qy = y + sgn * s.uy * (k + d);
                    if (qx >= 0 && qy >= 0 && qx < W && qy < H && !step[qy * W + qx])
                        (sgn > 0 ? hi : lo)[qy * W + qx] = 1;
                }
            }
        }
        for (int i = 0; i < NC; i++) { if (hi[i]) s.hi.push_back(i); if (lo[i]) s.lo.push_back(i); }
    }
    out.places = (int)pls.size();

    /* ---- 1. levels -------------------------------------------------------------- */
    Rows R;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const int i = y * W + x;
            if (!pl[i]) { R.add({{i, 1.0}}, 0.0, 1.0); continue; }
            R.add({{i, 1.0}}, 0.0, wet[i] ? AHT_W_BASE_WATER : AHT_W_BASE);
            static const int D[4][2] = { {1, 0}, {0, 1}, {1, 1}, {-1, 1} };
            for (int k = 0; k < 4; k++) {
                const int qx = x + D[k][0], qy = y + D[k][1];
                if (qx < 0 || qy < 0 || qx >= W || qy >= H || !pl[qy * W + qx]) continue;
                const int j = qy * W + qx;
                const double wb = k < 2 ? AHT_W_SMOOTH : AHT_W_DIAG;
                double w;
                if (step[i] || step[j]) w = 0.01;
                else if (wet[i] != wet[j]) w = AHT_W_WATERLAND;
                else if (beach[i] != beach[j] && !wet[i]) w = AHT_W_BEACHLAND * wb;
                else w = wb;
                if (w > 0) R.add({{i, 1.0}, {j, -1.0}}, 0.0, w);
            }
        }
    for (auto& fc : in.foot) {
        std::vector<int> c;
        for (auto& q : fc)
            if (q.first >= 0 && q.second >= 0 && q.first < W && q.second < H) {
                const int i = q.second * W + q.first;
                if (pl[i] && !step[i]) c.push_back(i);
            }
        for (size_t k = 1; k < c.size(); k++) R.add({{c[k - 1], 1.0}, {c[k], -1.0}}, 0.0, AHT_W_FOOT);
    }
    std::vector<unsigned char> site(NC, 0);
    const double sr = AHT_BASE_SITE_R;
    for (auto& s : in.starts)
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                if ((x - s.first) * (x - s.first) + (y - s.second) * (y - s.second) <= sr * sr)
                    site[y * W + x] = 1;
    if (!in.starts.empty())
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                const int i = y * W + x;
                auto ok = [&](int j) { return site[j] && pl[j] && !step[j] && !wet[j]; };
                if (!ok(i)) continue;
                if (x + 1 < W && ok(i + 1)) R.add({{i, 1.0}, {i + 1, -1.0}}, 0.0, AHT_W_FOOT);
                if (y + 1 < H && ok(i + W)) R.add({{i, 1.0}, {i + W, -1.0}}, 0.0, AHT_W_FOOT);
            }
    std::vector<int> placeRow(pls.size(), -1);
    for (size_t k = 0; k < pls.size(); k++) {
        std::vector<int> hi, lo;
        for (int c : pls[k].hi) if (pl[c]) hi.push_back(c);
        for (int c : pls[k].lo) if (pl[c]) lo.push_back(c);
        if (hi.empty() || lo.empty()) continue;
        std::vector<std::pair<int, double>> e;
        for (int c : hi) e.push_back({c, 1.0 / hi.size()});
        for (int c : lo) e.push_back({c, -1.0 / lo.size()});
        placeRow[k] = R.n();
        R.add(e, AHT_STEP[pls[k].t], AHT_W_PLACE);
    }
    std::vector<double> L(NC, 0.0), w = R.w0, res(R.n());
    for (int it = 0; it < (int)AHT_IRLS_ITERS; it++) {
        solve_normal(R, w, L, NC);
        for (int k = 0; k < R.n(); k++) {
            double s = 0;
            for (int j = R.start[k]; j < R.start[k + 1]; j++) s += R.val[j] * L[R.col[j]];
            res[k] = s - R.b[k];
            w[k] = R.w0[k] * AHT_IRLS_EPS / std::max(std::fabs(res[k]), (double)AHT_IRLS_EPS);
        }
    }
    /* a placement the solve could not honour by much is art the fit out-voted */
    for (size_t k = 0; k < pls.size(); k++)
        if (placeRow[k] >= 0 && std::fabs(res[placeRow[k]]) > 24.0) {
            out.outvoted++;
            for (int c : pls[k].cells) if (pl[c]) out.mark[c] = AHG_OUTVOTED;
        }

    /* ---- 2. rungs --------------------------------------------------------------- */
    std::vector<int> rung(NC, 1);
    {
        double sx = 0, sy = 0;
        int nd = 0;
        for (int i = 0; i < NC; i++)
            if (!step[i] && pl[i] && !wet[i]) {
                const double a = 2.0 * AHG_PI * L[i] / AHT_RUNG;
                sx += std::cos(a); sy += std::sin(a); nd++;
            }
        double o = std::atan2(nd ? sy / nd : 0.0, nd ? sx / nd : 1.0) / (2.0 * AHG_PI);
        o = o - std::floor(o);
        std::vector<int> r(NC);
        for (int i = 0; i < NC; i++) r[i] = (int)std::nearbyint(L[i] / AHT_RUNG - o);
        std::map<int, int> cnt;
        int tot = 0;
        for (int i = 0; i < NC; i++) if (!step[i] && pl[i] && !wet[i]) { cnt[r[i]]++; tot++; }
        int base = cnt.empty() ? 0 : cnt.begin()->first, found = 0;
        for (auto& kv : cnt)
            if (kv.second >= std::max(8.0, 0.02 * tot)) { base = kv.first; found = 1; break; }
        (void)found;
        for (int i = 0; i < NC; i++) {
            const int v = r[i] - base + 1;
            if ((v < 0 || v > 4) && pl[i] && !step[i]) {
                if (out.mark[i] == AHG_OK) out.clamped++;
                out.mark[i] = AHG_CLAMPED;
            }
            rung[i] = clampi(v, 0, 4);
        }
        /* outside the playable rectangle: the nearest playable cell's rung */
        std::vector<int> r2 = rung;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                if (pl[y * W + x]) continue;
                const int qx = clampi(x, in.px, in.px + in.pw - 1), qy = clampi(y, in.py, in.py + in.ph - 1);
                r2[y * W + x] = rung[clampi(qy, 0, H - 1) * W + clampi(qx, 0, W - 1)];
            }
        rung.swap(r2);
        /* water settles on the lowest terrace that makes up a real share of its bank */
        std::vector<int> lab(NC, 0);
        int nb = 0;
        for (int i = 0; i < NC; i++) {
            if (!wet[i] || step[i] || lab[i]) continue;
            nb++;
            std::vector<int> stack{i};
            lab[i] = nb;
            std::vector<int> body;
            while (!stack.empty()) {
                const int c = stack.back(); stack.pop_back(); body.push_back(c);
                const int x = c % W, y = c / W;
                const int nbr[4] = { x > 0 ? c - 1 : -1, x < W - 1 ? c + 1 : -1, y > 0 ? c - W : -1, y < H - 1 ? c + W : -1 };
                for (int j : nbr) if (j >= 0 && wet[j] && !step[j] && !lab[j]) { lab[j] = nb; stack.push_back(j); }
            }
            std::vector<unsigned char> m(NC, 0);
            for (int c : body) m[c] = 1;
            std::vector<unsigned char> g = m;
            dilate(g, W, H, 2);
            std::map<int, int> bc;
            int bt = 0;
            for (int j = 0; j < NC; j++)
                if (g[j] && !m[j] && !step[j] && !wet[j]) { bc[rung[j]]++; bt++; }
            if (!bt) continue;
            int lv = bc.begin()->first;
            for (auto& kv : bc)
                if (kv.second >= std::max(2.0, AHT_WATER_BANK_SHARE * bt)) { lv = kv.first; break; }
            for (int c : body) rung[c] = lv;
        }
    }

    /* ---- 3. shape --------------------------------------------------------------- */
    std::vector<double> T(NC);
    for (int i = 0; i < NC; i++) T[i] = AHT_LADDER[rung[i]];
    std::vector<double> num(CW * CH, 0), den(CW * CH, 0), snum(CW * CH, 0), sden(CW * CH, 0);
    static const int CO[4][2] = { {0, 0}, {1, 0}, {0, 1}, {1, 1} };
    for (Step& s : pls) {
        std::vector<double> vh, vl;
        for (int c : s.hi) vh.push_back(rung[c]);
        for (int c : s.lo) vl.push_back(rung[c]);
        if (vh.empty() && vl.empty()) continue;
        double rl = vl.empty() ? median(vh) - 1 : median(vl);
        double rh = vh.empty() ? rl + 1 : median(vh);
        if (rh <= rl) rh = rl + 1;
        const double lov = ladder_interp(rl), hiv = ladder_interp(rh);
        for (size_t k = 0; k < s.cells.size(); k++) {
            const int c = s.cells[k], x = c % W, y = c / W;
            const float* f = AHT_FRAC[prior_index(s.t, s.icons[k])];
            for (int q = 0; q < 4; q++) {
                const int ci = (y + CO[q][1]) * CW + x + CO[q][0];
                snum[ci] += lov + f[q] * (hiv - lov); sden[ci] += 1;
            }
        }
    }
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const int i = y * W + x;
            if (step[i]) continue;
            const int pi = prior_index(T_(x, y), I_(x, y));
            for (int q = 0; q < 4; q++) {
                const double sp = AHT_OFF_SP[pi][q], wg = 1.0 / (sp * sp);
                const int ci = (y + CO[q][1]) * CW + x + CO[q][0];
                num[ci] += wg * (T[i] + AHT_OFF_MU[pi][q]); den[ci] += wg;
            }
        }
    std::vector<double> h(CW * CH);
    std::vector<unsigned char> fixed(CW * CH);
    for (int i = 0; i < CW * CH; i++) {
        fixed[i] = sden[i] > 0;
        h[i] = sden[i] > 0 ? snum[i] / sden[i] : den[i] > 0 ? num[i] / den[i] : 0.0;
    }
    auto setcell = [&](int c, double v) {
        const int x = c % W, y = c / W;
        for (int q = 0; q < 4; q++) { const int ci = (y + CO[q][1]) * CW + x + CO[q][0]; h[ci] = v; fixed[ci] = 1; }
    };
    /* a bridge is a level deck at its banks' terrace */
    for (auto& kv : pm) {
        const Place& p = kv.second;
        if (AHT_FAMILY[p.t] != AHT_FAM_BRIDGE) continue;
        std::vector<unsigned char> m(NC, 0);
        for (int c : p.cells) m[c] = 1;
        std::vector<unsigned char> g = m;
        dilate(g, W, H, 1);
        std::vector<double> bank;
        for (int j = 0; j < NC; j++)
            if (g[j] && !m[j] && !step[j] && !wet[j] && fam[j] != AHT_FAM_BRIDGE) bank.push_back(T[j]);
        if (bank.empty()) continue;
        const double deck = median(bank);
        for (int c : p.cells) setcell(c, deck);
    }
    /* a building stands on the highest terrace under it */
    for (auto& fc : in.foot) {
        std::vector<int> c;
        for (auto& q : fc)
            if (q.first >= 0 && q.second >= 0 && q.first < W && q.second < H) {
                const int i = q.second * W + q.first;
                if (!step[i] && !wet[i]) c.push_back(i);
            }
        if (c.empty()) continue;
        double top = -1;
        for (int i : c) top = std::max(top, T[i]);
        for (int i : c) setcell(i, top);
    }
    /* ramps: walkable cells steeper than the cartridge draws them are spread */
    std::vector<unsigned char> wc(CW * CH), freec(CW * CH);
    for (int gy = 0; gy < CH; gy++)
        for (int gx = 0; gx < CW; gx++) {
            bool all = true;
            for (int dy = -1; dy <= 0; dy++)
                for (int dx = -1; dx <= 0; dx++)
                    all = all && wet[clampi(gy + dy, 0, H - 1) * W + clampi(gx + dx, 0, W - 1)];
            wc[gy * CW + gx] = all;
            freec[gy * CW + gx] = !fixed[gy * CW + gx] && !all;
        }
    std::vector<unsigned char> walk(NC);
    for (int i = 0; i < NC; i++)
        walk[i] = !step[i] && land[i] != EDIT_LAND_WATER && land[i] != EDIT_LAND_ROCK && land[i] != EDIT_LAND_WALL;
    auto span = [&](const std::vector<double>& a, int x, int y) {
        const double v0 = a[y * CW + x], v1 = a[y * CW + x + 1], v2 = a[(y + 1) * CW + x], v3 = a[(y + 1) * CW + x + 1];
        return std::max(std::max(v0, v1), std::max(v2, v3)) - std::min(std::min(v0, v1), std::min(v2, v3));
    };
    auto relax = [&](std::vector<double>& a, const std::vector<unsigned char>& band) {
        std::vector<double> n = a;
        for (int gy = 0; gy < CH; gy++)
            for (int gx = 0; gx < CW; gx++)
                if (band[gy * CW + gx])
                    n[gy * CW + gx] = 0.25 * (a[clampi(gy - 1, 0, CH - 1) * CW + gx] + a[clampi(gy + 1, 0, CH - 1) * CW + gx] +
                                              a[gy * CW + clampi(gx - 1, 0, CW - 1)] + a[gy * CW + clampi(gx + 1, 0, CW - 1)]);
        a.swap(n);
    };
    for (int round = 0; round < (int)AHT_RAMP_ROUNDS; round++) {
        std::vector<unsigned char> cb(CW * CH, 0);
        bool any = false;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                if (walk[y * W + x] && span(h, x, y) > AHT_RAMP_LIMIT) {
                    any = true;
                    for (int q = 0; q < 4; q++) cb[(y + CO[q][1]) * CW + x + CO[q][0]] = 1;
                }
        if (!any) break;
        dilate(cb, CW, CH, (int)AHT_RAMP_RING);
        for (int i = 0; i < CW * CH; i++) cb[i] = cb[i] && freec[i];
        for (int k = 0; k < (int)AHT_RAMP_ITERS; k++) relax(h, cb);
    }

    /* ---- 4. swells -------------------------------------------------------------- */
    if (in.swells) {
        std::vector<unsigned char> keep(NC);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                const int i = y * W + x, f = fam[i];
                keep[i] = step[i] || land[i] == EDIT_LAND_WATER || land[i] == EDIT_LAND_ROCK || land[i] == EDIT_LAND_WALL ||
                          f == AHT_FAM_WATER || f == AHT_FAM_RIVER || f == AHT_FAM_SHORE || f == AHT_FAM_FORD ||
                          f == AHT_FAM_FALLS || f == AHT_FAM_BRIDGE || span(h, x, y) > 2.0 ||
                          (!in.starts.empty() && site[i]);
            }
        for (auto& fc : in.foot)
            for (auto& q : fc)
                if (q.first >= 0 && q.second >= 0 && q.first < W && q.second < H) keep[q.second * W + q.first] = 1;
        std::vector<unsigned char> kc(CW * CH, 0);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                if (keep[y * W + x])
                    for (int q = 0; q < 4; q++) kc[(y + CO[q][1]) * CW + x + CO[q][0]] = 1;
        for (int i = 0; i < CW * CH; i++) kc[i] = kc[i] || fixed[i];
        const std::vector<double> room = edt(kc, CW, CH);
        std::vector<double> wgt(CW * CH);
        bool anyw = false;
        for (int i = 0; i < CW * CH; i++) { wgt[i] = smoothstep(AHT_SWELL_NEAR, AHT_SWELL_FULL, room[i]); anyw |= wgt[i] > 0; }
        if (anyw) {
            const int PAD = 12, FW = CW + 2 * PAD, FH = CH + 2 * PAD;
            Rng rng(in.seed);
            std::vector<double> big(FW * FH);
            for (double& v : big) v = rng.normal();
            gauss(big, FW, FH, AHT_SWELL_SIGMA, true);
            std::vector<double> fld(CW * CH);
            double mean = 0, var = 0;
            for (int y = 0; y < CH; y++) for (int x = 0; x < CW; x++) fld[y * CW + x] = big[(y + PAD) * FW + x + PAD];
            for (double v : fld) mean += v;
            mean /= fld.size();
            for (double v : fld) var += (v - mean) * (v - mean);
            const double sd = std::sqrt(var / fld.size()) + 1e-12;
            std::vector<double> lift(CW * CH);
            for (int i = 0; i < CW * CH; i++) {
                const double f = fld[i] / sd + AHT_SWELL_BIAS * wgt[i];
                lift[i] = std::min(AHT_SWELL_MAX * wgt[i] * smoothstep(AHT_SWELL_LOW, AHT_SWELL_HIGH, f),
                                   std::max(0.0, 250.0 - h[i]));
            }
            auto cap = [&]() {
                for (int k = 0; k < (int)AHT_SWELL_PASSES; k++) {
                    std::vector<double> n = lift;
                    for (int gy = 0; gy < CH; gy++)
                        for (int gx = 0; gx < CW; gx++) {
                            double m = lift[gy * CW + gx];
                            m = std::min(m, lift[clampi(gy - 1, 0, CH - 1) * CW + gx]);
                            m = std::min(m, lift[clampi(gy + 1, 0, CH - 1) * CW + gx]);
                            m = std::min(m, lift[gy * CW + clampi(gx - 1, 0, CW - 1)]);
                            m = std::min(m, lift[gy * CW + clampi(gx + 1, 0, CW - 1)]);
                            n[gy * CW + gx] = std::min(lift[gy * CW + gx], m + AHT_SWELL_STEP);
                        }
                    lift.swap(n);
                }
            };
            cap();
            gauss(lift, CW, CH, 1.0, false);
            for (int i = 0; i < CW * CH; i++) if (kc[i]) lift[i] = 0.0;
            cap();
            for (int i = 0; i < CW * CH; i++) h[i] += lift[i];
        }
    }

    /* orphan cliff art: a slope stamp too broken to carry a step */
    for (int i = 0; i < NC; i++)
        if (pl[i] && !step[i] && (AHT_STEP_DX[T_(i % W, i / W)] || AHT_STEP_DY[T_(i % W, i / W)]) &&
            out.mark[i] == AHG_OK) { out.mark[i] = AHG_ORPHAN; out.orphans++; }

    for (int i = 0; i < CW * CH; i++)
        out.corner[i] = (unsigned char)clampi((int)std::nearbyint(h[i]), 0, 255);
}
