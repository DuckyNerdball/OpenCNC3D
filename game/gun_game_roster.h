#pragma once

static const char* gun_ra_name(const char* code)
{
    static const struct { const char* code; const char* name; } roster[] = {
        {"RTANY", "Tanya"}, {"SHOK", "Shock Trooper"}, {"3TNK", "Heavy Tank"},
        {"V2RL", "V2 Launcher"}, {"TTNK", "Tesla Tank"}, {"CTNK", "Chrono Tank"},
        {"HIND", "Hind"}, {"YAK", "Yak"}, {"MIG", "MiG"},
    };
    for (const auto& entry : roster)
        if (!strcmp(code, entry.code)) return entry.name;
    return nullptr;
}
