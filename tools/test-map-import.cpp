/* Standalone native importer checks; no SDL, game window or installed-map writes.
 * clang++ -std=c++17 -O2 tools/test-map-import.cpp -o /tmp/test-map-import
 * /tmp/test-map-import [--corpus /asset-complete/CNC3D]
 */
#include "../app/map_import.h"
#include <iostream>
using namespace mapimport;
using namespace mapimport::detail;
static int checks = 0;
static void check(bool ok, const char* message) {
    ++checks; if (!ok) { std::cerr << "FAIL: " << message << "\n"; std::exit(1); }
}
static void put(const std::string& p, const std::string& s) { check(write(p, bytes(s)), "write fixture"); }
static void put(const std::string& p, const Bytes& b) { check(write(p, b), "write fixture bytes"); }
static Bytes get(const std::string& p) { Info i; Bytes b; check(read(p, b, i), "read test output"); return b; }
static std::string td(const std::string& basic = "Player=GoodGuy\n", const std::string& map = "", const std::string& tail = "") {
    return "[Basic]\nName=Fixture map\n" + basic + "\n[Map]\nTheater=TEMPERATE\nX=1\nY=1\nWidth=30\nHeight=30\n" + map + "\n[Waypoints]\n0=650\n1=1300\n" + tail;
}
static Bytes dense() { Bytes b(8192, 0); for (size_t p = 0; p < b.size(); p += 2) b[p] = 255; return b; }
static std::string b64(const Bytes& b) {
    static const char abc[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string s;
    for (size_t p = 0; p < b.size(); p += 3) {
        uint32_t n = (uint32_t)b[p] << 16;
        if (p + 1 < b.size()) n |= (uint32_t)b[p + 1] << 8;
        if (p + 2 < b.size()) n |= b[p + 2];
        s += abc[(n >> 18) & 63]; s += abc[(n >> 12) & 63];
        s += p + 1 < b.size() ? abc[(n >> 6) & 63] : '=';
        s += p + 2 < b.size() ? abc[n & 63] : '=';
    }
    return s;
}
static std::string packed(const Bytes& raw) {
    Bytes result;
    for (size_t p = 0; p < raw.size();) {
        const size_t count = std::min((size_t)8192, raw.size() - p); Bytes block;
        for (size_t q = 0; q < count;) {
            size_t repeated = 0;
            if (q >= 3) while (q + repeated < count && raw[p + q + repeated] == raw[p + q + repeated - 3]) ++repeated;
            if (repeated >= 6) {
                block.push_back(0xff); block.push_back((unsigned char)repeated); block.push_back((unsigned char)(repeated >> 8));
                block.push_back((unsigned char)(q - 3)); block.push_back((unsigned char)((q - 3) >> 8));
                q += repeated; continue;
            }
            size_t run = 1;
            while (q + run < count && raw[p + q + run] == raw[p + q]) ++run;
            if (run >= 4) {
                block.push_back(0xfe); block.push_back((unsigned char)run); block.push_back((unsigned char)(run >> 8));
                block.push_back(raw[p + q]); q += run; continue;
            }
            size_t n = std::min((size_t)63, count - q); block.push_back((unsigned char)(0x80 | n));
            block.insert(block.end(), raw.begin() + p + q, raw.begin() + p + q + n); q += n;
        }
        block.push_back(0x80);
        result.push_back((unsigned char)block.size()); result.push_back((unsigned char)(block.size() >> 8));
        result.push_back((unsigned char)count); result.push_back((unsigned char)(count >> 8));
        result.insert(result.end(), block.begin(), block.end()); p += count;
    }
    const std::string encoded = b64(result); std::string out;
    for (size_t p = 0; p < encoded.size(); p += 70) out += std::to_string(p / 70 + 1) + "=" + encoded.substr(p, 70) + "\n";
    return out;
}
static std::string ra(int format = 3, const std::string& basic = "", const std::string& tail = "", const std::string& theater = "TEMPERATE") {
    Bytes raw(128 * 128 * 3, 0);
    for (size_t c = 0; c < 128 * 128; ++c) {
        const size_t p = format >= 3 ? c * 2 : c * 3; raw[p] = raw[p + 1] = 255;
    }
    return "[Basic]\nName=RA fixture\nPlayer=Greece\nNewINIFormat=" + std::to_string(format) + "\n" + basic +
        "[Map]\nTheater=" + theater + "\nX=1\nY=1\nWidth=60\nHeight=60\n[Waypoints]\n0=1290\n1=2580\n[MapPack]\n" + packed(raw) + tail;
}
struct ProgressLog { int last = -1, n = 0; bool monotonic = true; };
static void progress(void* p, int n, const char*) {
    ProgressLog* log = (ProgressLog*)p; if (n < log->last) log->monotonic = false; log->last = n; log->n++;
}
static void cleanup(const std::string& p) {
    // This tree contains only files this test created. Never follow symlinks.
    DIR* d = opendir(p.c_str()); if (!d) return;
    while (struct dirent* e = readdir(d)) {
        const std::string name = e->d_name; if (name == "." || name == "..") continue;
        const std::string f = join(p, name); struct stat st;
        if (lstat(f.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) cleanup(f); else std::remove(f.c_str());
    }
    closedir(d); remove_dir(p);
}
int main(int argc, char** argv) {
    char folder[] = "/tmp/opencnc3d-map-import-XXXXXX";
    char* made = mkdtemp(folder); check(made != NULL, "temporary test root");
    const std::string root = folder, input = join(root, "input"), target = join(root, "td"), ratarget = join(root, "ra");
    check(mkdirs(input), "fixture folder");
    Info i;
    const std::string path = join(input, "fan.ini");
    put(path, td());
    check(!inspect(path, i) && i.message.find("matching .BIN") != std::string::npos, "missing TD companion rejected");
    put(join(input, "FAN.BiN"), dense());
    check(inspect(path, i) && !i.red_alert && !i.multiplayer && i.grid == 64, "case-insensitive TD pair; GoodGuy remains SP despite two waypoints");
    check(i.preview_width == 30 && i.preview_height == 30 && i.preview_rgba.size() == 30u * 30u * 4u, "preview exactly follows playable bounds");
    check(inspect(join(input, "FAN.BiN"), i), "BIN selection resolves INI");
    put(join(input, "fan.bin"), dense());
    bool ambiguous = false; sibling(path, ".bin", ambiguous);
    if (ambiguous) check(!inspect(path, i) && i.message.find("Several") != std::string::npos, "ambiguous paired filenames rejected");
    std::remove(join(input, "fan.bin").c_str());
    put(join(input, "FAN.BiN"), Bytes(8191));
    check(!inspect(path, i), "truncated TD terrain rejected");
    put(join(input, "FAN.BiN"), dense());
    put(path, td("Player=Multi1\n"));
    check(inspect(path, i) && i.multiplayer && i.starts == 2, "Multi player identifies MP");
    put(path, td("Player=Multi1\n", "", "2=650\n"));
    check(inspect(path, i) && i.starts == 3 && i.warnings.find("share") != std::string::npos, "duplicate slots retain native count/order with a warning");
    put(path, td("Player=Multi1\n", "", "1=650\n"));
    check(!inspect(path, i) && i.message.find("distinct") != std::string::npos, "MP still requires two distinct actual starting positions");
    put(path, td("Player=Multi1\n", "", "2=4095\n"));
    check(!inspect(path, i) && i.message.find("outside") != std::string::npos, "out-of-bounds MP starts rejected");
    put(path, td("Player=Multi1\n", "", "1=-1\n2=1300\n"));
    check(inspect(path, i) && i.starts == 2, "native compaction across unset waypoint gaps retained");
    put(path, td("Player=Multi1\nSoloMission=yes\n"));
    check(inspect(path, i) && !i.multiplayer, "explicit SoloMission wins over Player and starts");
    put(path, td("Player=Multi1\nSoloMission=yes\nOpenCNC3DKind=Multiplayer\n"));
    check(inspect(path, i) && i.multiplayer, "explicit application kind has top precedence");
    put(path, td("Player=GoodGuy\n", "Width=90\n"));
    check(!inspect(path, i), "out-of-range legacy playable width rejected");
    put(path, td("Player=GoodGuy\n", "Width=2147483647\n"));
    check(!inspect(path, i), "overflow-sized rectangle rejected");
    put(path, td("Player=GoodGuy\n", "Version=2\n"));
    check(!inspect(path, i) && i.message.find("another version") != std::string::npos, "unsupported XL version named explicitly");
    put(path, td("Player=GoodGuy\n", "Version=1\nWidth=100\nHeight=100\n"));
    put(join(input, "FAN.BiN"), Bytes());
    check(inspect(path, i) && i.grid == 128, "empty sparse 128 terrain is valid all-clear");
    put(join(input, "FAN.BiN"), Bytes{0, 64, 255, 0});
    check(!inspect(path, i), "sparse record beyond 128 grid rejected");
    put(join(input, "FAN.BiN"), Bytes{1, 0, 255});
    check(!inspect(path, i), "truncated sparse record rejected");
    put(join(input, "FAN.BiN"), Bytes{1, 0, 255, 0, 1, 0, 255, 0});
    check(!inspect(path, i), "duplicate sparse record rejected");
    const std::string original = "\xef\xbb\xbf" + td("Player=GoodGuy\n", "", "\n; scripted custom data is byte-preserved\n[CustomRules]\nCost = 123 ; keep comment\n[Triggers]\nwin=1,2,3,4\n[Digest]\n1=stale-generated-checksum\n");
    put(path, original); put(join(input, "FAN.BiN"), dense());
    ProgressLog log;
    check(run(path, target, ratarget, i, progress, &log), "native TD import including autoheight");
    check(log.monotonic && log.last == 100 && log.n >= 5, "progress reaches 100 monotonically");
    const std::string first = i.scenario, prefix = join(target, first);
    check(first.size() == 8 && first.substr(0, 3) == "IMP", "safe native scenario ID");
    check(get(prefix + ".source.ini") == bytes(original), "source INI including BOM retained byte-for-byte");
    const std::string working = text(get(prefix + ".INI"));
    check(working.compare(0, 3, "\xef\xbb\xbf") != 0 && working.find("[Basic]") == 0, "playable INI BOM removed for Westwood parser");
    check(working.find("[CustomRules]\nCost = 123 ; keep comment\n[Triggers]\nwin=1,2,3,4\n") != std::string::npos, "custom rules/scripts preserved exactly");
    check(working.find("OpenCNC3DKind=Singleplayer") != std::string::npos, "runtime classification metadata persisted");
    check(working.find("[Digest]") == std::string::npos && working.find("stale-generated-checksum") == std::string::npos, "stale original Digest omitted only from changed playable copy");
    check(get(prefix + ".BIN") == dense(), "binary preserved byte-for-byte");
    check(get(prefix + ".HGT").size() == 65u * 65u, "complete generated TD corners");
    check(regular(prefix + ".preview.ppm") && regular(prefix + ".import.txt"), "preview and receipt saved");
    check(run(path, target, ratarget, i) && i.scenario == first && i.message.find("Already") == 0, "repeat import reuses identical existing map");
    const std::string secondtarget = join(root, "other-computer");
    check(mkdirs(secondtarget), "other computer fixture");
    put(join(secondtarget, "IMPAAAAA.INI"), "unrelated older map");
    check(run(path, secondtarget, ratarget, i) && i.scenario == first, "content ID independent of existing maps and import order");
    const std::string collisiontarget = join(root, "collision"); check(mkdirs(collisiontarget), "collision fixture");
    put(join(collisiontarget, first + ".INI"), "existing unrelated bytes");
    check(run(path, collisiontarget, ratarget, i) && i.scenario != first, "real content-name collision resolved");
    check(text(get(join(collisiontarget, first + ".INI"))) == "existing unrelated bytes", "collision never overwrites original");
    put(join(input, "fan.hgt"), Bytes(65 * 65, 73));
    check(run(path, target, ratarget, i) && get(join(target, i.scenario + ".HGT")) == Bytes(65 * 65, 73), "valid provided heightmap preserved");
    put(join(input, "fan.hgt"), Bytes(20));
    check(inspect(path, i) && i.warnings.find("wrong size") != std::string::npos, "invalid provided heightmap announces regeneration");
    check(run(path, target, ratarget, i), "invalid provided heightmap safely regenerated");
    check(get(join(target, i.scenario + ".HGT")).size() == 65u * 65u &&
          get(join(target, i.scenario + ".source.hgt")) == Bytes(20), "invalid original heightmap still archived exactly");
    std::remove(join(input, "fan.hgt").c_str());
    const std::string rapath = join(input, "snow.mpr"); put(rapath, ra());
    check(inspect(rapath, i) && i.red_alert && i.multiplayer && i.grid == 128, "RA format 3 inferred multiplayer from un-scripted starts");
    for (int f = 0; f <= 2; ++f) { put(rapath, ra(f, "SoloMission=yes\n")); check(inspect(rapath, i) && !i.multiplayer, "RA older interleaved formats and explicit SP"); }
    put(rapath, ra(3, "", "[Trigs]\nwin=0,0,0\n"));
    check(inspect(rapath, i) && !i.multiplayer, "scripted RA map remains SP despite starts");
    put(rapath, ra(3, "SoloMission=no\n", "[Trigs]\ntrigger=0,0,0\n"));
    check(inspect(rapath, i) && i.multiplayer, "explicit RA MP retains custom triggers");
    const std::string scm = join(input, "SCM01EA.INI");
    put(scm, ra(3, "", "[Briefing]\n1=Multiplayer scenario instructions\n[Trigs]\ntrigger=0,0,0\n"));
    check(inspect(scm, i) && i.multiplayer, "native SCM convention outranks generic briefing and scripting clues");
    put(scm, ra(3, "SoloMission=yes\n", "[Briefing]\n1=Singleplayer instructions\n"));
    check(inspect(scm, i) && !i.multiplayer, "explicit solo declaration still overrides SCM convention");
    put(rapath, ra(3, "SoloMission=yes\n", "", "INTERIOR"));
    check(run(rapath, target, ratarget, i), "RA interior import");
    check(get(join(ratarget, i.scenario + ".HGT")) == Bytes(129 * 129, 0), "RA interior is exactly flat");
    put(rapath, ra()); check(run(rapath, target, ratarget, i), "RA outdoor autoheight import");
    check(get(join(ratarget, i.scenario + ".HGT")).size() == 129u * 129u, "RA outdoor complete corner grid");
    std::string broken = ra(); broken.resize(broken.size() - 80); put(rapath, broken);
    check(!inspect(rapath, i), "truncated RA MapPack refused");
    Bytes bomb{4,0,0,32,0xff,0xff,0xff,0xff};
    put(rapath, "[Basic]\nNewINIFormat=3\n[Map]\nTheater=SNOW\nX=1\nY=1\nWidth=30\nHeight=30\n[MapPack]\n1=" + b64(bomb));
    check(!inspect(rapath, i), "invalid LCW references refused without out-of-bounds reads");
    put(rapath, "[Basic]\n[Map]\n[IsoMapPack5]\n1=abcd\n");
    check(!inspect(rapath, i) && i.message.find("later-game") != std::string::npos, "later-game map refused clearly");
    const std::string blocked = join(root, "blocked"); put(blocked, "not a directory");
    check(!run(path, blocked, ratarget, i), "unwritable destination reports failure");
    check(text(get(blocked)) == "not a directory", "failed import leaves destination file intact");
    if (argc == 3 && std::string(argv[1]) == "--corpus") {
        int maps = 0, rejected = 0, td_multi = 0, ra_multi = 0;
        for (const char* sub : {"playable/missions", "playable/ra3d/missions"}) {
            const std::string corpus = join(argv[2], sub); DIR* d = opendir(corpus.c_str());
            check(d != NULL, "open installed read-only corpus");
            while (struct dirent* e = readdir(d)) {
                if (ext(e->d_name) != ".ini") continue;
                ++maps; const std::string source = join(corpus, e->d_name);
                const bool accepted = inspect(source, i);
                if (!accepted) { ++rejected; std::cerr << "CORPUS_REJECT " << sub << "/" << e->d_name << " | " << i.message << "\n"; }
                const std::string code = upper(stem(e->d_name));
                if (code.size() == 7 && code.substr(0, 3) == "SCM" && code.substr(5) == "EA") {
                    int number = 0; const bool is_ra = std::string(sub) == "playable/ra3d/missions";
                    if (integer(code.substr(3, 2), number) && number >= 1 && number <= (is_ra ? 24 : 9)) {
                        check(accepted && i.multiplayer, "every original skirmish map is classified multiplayer");
                        if (is_ra) ++ra_multi; else ++td_multi;
                    }
                }
            }
            closedir(d);
        }
        check(td_multi == 9 && ra_multi == 24, "full original TD9 / RA24 multiplayer corpus present and correctly classified");
        std::cout << "CORPUS maps=" << maps << " rejected=" << rejected << " TD_multiplayer=" << td_multi << " RA_multiplayer=" << ra_multi << "\n";
    }
    cleanup(root);
    std::cout << "PASS " << checks << " native importer checks\n";
}
