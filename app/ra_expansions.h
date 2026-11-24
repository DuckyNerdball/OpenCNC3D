/* Original expansion missions, listed by the source-built installation manifest.
   A selectable row always has both its original INI and its own 3D scene. */
static void ra_expansions_scan(void)
{
    g_specops.clear(); g_specopsRows.clear();
    FILE* f = fopen("ra3d/expansions.txt", "rb");
    if (!f) return;
    struct Row { std::string code, name, pack; int expansion, soviet; };
    std::vector<Row> rows;
    char line[512];
    while (fgets(line, sizeof line, f)) {
        char family[24], code[16], name[384];
        if (sscanf(line, "%23s %15s %383[^\r\n]", family, code, name) != 3) continue;
        const int family_id = !strcmp(family, "counterstrike") ? 0 : !strcmp(family, "aftermath") ? 1 : -1;
        if (family_id < 0 || strlen(code) != 7 || strncmp(code, "SC", 2)
            || (code[2] != 'G' && code[2] != 'U')) continue;
        bool valid = true;
        for (const char* p = code; *p; ++p) if (!((*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9'))) valid = false;
        if (!valid) continue;
        char ini[64], pack[64];
        snprintf(ini, sizeof ini, "ra3d/missions/%s.INI", code);
        snprintf(pack, sizeof pack, "ra3d/3d/%s.pack", code);
        FILE* original = fopen(ini, "rb"); if (!original) continue; fclose(original);
        FILE* scene = fopen(pack, "rb"); if (!scene) continue; fclose(scene);
        rows.push_back({code, name, pack, family_id, code[2] == 'U' ? 1 : 0});
    }
    fclose(f);
    for (int family = 0; family < 2; ++family) for (int soviet = 0; soviet < 2; ++soviet) {
        bool heading = false;
        for (const Row& row : rows) {
            if (row.expansion != family || row.soviet != soviet) continue;
            if (!heading) {
                SpecOp h;
                h.header = 1; h.group = family;
                h.name = std::string(soviet ? "SOVIET " : "ALLIED ") + (family ? "AFTERMATH" : "COUNTERSTRIKE");
                g_specops.push_back(h); heading = true;
            }
            SpecOp mission;
            mission.scen = row.code; mission.pack = row.pack; mission.name = row.name; mission.group = family;
            g_specops.push_back(mission);
        }
    }
    for (const SpecOp& row : g_specops) {
        DO_Mission display = {};
        display.scen = row.scen.c_str(); display.name = row.name.c_str();
        display.nod = row.scen.size() > 2 && row.scen[2] == 'U'; display.header = row.header;
        g_specopsRows.push_back(display);
        if (!row.header) printf("RAEXPANSION|mission|%s|%s|%s\n", row.group ? "aftermath" : "counterstrike", row.scen.c_str(), row.name.c_str());
    }
}

static bool ra_ant_scenario(const char* scen) {
    return scen && strlen(scen)==7 && !strncmp(scen,"SCA0",4)
        && scen[4]>='1' && scen[4]<='4' && !strcmp(scen+5,"EA");
}
static bool ra_ants_installed(void) {
    for(int i=1;i<=4;i++) {
        char path[80];snprintf(path,sizeof path,"ra3d/missions/SCA%02dEA.INI",i);
        FILE* f=fopen(path,"rb");if(!f)return false;fclose(f);
        snprintf(path,sizeof path,"ra3d/3d/SCA%02dEA.pack",i);
        f=fopen(path,"rb");if(!f)return false;fclose(f);
    }
    return true;
}
static void ra_ants_scan(void) {
    g_specops.clear();g_specopsRows.clear();
    FILE* f=fopen("ra3d/ants.txt","rb");if(!f)return;
    char line[512],family[24],code[16],name[384];
    while(fgets(line,sizeof line,f)) {
        if(sscanf(line,"%23s %15s %383[^\r\n]",family,code,name)!=3 || strcmp(family,"ants") || !ra_ant_scenario(code))continue;
        char path[80];snprintf(path,sizeof path,"ra3d/missions/%s.INI",code);
        FILE* original=fopen(path,"rb");if(!original)continue;fclose(original);
        snprintf(path,sizeof path,"ra3d/3d/%s.pack",code);
        FILE* scene=fopen(path,"rb");if(!scene)continue;fclose(scene);
        SpecOp row;row.scen=code;row.name=name;row.pack=path;g_specops.push_back(row);
    }
    fclose(f);
    for(const SpecOp& row:g_specops) {
        DO_Mission display={};display.scen=row.scen.c_str();display.name=row.name.c_str();g_specopsRows.push_back(display);
        printf("RAANTS|mission|%s|%s\n",row.scen.c_str(),row.name.c_str());
    }
}
