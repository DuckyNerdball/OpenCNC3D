/* Imported RA maps use native engine terrain plus their own generated corners.
   The donor is an art container; none of its ground or holes belong to this map. */
#pragma once

static bool map_import_ra_ground(const GameOpts* o)
{
    if (!o || !o->ra3d || !o->scen || strncmp(o->scen, "IMP", 3)) return true;
    const std::string path = std::string(o->dir ? o->dir : "") + o->scen + ".HGT";
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { boot_refuse("The imported map heightmap %s is missing", path.c_str()); return false; }
    std::vector<unsigned char> heights(129 * 129);
    const size_t got = fread(heights.data(), 1, heights.size(), f);
    const bool exact = got == heights.size() && fgetc(f) == EOF && !ferror(f);
    fclose(f);
    if (!exact) { boot_refuse("The imported map heightmap %s is damaged", path.c_str()); return false; }
    edit_set_world_grid(128, 128);
    g_pack.corner.swap(heights);
    g_pack.cmtint.clear();
    snprintf(g_pack.theater, sizeof g_pack.theater, "%s",
             g_theater == CNC_THEATER_WINTER ? "RASNOW" :
             g_theater == CNC_THEATER_NONE ? "RAINT" : "RATEMP");
    snprintf(g_pack.scen, sizeof g_pack.scen, "%s", o->scen);
    memset(g_cellHoles, 0, sizeof g_cellHoles);
    for (PackCell& cell : g_pack.cell) cell.holes = 0;
    ++g_cellHolesGen;
    unsigned histogram[256] = {}, count = 0;
    for (int y = std::max(0, g_mapY); y <= std::min(128, g_mapY + g_mapH); ++y)
        for (int x = std::max(0, g_mapX); x <= std::min(128, g_mapX + g_mapW); ++x) {
            ++histogram[g_pack.corner[(size_t)y * 129 + x]]; ++count;
        }
    unsigned median = 0, sum = 0;
    for (unsigned i = 0; i < 256 && sum * 2 < count; ++i) { sum += histogram[i]; median = i; }
    g_terrainBase = median / 64.0f;
    memset(g_padOn, 0, sizeof g_padOn);
    g_shadeReady = false;
    ++g_groundGen;
    terrain_range_refresh();
    fprintf(stderr, "MAPIMPORT|ground|%s|%s|128x128|corners=%zu|datum=%u\n",
            o->scen, g_pack.theater, g_pack.corner.size(), median);
    return true;
}
