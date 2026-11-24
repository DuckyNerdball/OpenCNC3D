#ifndef OPENCNC3D_SCENERY_BOUNDS_H
#define OPENCNC3D_SCENERY_BOUNDS_H

#include <algorithm>
#include <cmath>
#include <limits>

/* CPU-only interval arithmetic for the whole scenery draw. Keep this independent
   of GL: a box must enclose the actual vertex pipeline before any clip test may
   reject it. Non-finite inputs fail open at the caller. */
struct SceneryBounds {
    double lo[3], hi[3];
    bool valid;
    SceneryBounds() : valid(false) {
        for (int k = 0; k < 3; k++) { lo[k] = 0; hi[k] = 0; }
    }
    bool add(float x, float y, float z) {
        const float p[3] = {x, y, z};
        for (int k = 0; k < 3; k++) if (!std::isfinite(p[k])) return false;
        for (int k = 0; k < 3; k++) {
            if (!valid) lo[k] = hi[k] = p[k];
            else { lo[k] = std::min(lo[k], (double)p[k]); hi[k] = std::max(hi[k], (double)p[k]); }
        }
        valid = true;
        return true;
    }
};

/* draw_mesh's weighted sway and flutter, over ALL clock phases and bearings.
   Weights are bytes in [0,255]. The three slow sine weights sum to one;
   the two flutter terms have absolute sum at most 1.55. The tiny excess covers
   stored single-precision sine/cosine phases and their multiply/add roundoff. */
static bool scenery_bounds_wind(SceneryBounds& box, float sway, float flutter)
{
    if (!box.valid || !std::isfinite(sway) || !std::isfinite(flutter)) return false;
    const double m = std::fabs((double)sway / 0.04107f) * 0.052001;
    const double f = std::fabs(0.007f * (double)flutter / 0.07143f) * 1.55001;
    box.lo[0] -= 1024.0 * m; box.hi[0] += 1024.0 * m;
    box.lo[2] -= 1024.0 * m; box.hi[2] += 1024.0 * m;
    box.lo[1] -= 1024.0 * (0.60001 * m * m + f);
    box.hi[1] += 1024.0 * f;
    return true;
}

/* Row-major 3x4, like draw_mesh's extraMat. Intervals allow negative scales
   and shear too; taking transformed min/max corners without signs does not. */
static bool scenery_bounds_affine(SceneryBounds& box, const float matrix[12])
{
    if (!box.valid) return false;
    SceneryBounds result;
    result.valid = true;
    for (int row = 0; row < 3; row++) {
        double lo = matrix[row * 4 + 3], hi = lo;
        double magnitude = std::fabs(lo);
        if (!std::isfinite(lo)) return false;
        for (int k = 0; k < 3; k++) {
            const double a = matrix[row * 4 + k];
            if (!std::isfinite(a)) return false;
            magnitude += std::fabs(a) * std::max(std::fabs(box.lo[k]), std::fabs(box.hi[k]));
            lo += a * (a < 0 ? box.hi[k] : box.lo[k]);
            hi += a * (a < 0 ? box.lo[k] : box.hi[k]);
        }
        if (!std::isfinite(lo) || !std::isfinite(hi)) return false;
        /* Enclose the float multiply/add order used by the renderer. */
        const double pad = (1.0 + magnitude) * 0.000002;
        result.lo[row] = lo - pad; result.hi[row] = hi + pad;
    }
    box = result;
    return true;
}

static bool scenery_bounds_export(const SceneryBounds& box, float lo[3], float hi[3])
{
    if (!box.valid) return false;
    for (int k = 0; k < 3; k++) {
        /* Include placement/terrain cancellation and depth polygon offset. */
        const double pad = 0.02 + std::max(std::fabs(box.lo[k]), std::fabs(box.hi[k])) * 0.000002;
        const double a = box.lo[k] - pad, b = box.hi[k] + pad;
        if (!std::isfinite(a) || !std::isfinite(b)
            || a < -std::numeric_limits<float>::max() || b > std::numeric_limits<float>::max()) return false;
        lo[k] = (float)a; hi[k] = (float)b;
    }
    return true;
}

#endif
