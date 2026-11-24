/* =====================================================================================
 *  ra3d_interior_mod.h -- Red Alert's indoor missions under Enhanced: their walls in 3D.
 *
 *  Seven missions are set indoors (three on the 1996 discs, four on the expansion discs),
 *  drawn from INTERIOR.MIX's templates. The 1996 art draws their walls flat on the floor;
 *  under Enhanced each mission's walls stand as the remastered lab's approved kit, baked
 *  per mission by tools/ra3d/bake-interior.py into ra3d/interior/<SCEN>.einw (one atlas,
 *  ra3d/interior/walls_atlas.rgbz, for all of them), over the map the engine exports: the
 *  playable rectangle and the one-cell rim round it, which the engine keeps shrouded:
 *
 *    THE WALLS are a block on every cell the engine blocks, a cell thick and 1.15 cells
 *    high, bevelled and panelled on their room sides and broken on their void sides, made
 *    per cell from the cell's eight neighbours. The file lists each wall cell's own
 *    triangles, so a wall is drawn only once the player has revealed its cell: the walls
 *    come out of the shroud with the room, and nothing stands up out of the black over a
 *    room nobody has seen.
 *    THE FLOOR SWAP: where the walls stand in 3D the game casts their shadows, so a floor
 *    piece the 1996 art drew with a wall's shadow on it draws as the plain floor (the
 *    pieces share the plain floor's layout, icon for icon), and the floor icon beside a cut
 *    corner or under a wall console, which carried part of the wall's art, draws as a plain
 *    floor icon chosen by the cell (ra3d_interior_floor_swap, read by the ground's sheet).
 *
 *  Only with Red Alert's module, under Enhanced, on a mission whose file is there: Classic
 *  and Tiberian Dawn never reach any of it, and a mission without a file draws exactly as
 *  before. `rainterior 0` draws the 1996 art again (the walls flat, the shadow floors back)
 *  for an A/B; `rainterior` alone dumps what is loaded and drawn.
 * ===================================================================================== */
#pragma once

static bool g_ra3dInteriorOn = true;

/* the walls are drawn and the floors swapped: Red Alert, Enhanced, the switch on (whether the
   mission has walls is the loader's: ra3d_interior_have) */
static bool ra3d_interior_wanted(void)
{
    return g_ra3dInteriorOn && g_fxActive && g_fx.enabled && !g_fx.dos_classic && ra3d_own_ground();
}

/* THE FLOOR SWAP (tools/art/remaster/ra/interior.py FLOOR_SWAP is the same table): the
   template and icon a cell's ground draws, given the exported ones and the cell */
static void ra3d_interior_floor_swap(char name[17], int* icon, int x, int y)
{
    static const struct { const char* from; const char* to; bool cell1; } swap[] = {
        {"FLOR0002", "FLOR0001", false}, {"FLOR0003", "FLOR0001", false}, {"FLOR0004", "FLOR0001", false},
        {"FLOR0005", "FLOR0001", false}, {"FLOR0006", "FLOR0001", false}, {"FLOR0007", "FLOR0001", false},
        {"GFLR0002", "GFLR0001", false}, {"GFLR0003", "GFLR0001", false}, {"GFLR0004", "GFLR0001", false},
        {"GFLR0005", "GFLR0001", false},
        {"XTRA0002", "GFLR0001", true}, {"XTRA0004", "GFLR0001", true},
        {"XTRA0006", "FLOR0001", true}, {"XTRA0008", "FLOR0001", true},
        {"XTRA0012", "GFLR0001", true}, {"XTRA0013", "GFLR0001", true},
    };
    for (size_t k = 0; k < sizeof swap / sizeof swap[0]; k++) {
        if (strcmp(name, swap[k].from)) continue;
        if (swap[k].cell1) {
            /* only the template's second icon, its floor: one of the plain floor's twelve,
               by the cell's place on the map, so neighbours differ */
            if (*icon != 1) return;
            *icon = (x & 3) + 4 * (y % 3);
        }
        strcpy(name, swap[k].to);
        return;
    }
}

/* ---- THE WALLS ---------------------------------------------------------------------- */
#define RA3D_EINW_MAGIC  "RAINTW01"
#define RA3D_EINWA_MAGIC "RAINTA01"
/* the cells per mesh unit the remasters are built in (MODEL_SCALE, 1/1024 of a cell) */
#define RA3D_EINW_UNITS 1024.0f

struct Ra3dInteriorCell { int x, y; uint32_t first, count; int mesh; };
struct Ra3dInterior {
    std::string scene;                   /* the mission the file was read for            */
    bool tried = false, ok = false;      /* read (or found missing) for that mission      */
    std::vector<Ra3dInteriorCell> cells;
    std::vector<float> verts;            /* the file's vertices, 8 floats each            */
    std::vector<unsigned char> atlas;    /* RGBA, aw x ah                                 */
    int aw = 0, ah = 0;
    int tex = -1;                        /* the atlas's slot in g_pack.tex, once built    */
    GLuint gl = 0;
    size_t meshBase = (size_t)-1;        /* where the cells' meshes start in g_pack.mesh  */
    long triangles = 0;
    long drawn[3] = { 0, 0, 0 };         /* cells drawn last frame: colour, casters/bake, normals */
};
static Ra3dInterior g_ra3dInt;

static const char* ra3d_interior_dir(void)
{
    const char* e = getenv("OPENCNC3D_RAINTERIOR");
    return e && *e ? e : "ra3d/interior";
}

static bool ra3d_interior_read_atlas(Ra3dInterior& I)
{
    char path[512];
    snprintf(path, sizeof path, "%s/walls_atlas.rgbz", ra3d_interior_dir());
    const std::vector<unsigned char> b = ra3d_read_file(path);
    if (b.size() < 16 || memcmp(&b[0], RA3D_EINWA_MAGIC, 8)) return false;
    I.aw = (int)rt_le32(&b[8]);
    I.ah = (int)rt_le32(&b[12]);
    if (I.aw <= 0 || I.ah <= 0 || I.aw > 4096 || I.ah > 4096) return false;
    uLongf n = (uLongf)I.aw * I.ah * 4;
    I.atlas.assign((size_t)n, 0);
    return uncompress(&I.atlas[0], &n, &b[16], (uLong)(b.size() - 16)) == Z_OK
        && n == (uLongf)I.aw * I.ah * 4;
}

/* The mission's walls file: read once a mission, the vertices kept to build meshes from.
   A mission with no file (every mission not set indoors) answers false and draws as before. */
static bool ra3d_interior_have(void)
{
    const std::string scene = g_raStaticScene.empty() ? std::string(g_bootScen) : g_raStaticScene;
    Ra3dInterior& I = g_ra3dInt;
    if (I.tried && I.scene == scene) return I.ok;
    I = Ra3dInterior();
    I.tried = true;
    I.scene = scene;
    if (!g_raModule || scene.empty()) return false;
    char path[512];
    snprintf(path, sizeof path, "%s/%s.einw", ra3d_interior_dir(), scene.c_str());
    const std::vector<unsigned char> b = ra3d_read_file(path);
    if (b.empty()) return false;                 /* not an indoor mission */
    bool good = b.size() >= 16 && !memcmp(&b[0], RA3D_EINW_MAGIC, 8);
    const uint32_t nc = good ? rt_le32(&b[8]) : 0, nv = good ? rt_le32(&b[12]) : 0;
    good = good && b.size() == 16 + (size_t)nc * 12 + (size_t)nv * 32 && nc <= 128 * 128;
    for (uint32_t i = 0; good && i < nc; i++) {
        const unsigned char* e = &b[16 + (size_t)i * 12];
        Ra3dInteriorCell c;
        c.x = e[0] | (e[1] << 8); c.y = e[2] | (e[3] << 8);
        c.first = rt_le32(e + 4); c.count = rt_le32(e + 8); c.mesh = -1;
        good = c.x < 128 && c.y < 128 && c.count % 3 == 0 && (uint64_t)c.first + c.count <= nv;
        I.cells.push_back(c);
    }
    if (good) {
        I.verts.resize((size_t)nv * 8);
        memcpy(&I.verts[0], &b[16 + (size_t)nc * 12], (size_t)nv * 32);
        good = ra3d_interior_read_atlas(I);
    }
    I.ok = good;
    I.triangles = good ? (long)nv / 3 : 0;
    fprintf(stderr, "RA3D|interior|walls|scene=%s|%s|cells=%u|triangles=%ld|atlas=%dx%d\n", scene.c_str(),
            good ? "read" : "BAD FILE, the walls stay flat", nc, I.triangles, I.aw, I.ah);
    if (!good) I.cells.clear();
    return I.ok;
}

/* the floor swap is on: the walls stand in 3D on this mission */
static bool ra3d_interior_swapping(void)
{
    return ra3d_interior_wanted() && ra3d_interior_have();
}

/* THE MESHES, one a wall cell, in the pack's mesh list (the remasters' own way into every
   pass, draw_mesh), and the atlas in its texture bank. Built at the first draw, and again
   whenever the pack has been loaded since: its lists are replaced then. */
static bool ra3d_interior_meshes(void)
{
    Ra3dInterior& I = g_ra3dInt;
    if (!I.ok || I.cells.empty()) return false;
    if (I.meshBase != (size_t)-1 && I.meshBase + I.cells.size() <= g_pack.mesh.size() && I.tex >= 0
        && I.tex < (int)g_pack.tex.size() && g_pack.tex[I.tex].gl == I.gl) {
        char want[17];
        snprintf(want, sizeof want, "einw:%d,%d", I.cells[0].x, I.cells[0].y);
        if (!strncmp(g_pack.mesh[I.meshBase].name, want, sizeof want)) return true;
    }
    if (I.gl) { glDeleteTextures(1, &I.gl); I.gl = 0; }
    I.gl = ebld_upload_mips(I.atlas, I.aw, I.ah, false);
    PackTex pt;
    pt.w = pt.uw = I.aw;
    pt.h = pt.uh = I.ah;
    pt.gl = pt.gl_gdi = I.gl;
    for (int e = 0; e < 7; e++) pt.gl_extra[e] = 0;
    pt.mips = true;
    I.tex = (int)g_pack.tex.size();
    g_pack.tex.push_back(pt);
    I.meshBase = g_pack.mesh.size();
    for (Ra3dInteriorCell& c : I.cells) {
        PackMesh pm;
        memset(pm.name, 0, sizeof pm.name);
        snprintf(pm.name, sizeof pm.name, "einw:%d,%d", c.x, c.y);
        const float cx = (float)c.x + 0.5f, cz = (float)c.y + 0.5f;
        for (uint32_t t = 0; t < c.count / 3; t++) {
            PackTri tri;
            tri.tex = I.tex;
            tri.mode = MODE_OPAQUE;
            tri.wrap = 0;
            for (int k = 0; k < 3; k++) {
                const float* s = &I.verts[((size_t)c.first + t * 3 + k) * 8];
                PackVert& d = tri.v[k];
                /* the file: x east, y south, z up, in cells from the map's corner; the mesh:
                   x east, y up, z south, in mesh units from the cell's middle */
                d.x = (s[0] - cx) * RA3D_EINW_UNITS;
                d.y = s[2] * RA3D_EINW_UNITS;
                d.z = (s[1] - cz) * RA3D_EINW_UNITS;
                d.u = s[3]; d.v = s[4];
                d.r = d.g = d.b = d.a = 255;
                /* its normal for the normal target: the file's x east, y north, z up */
                pm.nrm.push_back(s[5]); pm.nrm.push_back(s[7]); pm.nrm.push_back(-s[6]);
            }
            pm.tfl.push_back(0);
            pm.tris.push_back(tri);
        }
        PackPart part;
        part.tri0 = 0; part.ntris = (int)pm.tris.size(); part.role = ROLE_STATIC;
        part.px = part.py = part.pz = 0.0f;
        pm.parts.assign(1, part);
        pm.modeMask = 1u << MODE_OPAQUE;
        c.mesh = (int)g_pack.mesh.size();
        g_pack.mesh.push_back(pm);
    }
    fprintf(stderr, "RA3D|interior|meshes|scene=%s|cells=%zu|triangles=%ld|tex=%d\n", I.scene.c_str(),
            I.cells.size(), I.triangles, I.tex);
    return true;
}

/* THE DRAW, in whatever pass the caller is in (the colour pass, the sun's casters and the
   relight bake, the normal target): every wall cell the player has revealed and the pass's
   box holds. slot: 0 colour, 1 casters or bake, 2 normals, for the counts. */
static void ra3d_interior_draw(int pass, int slot)
{
    if (slot < 0 || slot > 2) return;
    g_ra3dInt.drawn[slot] = 0;
    if (!ra3d_interior_swapping() || !ra3d_interior_meshes()) return;
    long n = 0;
    for (const Ra3dInteriorCell& c : g_ra3dInt.cells) {
        if (!cell_shown(c.x, c.y) || shroud_cell_hidden(c.x, c.y) || !cell_in_view(c.x, c.y)) continue;
        draw_mesh(c.mesh, (float)c.x + 0.5f, (float)c.y + 0.5f, -1, pass, 0.0f, 0.0f, 0, 1.0f, 0.0f,
                  false, WOBBLE_NONE, -1.0f, NULL, 0, 0);
        n++;
    }
    g_ra3dInt.drawn[slot] = n;
}

static void ra3d_interior_dump(void)
{
    const Ra3dInterior& I = g_ra3dInt;
    int hidden = 0;
    for (const Ra3dInteriorCell& c : I.cells) hidden += shroud_cell_hidden(c.x, c.y) ? 1 : 0;
    printf("RA3DINT|scene=%s|on=%d|file=%d|cells=%zu|triangles=%ld|hidden=%d|"
           "drawn=%ld|aux=%ld|normals=%ld|meshes=%d\n", I.scene.c_str(), g_ra3dInteriorOn ? 1 : 0,
           I.ok ? 1 : 0, I.cells.size(), I.triangles, hidden,
           I.drawn[0], I.drawn[1], I.drawn[2], I.meshBase == (size_t)-1 ? 0 : 1);
    fflush(stdout);
}
