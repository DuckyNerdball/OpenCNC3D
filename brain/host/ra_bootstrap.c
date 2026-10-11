/*
 * OpenCNC3D Red Alert Phase 2 bootstrap host.
 *
 * This deliberately uses only Red Alert's stock remaster ABI. It loads the
 * synthetic RA3DBOOT scenario, advances it, and emits a small canonical report
 * that can be compared byte for byte across supported platforms.
 */

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
typedef HMODULE cnc_lib_t;
#define CNC_CALL __cdecl
#define cnc_lib_open(path) LoadLibraryA(path)
#define cnc_lib_symbol(lib, name) ((void*)GetProcAddress((lib), (name)))
static const char* cnc_lib_error(void)
{
    static char text[64];
    snprintf(text, sizeof(text), "GetLastError()=%lu", (unsigned long)GetLastError());
    return text;
}
#else
#include <dlfcn.h>
typedef void* cnc_lib_t;
#define CNC_CALL
#define cnc_lib_open(path) dlopen((path), RTLD_NOW | RTLD_LOCAL)
#define cnc_lib_symbol(lib, name) dlsym((lib), (name))
#define cnc_lib_error() dlerror()
#endif

typedef unsigned char cnc_bool;
typedef unsigned long long cnc_u64;
typedef long long cnc_i64;

#define RA3D_API_VERSION 258U
#define RA3D_SEED 19005
#define RA3D_SCENARIO "RA3DBOOT"
#define RA3D_MISSION "SCG01EA"
#define RA3D_TIMER_TICKS 300
#define RA3D_DEFAULT_TICKS 60
#define RA3D_SIDEBAR_BYTES (1024U * 1024U)
#define RA3D_MAX_CELLS (128 * 128)

#if defined(_WIN32)
#define CNC_MAX_FNAME_EXT (256 + 256)
#else
#define CNC_MAX_FNAME_EXT (255 + 8)
#endif

enum
{
    GAME_STATE_STATIC_MAP = 1,
    GAME_STATE_SIDEBAR = 4,
    CALLBACK_EVENT_DEBUG_PRINT = 3
};

#pragma pack(push, 1)

typedef struct
{
    char TemplateTypeName[32];
    int IconNumber;
} CNCStaticCellStruct;

typedef struct
{
    int MapCellX;
    int MapCellY;
    int MapCellWidth;
    int MapCellHeight;
    int OriginalMapCellX;
    int OriginalMapCellY;
    int OriginalMapCellWidth;
    int OriginalMapCellHeight;
    int Theater;
    char ScenarioName[CNC_MAX_FNAME_EXT];
    CNCStaticCellStruct StaticCells[RA3D_MAX_CELLS];
} CNCMapDataStruct;

typedef struct
{
    int EntryCount[2];
    int Credits;
    int CreditsCounter;
    int Tiberium;
    int MaxTiberium;
    int PowerProduced;
    int PowerDrained;
    int MissionTimer;
    unsigned int UnitsKilled;
    unsigned int BuildingsKilled;
    unsigned int UnitsLost;
    unsigned int BuildingsLost;
    unsigned int TotalHarvestedCredits;
    cnc_bool RepairBtnEnabled;
    cnc_bool SellBtnEnabled;
    cnc_bool RadarMapActive;
} CNCSidebarHeader;

typedef struct
{
    float FirepowerBias;
    float GroundspeedBias;
    float AirspeedBias;
    float ArmorBias;
    float ROFBias;
    float CostBias;
    float BuildSpeedBias;
    float RepairDelay;
    float BuildDelay;
    cnc_bool IsBuildSlowdown;
    cnc_bool IsWallDestroyer;
    cnc_bool IsContentScan;
} CNCDifficultyDataStruct;

typedef struct
{
    CNCDifficultyDataStruct Difficulties[3];
} CNCRulesDataStruct;

typedef struct
{
    int EventType;
    cnc_i64 GlyphXPlayerID;
    union
    {
        struct
        {
            const char* PrintString;
        } DebugPrint;
        unsigned char Reserved[512];
    } Data;
} EventCallbackStruct;

#pragma pack(pop)

_Static_assert(sizeof(CNCStaticCellStruct) == 36, "static cell ABI drift");
_Static_assert(sizeof(CNCDifficultyDataStruct) == 39, "difficulty ABI drift");
_Static_assert(sizeof(CNCRulesDataStruct) == 117, "rules ABI drift");
_Static_assert(offsetof(CNCSidebarHeader, MissionTimer) == 32, "sidebar ABI drift");
_Static_assert(sizeof(CNCSidebarHeader) == 59, "sidebar header ABI drift");
_Static_assert(offsetof(EventCallbackStruct, Data) == 12, "event ABI drift");
#if defined(_WIN32)
_Static_assert(sizeof(CNCMapDataStruct) == 590372, "Windows map ABI drift");
#else
_Static_assert(sizeof(CNCMapDataStruct) == 590123, "macOS map ABI drift");
#endif

typedef void(CNC_CALL* CNCEventCallback)(const EventCallbackStruct*);
typedef unsigned int(CNC_CALL* fn_CNC_Version)(unsigned int);
typedef void(CNC_CALL* fn_CNC_Init)(const char*, CNCEventCallback);
typedef void(CNC_CALL* fn_CNC_Config)(const CNCRulesDataStruct*);
typedef void(CNC_CALL* fn_CNC_Add_Mod_Path)(const char*);
typedef cnc_bool(CNC_CALL* fn_CNC_Start_Custom_Instance)(const char*, const char*, const char*, int, cnc_bool);
typedef cnc_bool(CNC_CALL* fn_CNC_Start_Instance_Variation)(int, int, int, int, const char*, const char*, const char*, int, const char*);
typedef int(CNC_CALL* fn_OpenCNC3D_Dump_Objects)(void);
typedef void(CNC_CALL* fn_OpenCNC3D_Get_View)(int*);
typedef cnc_bool(CNC_CALL* fn_OpenCNC3D_Cell_To_Pixel)(int, int, int*, int*);
typedef int(CNC_CALL* fn_OpenCNC3D_Dump_Selection)(void);
typedef cnc_bool(CNC_CALL* fn_OpenCNC3D_Sel_Info)(int, int*);
typedef const char*(CNC_CALL* fn_OpenCNC3D_Mission_Name)(int);
typedef int(CNC_CALL* fn_OpenCNC3D_Probe_Object_At)(int, int);
typedef cnc_bool(CNC_CALL* fn_CNC_Select_Object)(cnc_u64, int, int);
typedef cnc_bool(CNC_CALL* fn_CNC_Clear_Object_Selection)(cnc_u64);
typedef cnc_bool(CNC_CALL* fn_CNC_Advance_Instance)(cnc_u64);
typedef cnc_bool(CNC_CALL* fn_CNC_Get_Game_State)(int, cnc_u64, unsigned char*, unsigned int);
typedef void(CNC_CALL* fn_CNC_Start_Mission_Timer)(int);

static char LastStage[192] = "not-started";

static void CNC_CALL event_callback(const EventCallbackStruct* event)
{
    const char* message;
    if (event == NULL || event->EventType != CALLBACK_EVENT_DEBUG_PRINT) {
        return;
    }
    message = event->Data.DebugPrint.PrintString;
    if (message == NULL) {
        return;
    }
    if (strncmp(message, "RA3D|stage|", 11) == 0) {
        snprintf(LastStage, sizeof(LastStage), "%s", message + 11);
        fprintf(stderr, "RA_BOOT_STAGE|%s\n", LastStage);
        fflush(stderr);
    }
}

static void fail(const char* message)
{
    fprintf(stderr, "RA_BOOT_ERROR|stage=%s|%s\n", LastStage, message);
}

static void* require_symbol(cnc_lib_t library, const char* name)
{
    void* symbol = cnc_lib_symbol(library, name);
    if (symbol == NULL) {
        fprintf(stderr, "RA_BOOT_ERROR|stage=%s|missing-symbol=%s\n", LastStage, name);
    }
    return symbol;
}

static void fill_rules(CNCRulesDataStruct* rules)
{
    static const float easy[9] = {1.2f, 1.2f, 1.2f, 0.3f, 0.8f, 0.8f, 0.8f, 0.001f, 0.001f};
    static const float normal[9] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.02f, 0.03f};
    static const float hard[9] = {0.9f, 0.9f, 0.9f, 1.05f, 1.05f, 1.0f, 1.0f, 0.03f, 0.03f};
    static const float* values[3] = {easy, normal, hard};
    static const cnc_bool slowdown[3] = {0, 1, 1};
    static const cnc_bool wall_destroyer[3] = {1, 1, 1};
    static const cnc_bool content_scan[3] = {1, 1, 0};
    int difficulty;
    memset(rules, 0, sizeof(*rules));
    for (difficulty = 0; difficulty < 3; ++difficulty) {
        CNCDifficultyDataStruct* out = &rules->Difficulties[difficulty];
        out->FirepowerBias = values[difficulty][0];
        out->GroundspeedBias = values[difficulty][1];
        out->AirspeedBias = values[difficulty][2];
        out->ArmorBias = values[difficulty][3];
        out->ROFBias = values[difficulty][4];
        out->CostBias = values[difficulty][5];
        out->BuildSpeedBias = values[difficulty][6];
        out->RepairDelay = values[difficulty][7];
        out->BuildDelay = values[difficulty][8];
        out->IsBuildSlowdown = slowdown[difficulty];
        out->IsWallDestroyer = wall_destroyer[difficulty];
        out->IsContentScan = content_scan[difficulty];
    }
}

static int make_directory_path(char* output, size_t output_size, const char* input)
{
    size_t length = strlen(input);
    if (length + 2 > output_size) {
        return 0;
    }
    memcpy(output, input, length + 1);
    if (length == 0 || (output[length - 1] != '/' && output[length - 1] != '\\')) {
        output[length] = '/';
        output[length + 1] = '\0';
    }
    return 1;
}

static void canonical_scenario_name(char* output, size_t output_size, const char* input, size_t input_size)
{
    char copy[CNC_MAX_FNAME_EXT + 1];
    const char* base;
    char* extension;
    size_t length = input_size;
    size_t index;
    size_t base_length;
    while (length > 0 && input[length - 1] == '\0') {
        --length;
    }
    if (length > CNC_MAX_FNAME_EXT) {
        length = CNC_MAX_FNAME_EXT;
    }
    memcpy(copy, input, length);
    copy[length] = '\0';
    base = copy;
    for (index = 0; index < length; ++index) {
        if (copy[index] == '/' || copy[index] == '\\') {
            base = copy + index + 1;
        }
    }
    extension = strrchr((char*)base, '.');
    if (extension != NULL) {
        *extension = '\0';
    }
    base_length = strlen(base);
    if (base_length >= output_size) {
        base_length = output_size - 1;
    }
    memcpy(output, base, base_length);
    output[base_length] = '\0';
    for (index = 0; output[index] != '\0'; ++index) {
        output[index] = (char)toupper((unsigned char)output[index]);
    }
}

static int read_timer(fn_CNC_Get_Game_State get_state, unsigned char* buffer, int* timer)
{
    CNCSidebarHeader* sidebar = (CNCSidebarHeader*)buffer;
    memset(buffer, 0, RA3D_SIDEBAR_BYTES);
    if (!get_state(GAME_STATE_SIDEBAR, 0, buffer, RA3D_SIDEBAR_BYTES)) {
        return 0;
    }
    *timer = sidebar->MissionTimer;
    return 1;
}

static void usage(const char* program)
{
    fprintf(stderr, "usage: %s <RedAlert library> <scenario directory> [60 [mission|mission-abi]]\n", program);
}

int main(int argc, char** argv)
{
    cnc_lib_t library;
    fn_CNC_Version version;
    fn_CNC_Init init;
    fn_CNC_Config config;
    fn_CNC_Add_Mod_Path add_mod_path;
    fn_CNC_Start_Custom_Instance start_custom;
    fn_CNC_Start_Instance_Variation start_mission = NULL;
    fn_OpenCNC3D_Dump_Objects dump_objects = NULL;
    fn_OpenCNC3D_Get_View get_view = NULL;
    fn_OpenCNC3D_Cell_To_Pixel cell_to_pixel = NULL;
    fn_OpenCNC3D_Dump_Selection dump_selection = NULL;
    fn_OpenCNC3D_Sel_Info sel_info = NULL;
    fn_OpenCNC3D_Mission_Name mission_name = NULL;
    fn_OpenCNC3D_Probe_Object_At probe_object = NULL;
    fn_CNC_Select_Object select_object = NULL;
    fn_CNC_Clear_Object_Selection clear_selection = NULL;
    fn_CNC_Advance_Instance advance;
    fn_CNC_Get_Game_State get_state;
    fn_CNC_Start_Mission_Timer start_timer;
    CNCRulesDataStruct rules;
    CNCMapDataStruct* map;
    unsigned char* sidebar_buffer;
    char scenario_directory[4096];
    char scenario_name[128];
    char command_line[64];
    int ticks = RA3D_DEFAULT_TICKS;
    int tick;
    int initial_timer;
    int final_timer;
    int sample_ticks[4];
    int sample_timers[4];
    unsigned int api_version;
    int real_mission;
    int abi_probe;
    int objects_at_start;
    int objects_at_end;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    if (argc < 3 || argc > 5) {
        usage(argv[0]);
        return 2;
    }
    if (argc >= 4) {
        ticks = atoi(argv[3]);
    }
    abi_probe = argc == 5 && strcmp(argv[4], "mission-abi") == 0;
    real_mission = argc == 5 && (strcmp(argv[4], "mission") == 0 || abi_probe);
    if (argc == 5 && !real_mission) {
        usage(argv[0]);
        return 2;
    }
    if (ticks != RA3D_DEFAULT_TICKS) {
        fail("the canonical Phase 2 gate requires exactly 60 ticks");
        return 2;
    }
    if (!make_directory_path(scenario_directory, sizeof(scenario_directory), argv[2])) {
        fail("fixture path is too long");
        return 2;
    }

    library = cnc_lib_open(argv[1]);
    if (library == NULL) {
        fprintf(stderr, "RA_BOOT_ERROR|stage=load|%s\n", cnc_lib_error());
        return 3;
    }
    version = (fn_CNC_Version)require_symbol(library, "CNC_Version");
    init = (fn_CNC_Init)require_symbol(library, "CNC_Init");
    config = (fn_CNC_Config)require_symbol(library, "CNC_Config");
    add_mod_path = (fn_CNC_Add_Mod_Path)require_symbol(library, "CNC_Add_Mod_Path");
    start_custom = (fn_CNC_Start_Custom_Instance)require_symbol(library, "CNC_Start_Custom_Instance");
    advance = (fn_CNC_Advance_Instance)require_symbol(library, "CNC_Advance_Instance");
    get_state = (fn_CNC_Get_Game_State)require_symbol(library, "CNC_Get_Game_State");
    start_timer = (fn_CNC_Start_Mission_Timer)require_symbol(library, "CNC_Start_Mission_Timer");
    if (real_mission) {
        start_mission = (fn_CNC_Start_Instance_Variation)require_symbol(library, "CNC_Start_Instance_Variation");
        dump_objects = (fn_OpenCNC3D_Dump_Objects)require_symbol(library, "OpenCNC3D_Dump_Objects");
    }
    if (abi_probe) {
        get_view = (fn_OpenCNC3D_Get_View)require_symbol(library, "OpenCNC3D_Get_View");
        cell_to_pixel = (fn_OpenCNC3D_Cell_To_Pixel)require_symbol(library, "OpenCNC3D_Cell_To_Pixel");
        dump_selection = (fn_OpenCNC3D_Dump_Selection)require_symbol(library, "OpenCNC3D_Dump_Selection");
        sel_info = (fn_OpenCNC3D_Sel_Info)require_symbol(library, "OpenCNC3D_Sel_Info");
        mission_name = (fn_OpenCNC3D_Mission_Name)require_symbol(library, "OpenCNC3D_Mission_Name");
        probe_object = (fn_OpenCNC3D_Probe_Object_At)require_symbol(library, "OpenCNC3D_Probe_Object_At");
        select_object = (fn_CNC_Select_Object)require_symbol(library, "CNC_Select_Object");
        clear_selection = (fn_CNC_Clear_Object_Selection)require_symbol(library, "CNC_Clear_Object_Selection");
    }
    if (version == NULL || init == NULL || config == NULL || add_mod_path == NULL || start_custom == NULL
        || advance == NULL || get_state == NULL || start_timer == NULL
        || (real_mission && (start_mission == NULL || dump_objects == NULL))
        || (abi_probe && (get_view == NULL || cell_to_pixel == NULL || dump_selection == NULL
                          || sel_info == NULL || mission_name == NULL || probe_object == NULL
                          || select_object == NULL || clear_selection == NULL))) {
        return 4;
    }

    add_mod_path(argv[2]);
    snprintf(command_line, sizeof(command_line), "-HEADLESS -SEED=%d", RA3D_SEED);
    init(command_line, event_callback);
    api_version = version(0);
    if (api_version != RA3D_API_VERSION) {
        fail("unexpected CNC ABI version");
        return 5;
    }
    fill_rules(&rules);
    config(&rules);
    if (real_mission) {
        if (!start_mission(1, 0, 0, 10, "GREECE", "GAME_NORMAL", argv[2], 0, "SCG01EA.INI")) {
            fail("CNC_Start_Instance_Variation returned false");
            return 6;
        }
    } else if (!start_custom(argv[2], scenario_directory, RA3D_SCENARIO, 10, 0)) {
        fail("CNC_Start_Custom_Instance returned false");
        return 6;
    }

    map = (CNCMapDataStruct*)calloc(1, sizeof(*map));
    sidebar_buffer = (unsigned char*)calloc(1, RA3D_SIDEBAR_BYTES);
    if (map == NULL || sidebar_buffer == NULL) {
        fail("out of memory");
        return 7;
    }
    if (!get_state(GAME_STATE_STATIC_MAP, 0, (unsigned char*)map, (unsigned int)sizeof(*map))) {
        fail("static map state unavailable");
        return 8;
    }
    canonical_scenario_name(scenario_name, sizeof(scenario_name), map->ScenarioName, sizeof(map->ScenarioName));
    if (real_mission) {
        if (strcmp(scenario_name, RA3D_MISSION) != 0 || map->Theater != 3
            || map->OriginalMapCellX != 49 || map->OriginalMapCellY != 45
            || map->OriginalMapCellWidth != 30 || map->OriginalMapCellHeight != 36) {
            fail("static map does not match the disc mission");
            return 9;
        }
        if (abi_probe) {
            int view[11] = {0};
            int selected[8] = {0};
            int px = -1, py = -1;
            int pixel_ok;
            const char* name;
            get_view(view);
            printf("RA_ABI|view=%d,%d,%d,%d|origin=%d,%d|map=%d,%d,%d,%d|legacy=%d\n",
                   view[0], view[1], view[2], view[3], view[4], view[5],
                   view[6], view[7], view[8], view[9], view[10]);
            if (view[6] != 49 || view[7] != 45 || view[8] != 30 || view[9] != 36
                || dump_selection() != 0 || sel_info(0, selected)) {
                fail("initial renderer ABI state differs from the disc mission");
                return 14;
            }
            /* SCG01EA has a Greece JEEP in the unit heap at id 2. Select it through
               the stock RA interface, then read back the engine's actual selection. */
            if (!select_object(0, 2, 2) || dump_selection() != 1
                || !sel_info(0, selected) || selected[1] != 62 || selected[2] != 50) {
                fail("Red Alert unit selection did not reach the renderer ABI");
                return 15;
            }
            name = mission_name(selected[5]);
            if (name == NULL || strcmp(name, "Guard") != 0) {
                fail("selected unit mission name is not Guard");
                return 16;
            }
            printf("RA_ABI|selected=UNIT,2|cell=%d,%d|mission=%s|nav=%d|tar=%d\n",
                   selected[1], selected[2], name, selected[6], selected[7]);
            pixel_ok = cell_to_pixel(selected[1], selected[2], &px, &py);
            printf("RA_ABI|pixel=%d|px=%d|py=%d\n", pixel_ok, px, py);
            probe_object(px, py);
            if (dump_selection() != 1 || !clear_selection(0) || dump_selection() != 0) {
                fail("read-only probe changed selection or clear failed");
                return 17;
            }
            free(sidebar_buffer);
            free(map);
            return 0;
        }
        printf("RA_MISSION|v=1|api=%u|requested_seed=%d|scenario=%s|theater=%d|rect=%d,%d,%d,%d\n",
               api_version, RA3D_SEED, scenario_name, map->Theater,
               map->OriginalMapCellX, map->OriginalMapCellY,
               map->OriginalMapCellWidth, map->OriginalMapCellHeight);
        printf("TICK|0\n");
        objects_at_start = dump_objects();
        printf("COUNT|0|%d\n", objects_at_start);
        if (objects_at_start <= 0) {
            fail("disc mission has no exported objects");
            return 13;
        }
        for (tick = 1; tick <= ticks; ++tick) {
            if (!advance(0)) {
                fail("disc mission stopped before the requested tick count");
                return 11;
            }
        }
        printf("TICK|%d\n", ticks);
        objects_at_end = dump_objects();
        printf("COUNT|%d|%d\n", ticks, objects_at_end);
        if (objects_at_end <= 0) {
            fail("disc mission lost all exported objects");
            return 13;
        }
        free(sidebar_buffer);
        free(map);
        return 0;
    }
    if (strcmp(scenario_name, RA3D_SCENARIO) != 0 || map->Theater != 2 || map->OriginalMapCellX != 20
        || map->OriginalMapCellY != 20 || map->OriginalMapCellWidth != 24 || map->OriginalMapCellHeight != 24) {
        fail("static map does not match the synthetic fixture");
        return 9;
    }

    start_timer(RA3D_TIMER_TICKS);
    if (!read_timer(get_state, sidebar_buffer, &initial_timer)) {
        fail("initial sidebar state unavailable");
        return 10;
    }
    sample_ticks[0] = 15;
    sample_ticks[1] = 30;
    sample_ticks[2] = 45;
    sample_ticks[3] = 60;
    for (tick = 1; tick <= ticks; ++tick) {
        if (!advance(0)) {
            fail("simulation stopped before the requested tick count");
            return 11;
        }
        if (tick % 15 == 0 && !read_timer(get_state, sidebar_buffer, &sample_timers[(tick / 15) - 1])) {
            fail("sidebar state unavailable after advance");
            return 12;
        }
    }
    final_timer = sample_timers[3];
    if (final_timer >= initial_timer) {
        fail("mission timer did not advance");
        return 13;
    }

    printf("RA_BOOT|v=1|api=%u|requested_seed=%d|scenario=%s|theater=%d|rect=%d,%d,%d,%d\n",
           api_version,
           RA3D_SEED,
           scenario_name,
           map->Theater,
           map->OriginalMapCellX,
           map->OriginalMapCellY,
           map->OriginalMapCellWidth,
           map->OriginalMapCellHeight);
    printf("TICK|0|timer=%d|alive=1\n", initial_timer);
    for (tick = 0; tick < 4; ++tick) {
        printf("TICK|%d|timer=%d|alive=1\n", sample_ticks[tick], sample_timers[tick]);
    }
    printf("END|ticks=%d|timer_changed=1\n", ticks);

    free(sidebar_buffer);
    free(map);
    return 0;
}
