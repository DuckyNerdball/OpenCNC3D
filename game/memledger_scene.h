/* ---------------------------------------------------------------------------------- *
 *  THE MEMORY LEDGER'S SCENE READINGS, and the `memledger` script verb's body.
 *
 *  memledger.h holds the table and the counters that code sets as it goes. This half
 *  reads what the scene holds NOW off the structures that hold it, just before the
 *  table is printed, so a value is right whichever route loaded (or did not load) it:
 *
 *    unity_gl         Unity explosion flipbooks on the card, every mip level
 *    unity_seed       their recorded particle seeds in system memory (unity_seed_n of
 *                     them, in unity_packs packs)
 *    m3exp_tex        texture bytes on the card of the experimental 3D models (models3d/)
 *    m3enh_tex        the same for the remastered soldiers (einf/)
 *    decal_gl         Enhanced crater and scorch sheets on the card, every level
 *    decal_sets       which theater's sheets those are: "<theaterId>:body+soot[+snowsoot]"
 *    painted_decodes  remastered building decodes kept in system memory to paint house
 *                     colours from later (the GDI-look decode, its paint mask, any second
 *                     decode), for every texture that carries a paint mask
 *                     (painted_textures of them)
 *    painted_buildings  the part of painted_decodes a Red Alert building texture holds,
 *                     which it lets go once its liveries are painted when memory is
 *                     short (ebld_mod.h ebld_decodes_let_go; painted_buildings_n textures)
 *    house_decodes    the same kept for a cartridge house texture (no paint mask)
 *    readback_sites   where each counted whole-texture readback was asked, file:line*n
 *
 *  Counted elsewhere as they happen: mottle_gl (mottle_mod.h) and readbacks
 *  (memledger.h); and for Red Alert's building decodes (ebld_mod.h, cnc_eyes.cpp
 *  livery_build_one), present once something happened: painted_let_go (bytes let go),
 *  letgo_calls, redecodes, redecode_us and redecode_us_max (textures decoded again, and
 *  how long that took in microseconds), late_liveries (liveries painted after their
 *  texture's upload, for a colour first seen later), redecode_liveries (liveries painted
 *  from a decode made again), and with OPENCNC3D_EBLD_REDECODE_CHECK=1 redecode_checked
 *  and redecode_mismatch. A texel the driver stores in four bytes is counted as four, an RGB
 *  sheet included, since that is what the card is asked to hold for it.
 *
 *  On Windows the line also carries this process's committed bytes (private_usage), the
 *  memory cap of a job object it runs in (job_cap, 0 for none) and the commit room the
 *  game's own admission tests see (process_commit_room).
 *
 *  With OPENCNC3D_MEMDEBUG_TEXHASH=1 the verb then prints a TEXHASH| line for each large
 *  ground and water texture the scene holds (memledger.h texhash_gl), and for the two
 *  coast arrays the ground surface derives from a readback.
 * ---------------------------------------------------------------------------------- */
#ifndef OPENCNC3D_MEMLEDGER_SCENE_H
#define OPENCNC3D_MEMLEDGER_SCENE_H

/* the bytes of a texture whose levels halve down to 1 x 1 (or only level 0) */
static long long memledger_chain_bytes(int w, int h, int texelBytes, bool chain)
{
    long long b = 0;
    for (;;) {
        b += (long long)w * h * texelBytes;
        if (!chain || (w <= 1 && h <= 1)) break;
        w = w > 1 ? w / 2 : 1;
        h = h > 1 ? h / 2 : 1;
    }
    return b;
}

static void memledger_collect(void)
{
    long long uv = 0;
    for (size_t i = 0; i < g_uvTex.size(); i++)
        if (g_uvTex[i].gl) uv += memledger_chain_bytes(g_uvTex[i].w, g_uvTex[i].h, 4, g_uvTex[i].mips != 0);
    memledger_bytes("unity_gl", uv);
    long long seed = 0, seeds = 0;
    for (size_t p = 0; p < g_uvPacks.size(); p++)
        for (size_t s = 0; s < g_uvPacks[p].seed.size(); s++) {
            const UvfxSeed& sd = g_uvPacks[p].seed[s];
            seed += (long long)(sd.recs.capacity() * sizeof(float) + sd.off.capacity() * sizeof(unsigned)
                                + sd.cnt.capacity() * sizeof(unsigned));
            seeds++;
        }
    memledger_bytes("unity_seed", seed);
    memledger_count("unity_seed_n", seeds);
    memledger_count("unity_packs", (long long)g_uvPacks.size());

    long long m3exp = 0, m3enh = 0;
    for (size_t i = 0; i < g_m3.size(); i++)
        for (size_t t = 0; t < g_m3[i].tex.size(); t++) {
            const M3Tex& x = g_m3[i].tex[t];
            if (!x.gl) continue;
            (g_m3[i].enh ? m3enh : m3exp) += memledger_chain_bytes(x.w, x.h, 4, true);
        }
    memledger_bytes("m3exp_tex", m3exp);
    memledger_bytes("m3enh_tex", m3enh);

    long long decal = 0;
    std::string sets;
    for (size_t i = 0; i < g_deTh.size(); i++) {
        const DecalEnhTheater& T = g_deTh[i];
        char one[64];
        snprintf(one, sizeof one, "%s%d:%s%s%s%s", sets.empty() ? "" : ",", T.theaterId,
                 T.tex ? "body" : "", T.tex && T.soot ? "+" : "", T.soot ? "soot" : "",
                 T.sootSnow ? "+snowsoot" : "");
        sets += one;
        if (T.tex) decal += memledger_chain_bytes(T.texw, T.texh, 4, true);
        if (T.soot) decal += memledger_chain_bytes(T.sootw, T.sooth, 4, true);
        if (T.sootSnow) decal += memledger_chain_bytes(T.sootw, T.sooth, 4, true);
    }
    memledger_bytes("decal_gl", decal);
    memledger_text("decal_sets", sets.empty() ? "none" : sets);

    long long painted = 0, house = 0, paintedN = 0, building = 0, buildingN = 0;
    for (size_t i = 0; i < g_pack.tex.size(); i++) {
        const PackTex& pt = g_pack.tex[i];
        const long long held = (long long)(pt.gdi.capacity() + pt.nod.capacity() + pt.mask.capacity());
        if (!pt.mask.empty()) { painted += held; paintedN++; } else house += held;
        if (pt.redecodable && held) { building += held; buildingN++; }
    }
    memledger_bytes("painted_decodes", painted);
    memledger_count("painted_textures", paintedN);
    memledger_bytes("painted_buildings", building);
    memledger_count("painted_buildings_n", buildingN);
    memledger_bytes("house_decodes", house);

    memledger_gl_note(false);   /* water_gl, and every texture when asked (memledger_gl.h) */
    memledger_count("readbacks", memledger_get("readbacks"));   /* present (0) before any */
    const std::string where = memledger_readback_where();
    memledger_text("readback_sites", where.empty() ? "none" : where);

#ifdef _WIN32
    MEMORYSTATUSEX ms;
    memset(&ms, 0, sizeof ms);
    ms.dwLength = sizeof ms;
    if (GlobalMemoryStatusEx(&ms))
        memledger_bytes("process_commit_room", (long long)process_commit_room(ms));
    typedef BOOL (WINAPI *ProcMem)(HANDLE, PROCESS_MEMORY_COUNTERS*, DWORD);
    PROCESS_MEMORY_COUNTERS_EX pm;
    memset(&pm, 0, sizeof pm);
    pm.cb = sizeof pm;
    const HMODULE k32 = GetModuleHandleA("kernel32.dll");
    const ProcMem procMem = k32 ? (ProcMem)(void*)GetProcAddress(k32, "K32GetProcessMemoryInfo") : NULL;
    if (procMem && procMem(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pm, sizeof pm))
        memledger_bytes("private_usage", (long long)pm.PrivateUsage);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION job;
    memset(&job, 0, sizeof job);
    unsigned long long cap = 0;
    if (QueryInformationJobObject(NULL, JobObjectExtendedLimitInformation, &job, sizeof job, NULL)) {
        if (job.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_PROCESS_MEMORY)
            cap = (unsigned long long)job.ProcessMemoryLimit;
        if ((job.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_JOB_MEMORY)
            && (!cap || (unsigned long long)job.JobMemoryLimit < cap))
            cap = (unsigned long long)job.JobMemoryLimit;
    }
    memledger_bytes("job_cap", (long long)cap);
#endif
}

/* the ground and water textures and arrays a memory change could alter, hashed */
static void memledger_texhash_scene(void)
{
    if (!texhash_on()) return;
    texhash_gl("ra_sheet", g_ra3d.tex);
    texhash_gl("ra_dry_sheet", g_ra3d.dryTex);
    texhash_gl("ra_dry_seam_left", g_ra3d.drySeamL);
    texhash_gl("ra_dry_seam_top", g_ra3d.drySeamT);
    texhash_gl("ra_seam_left", g_ra3dSeams.texL);
    texhash_gl("ra_seam_top", g_ra3dSeams.texT);
    texhash_gl("td_seam_left", g_seams.texL);
    texhash_gl("td_seam_top", g_seams.texT);
    texhash_gl("td_sheet", g_pack.terrainTexEnh >= 0 && g_pack.terrainTexEnh < (int)g_pack.tex.size()
                           ? g_pack.tex[g_pack.terrainTexEnh].gl : 0);
    texhash_gl("water_land", g_waterLandTex);
    texhash_gl("water_bed", g_waterBedPatternTex);
    texhash_gl("water_field", g_waterFieldTex);
    texhash_gl("water_soft", g_waterSoftTex);
    for (int i = 0; i < 2; i++) {
        char nm[32];
        snprintf(nm, sizeof nm, "water_soft_slot%d", i);
        texhash_gl(nm, g_waterSoftSlot[i].tex);
    }
    for (int i = 0; i < 3; i++) {
        char nm[32];
        snprintf(nm, sizeof nm, "water_land_slot%d", i);
        texhash_gl(nm, g_waterLandSlot[i].tex);
        snprintf(nm, sizeof nm, "water_bed_slot%d", i);
        texhash_gl(nm, g_waterLandSlot[i].bedPattern);
    }
    texhash_gl("grass_mask", g_terrainGrassMasks.texture);
    texhash_gl("ground_material", g_terrainSurface.material);
    const TerrainSurfaceBase& B = g_terrainSurface.base;
    texhash_mem("surface_water", B.water.empty() ? NULL : &B.water[0], B.water.size());
    texhash_mem("surface_taper", B.taper.empty() ? NULL : &B.taper[0], B.taper.size() * sizeof(float));
}

/* memledger [LABEL]: the scene's readings, then the whole table as one MEM|ledger line,
   then the TEXHASH lines when they are switched on. The verb is also a failure
   injection site of its own ("memledger"), so the injection itself can be tested: an
   injected failure here is caught, reported as MEM|caught|memledger and the line is
   still printed. */
static void memledger_script(const char* label)
{
    try {
        memfail_point("memledger");
    } catch (const std::bad_alloc&) {
        printf("MEM|caught|memledger|injected\n");
    }
    memledger_collect();
    memledger_print(stdout, label ? label : "");
    memledger_texhash_scene();
}

#endif
