/* OpenCNC3D auto heightmap -- the editor's C++ generator run on its own, for port_check.py.
 *
 *   port_check <in.bin> <out.hgt>
 *
 * in.bin: int32 W H px py pw ph seed swells; W*H template bytes; W*H icon bytes;
 *         int32 nfoot, then per building int32 n and n (x, y) int32 pairs;
 *         int32 nstarts and that many (x, y) int32 pairs.
 * out.hgt: (W+1)*(H+1) corner bytes, then W*H mark bytes, then int32 places,
 *          outvoted, clamped, orphans. */
#include <cstdio>
#include "../../game/autoheight_mod.h"

static int rd(FILE* f) { int v = 0; if (fread(&v, 4, 1, f) != 1) v = 0; return v; }

int main(int argc, char** argv)
{
    if (argc < 3) { fprintf(stderr, "usage: port_check in.bin out.hgt\n"); return 2; }
    FILE* f = fopen(argv[1], "rb");
    if (!f) return 1;
    AutoHeightIn in;
    in.W = rd(f); in.H = rd(f); in.px = rd(f); in.py = rd(f); in.pw = rd(f); in.ph = rd(f);
    in.seed = (uint32_t)rd(f); in.swells = rd(f) != 0;
    std::vector<unsigned char> t(in.W * in.H), ic(in.W * in.H);
    if (fread(t.data(), 1, t.size(), f) != t.size() || fread(ic.data(), 1, ic.size(), f) != ic.size()) return 1;
    in.tmpl = t.data(); in.icon = ic.data();
    const int nf = rd(f);
    for (int k = 0; k < nf; k++) {
        const int n = rd(f);
        std::vector<std::pair<int, int>> c;
        for (int j = 0; j < n; j++) { const int x = rd(f); const int y = rd(f); c.push_back({x, y}); }
        in.foot.push_back(c);
    }
    const int ns = rd(f);
    for (int k = 0; k < ns; k++) { const int x = rd(f); const int y = rd(f); in.starts.push_back({x, y}); }
    fclose(f);
    AutoHeightOut out;
    autoheight_generate(in, out);
    FILE* o = fopen(argv[2], "wb");
    fwrite(out.corner.data(), 1, out.corner.size(), o);
    fwrite(out.mark.data(), 1, out.mark.size(), o);
    int s[4] = { out.places, out.outvoted, out.clamped, out.orphans };
    fwrite(s, 4, 4, o);
    fclose(o);
    return 0;
}
