/* Native, bounded import of original Tiberian Dawn and Red Alert maps.
 * Source files are read only. INI is published last, so catalog scans never see a
 * partially imported map. Original bytes and a receipt remain beside the import.
 * The height solver is the editor's; RA's adapter is frozen from ra_heights.py.
 */
#pragma once
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <map>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <dirent.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif
#include "../game/autoheight_mod.h"
#include "map_import_ra_tables.h"

namespace mapimport {
struct ImportedMap {
    std::string name, scenario;
    bool red_alert = false, multiplayer = false;
};
struct Info {
    std::string name, game, kind, theater, message, scenario, warnings;
    int width = 0, height = 0, starts = 0, grid = 0;
    bool red_alert = false, multiplayer = false;
    bool recovered = false;
    int preview_width = 0, preview_height = 0;
    std::vector<unsigned char> preview_rgba;
    int bundle_count = 0;
    std::vector<ImportedMap> imported_maps;
};
typedef void (*Progress)(void*, int, const char*);
namespace detail {
typedef std::vector<unsigned char> Bytes;
typedef std::map<std::string, std::string> Section;
typedef std::map<std::string, Section> Ini;
static const size_t MAP_IMPORT_INPUT_LIMIT = 4u * 1024u * 1024u;
#ifdef _WIN32
inline std::wstring wide(const std::string& s) {
    const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.c_str(), -1, NULL, 0);
    if (n < 1) return L"";
    std::vector<wchar_t> w((size_t)n);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.c_str(), -1, w.data(), n);
    return w.data();
}
inline std::string utf8(const wchar_t* w) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if (n < 1) return "";
    std::vector<char> s((size_t)n);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, NULL, NULL);
    return s.data();
}
typedef struct _stat64 FileStat;
inline int file_stat(const std::string& p, FileStat* st) { return _wstat64(wide(p).c_str(), st); }
inline FILE* file_open(const std::string& p, const char* mode) { return _wfopen(wide(p).c_str(), wide(mode).c_str()); }
inline int file_remove(const std::string& p) { return _wremove(wide(p).c_str()); }
#else
typedef struct stat FileStat;
inline int file_stat(const std::string& p, FileStat* st) { return stat(p.c_str(), st); }
inline FILE* file_open(const std::string& p, const char* mode) { return fopen(p.c_str(), mode); }
inline int file_remove(const std::string& p) { return std::remove(p.c_str()); }
#endif
inline std::string lower(std::string s) {
    for (size_t i = 0; i < s.size(); ++i) s[i] = (char)std::tolower((unsigned char)s[i]);
    return s;
}
inline std::string upper(std::string s) {
    for (size_t i = 0; i < s.size(); ++i) s[i] = (char)std::toupper((unsigned char)s[i]);
    return s;
}
inline std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}
inline std::string leaf(const std::string& p) {
    const size_t k = p.find_last_of("/\\"); return k == std::string::npos ? p : p.substr(k + 1);
}
inline std::string parent(const std::string& p) {
    const size_t k = p.find_last_of("/\\"); return k == std::string::npos ? "." : p.substr(0, k + 1);
}
inline std::string stem(const std::string& p) {
    std::string b = leaf(p); const size_t k = b.find_last_of('.'); return k == std::string::npos ? b : b.substr(0, k);
}
inline std::string ext(const std::string& p) {
    const std::string b = leaf(p); const size_t k = b.find_last_of('.'); return k == std::string::npos ? "" : lower(b.substr(k));
}
inline std::string join(const std::string& a, const std::string& b) {
    return a.empty() || a.back() == '/' || a.back() == '\\' ? a + b : a + "/" + b;
}
inline bool exists(const std::string& p) { FileStat st; return file_stat(p, &st) == 0; }
inline bool regular(const std::string& p) { FileStat st; return file_stat(p, &st) == 0 && S_ISREG(st.st_mode); }
inline bool directory(const std::string& p) { FileStat st; return file_stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
inline bool list_dir(const std::string& path, std::vector<std::string>& names) {
#ifdef _WIN32
    WIN32_FIND_DATAW data;
    HANDLE h = FindFirstFileW(wide(join(path, "*")).c_str(), &data);
    if (h == INVALID_HANDLE_VALUE) return false;
    do { names.push_back(utf8(data.cFileName)); } while (FindNextFileW(h, &data));
    FindClose(h); return true;
#else
    DIR* d = opendir(path.c_str()); if (!d) return false;
    while (struct dirent* e = readdir(d)) names.push_back(e->d_name);
    closedir(d); return true;
#endif
}
inline int make_dir(const std::string& p) {
#ifdef _WIN32
    return _wmkdir(wide(p).c_str());
#else
    return mkdir(p.c_str(), 0700);
#endif
}
inline bool mkdirs(const std::string& p) {
    if (directory(p)) return true;
    std::string q = p; while (q.size() > 1 && (q.back() == '/' || q.back() == '\\')) q.pop_back();
    const std::string up = parent(q);
    if (up != q && up != "." && !directory(up) && !mkdirs(up)) return false;
    return make_dir(q) == 0 || directory(q);
}
inline void remove_dir(const std::string& p) {
#ifdef _WIN32
    _wrmdir(wide(p).c_str());
#else
    rmdir(p.c_str());
#endif
}
inline bool fail(Info& out, const std::string& s) { out.message = s; return false; }
inline bool read(const std::string& p, Bytes& bytes, Info& out) {
    FileStat st;
    if (file_stat(p, &st) != 0 || !S_ISREG(st.st_mode)) return fail(out, "Cannot read map file: " + leaf(p));
    if (st.st_size < 0 || (uint64_t)st.st_size > MAP_IMPORT_INPUT_LIMIT) return fail(out, "Map file is larger than the supported 4 MB limit.");
    FILE* f = file_open(p, "rb");
    if (!f) return fail(out, "Cannot open map file: " + leaf(p));
    bytes.resize((size_t)st.st_size);
    const size_t n = bytes.empty() ? 0 : fread(bytes.data(), 1, bytes.size(), f);
    const bool ok = n == bytes.size() && !ferror(f); fclose(f);
    return ok || fail(out, "Map file changed or could not be read completely.");
}
inline bool write(const std::string& p, const Bytes& bytes) {
    FILE* f = file_open(p, "wb"); if (!f) return false;
    bool ok = bytes.empty() || fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    if (fclose(f) != 0) ok = false; return ok;
}
inline Bytes bytes(const std::string& s) { return Bytes(s.begin(), s.end()); }
inline std::string text(const Bytes& b) { return std::string(b.begin(), b.end()); }
inline bool integer(const std::string& s, int& n) {
    std::string v = trim(s); if (v.empty()) return false;
    char* end = NULL; errno = 0; const long z = strtol(v.c_str(), &end, 10);
    if (errno || !end || *end || z < INT_MIN || z > INT_MAX) return false;
    n = (int)z; return true;
}
inline std::string value(const Ini& ini, const char* sec, const char* key) {
    Ini::const_iterator s = ini.find(sec); if (s == ini.end()) return "";
    Section::const_iterator k = s->second.find(key); return k == s->second.end() ? "" : k->second;
}
inline bool number(const Ini& ini, const char* sec, const char* key, int fallback, int& n) {
    std::string s = value(ini, sec, key); if (s.empty()) { n = fallback; return true; } return integer(s, n);
}
inline int boolean(const std::string& s) {
    const std::string x = lower(trim(s));
    if (x == "yes" || x == "true" || x == "1") return 1;
    if (x == "no" || x == "false" || x == "0") return 0;
    return -1;
}
inline bool parse(const Bytes& b, Ini& ini, Info& out) {
    if (b.empty() || std::find(b.begin(), b.end(), 0) != b.end()) return fail(out, "This is not a text C&C scenario file.");
    std::string src = text(b), sec;
    size_t pos = src.compare(0, 3, "\xef\xbb\xbf") == 0 ? 3 : 0;
    while (pos < src.size()) {
        size_t end = src.find('\n', pos); if (end == std::string::npos) end = src.size();
        std::string line = trim(src.substr(pos, end - pos)); pos = end + 1;
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[') {
            const size_t close = line.find(']');
            if (close == std::string::npos) return fail(out, "The scenario contains an incomplete section heading.");
            sec = lower(trim(line.substr(1, close - 1))); ini[sec]; continue;
        }
        const size_t eq = line.find('='); if (eq == std::string::npos || sec.empty()) continue;
        std::string v = trim(line.substr(eq + 1));
        // Semicolons introduce INI comments. Compressed sections never use them.
        const size_t comment = v.find(';'); if (comment != std::string::npos) v = trim(v.substr(0, comment));
        ini[sec][lower(trim(line.substr(0, eq)))] = v;
    }
    if (!ini.count("map") || !ini.count("basic")) return fail(out, "A map needs both [Basic] and [Map] sections.");
    return true;
}
// Resolve paired files on case-sensitive filesystems without changing source bytes.
inline std::string sibling(const std::string& p, const std::string& suffix, bool& ambiguous) {
    const std::string wanted = lower(stem(p) + suffix); std::string result; ambiguous = false;
    std::vector<std::string> names; if (!list_dir(parent(p), names)) return result;
    for (const std::string& name : names) {
        if (lower(name) != wanted) continue;
        const std::string candidate = join(parent(p), name); if (!regular(candidate)) continue;
        if (!result.empty()) ambiguous = true; else result = candidate;
    }
    return result;
}
inline uint16_t u16(const Bytes& b, size_t p) { return (uint16_t)(b[p] | ((unsigned)b[p + 1] << 8)); }
inline uint32_t u32(const Bytes& b, size_t p) { return (uint32_t)u16(b, p) | ((uint32_t)u16(b, p + 2) << 16); }
inline uint64_t hash(const Bytes& b) { uint64_t h = UINT64_C(14695981039346656037); for (unsigned char c : b) h = (h ^ c) * UINT64_C(1099511628211); return h; }
inline std::string digest(const Bytes& b) { char s[24]; snprintf(s, sizeof s, "%016llx", (unsigned long long)hash(b)); return s; }
inline bool base64(const std::string& s, Bytes& out) {
    int bits = 0; uint32_t acc = 0; bool padded = false; int pad = 0, digits = 0;
    for (unsigned char c : s) {
        if (std::isspace(c)) continue;
        if (c == '=') { padded = true; if (++pad > 2) return false; continue; }
        if (padded) return false;
        int x = c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 : c >= '0' && c <= '9' ? c - '0' + 52 : c == '+' ? 62 : c == '/' ? 63 : -1;
        if (x < 0) return false;
        acc = (acc << 6) | (unsigned)x; bits += 6; digits++;
        if (bits >= 8) { bits -= 8; out.push_back((unsigned char)(acc >> bits)); }
    }
    return digits > 0 && digits % 4 != 1 && (!pad || (digits + pad) % 4 == 0) && (!bits || (acc & ((1u << bits) - 1)) == 0);
}
// LCW (Format80), with source, target and back-reference bounds checked before every copy.
inline bool lcw(const Bytes& in, size_t begin, size_t end, size_t expected, Bytes& out) {
    const size_t start = out.size(); size_t p = begin;
    while (p < end && out.size() - start < expected) {
        const unsigned op = in[p++]; size_t count = 0, from = 0;
        if (!(op & 0x80)) {
            if (p >= end) return false;
            count = (op >> 4) + 3; const size_t distance = ((op & 15) << 8) | in[p++];
            if (!distance || distance > out.size() - start) return false;
            from = out.size() - distance;
        } else if (!(op & 0x40)) {
            count = op & 63; if (!count) return out.size() - start == expected;
            if (count > end - p || count > expected - (out.size() - start)) return false;
            out.insert(out.end(), in.begin() + p, in.begin() + p + count); p += count; continue;
        } else if (op == 0xfe) {
            if (end - p < 3) return false;
            count = u16(in, p); p += 2;
            if (!count || count > expected - (out.size() - start)) return false;
            out.insert(out.end(), count, in[p++]); continue;
        } else {
            if (op == 0xff) { if (end - p < 4) return false; count = u16(in, p); p += 2; }
            else { if (end - p < 2) return false; count = (op & 63) + 3; }
            from = start + u16(in, p); p += 2;
            if (from >= out.size()) return false;
        }
        if (!count || count > expected - (out.size() - start)) return false;
        for (size_t k = 0; k < count; ++k) out.push_back(out[from + k]);
    }
    return out.size() - start == expected && p < end && in[p] == 0x80 && p + 1 == end;
}
inline bool unpack(const Section& sec, size_t expected, Bytes& out) {
    std::map<int, std::string> lines;
    for (const auto& kv : sec) { int n; if (!integer(kv.first, n) || n < 1 || lines.count(n)) return false; lines[n] = kv.second; }
    std::string encoded; int last = 0;
    for (const auto& kv : lines) { if (kv.first != ++last) return false; encoded += kv.second; }
    Bytes compressed; if (!base64(encoded, compressed) || compressed.size() > 32000) return false;
    size_t p = 0;
    while (p < compressed.size()) {
        if (compressed.size() - p < 4) return false;
        const size_t n = u16(compressed, p), raw = u16(compressed, p + 2); p += 4;
        // These are also the native LCWStraw / scenario staging limits; accepting
        // a bigger block here would hand unsafe input to the original game reader.
        if (!n || !raw || n > 8257 || raw > 8192 || n > compressed.size() - p || out.size() > expected || raw > expected - out.size()) return false;
        if (!lcw(compressed, p, p + n, raw, out)) return false;
        p += n;
    }
    return out.size() == expected;
}
} } // recovery reader uses the bounded file helpers above
#include "map_import_json.h"
namespace mapimport { namespace detail {
struct Map {
    Ini ini;
    Bytes source, bin, hgt, source_hgt, tmpl, icon, land, colours, source_json;
    std::string path, bin_path, hgt_path;
    std::string invalid_start;
    bool duplicate_starts = false;
    int x = 0, y = 0, version = 0, stride = 64;
    int ignored_border_tiles = 0;
    std::vector<int> start;
    std::vector<std::vector<std::pair<int, int>>> foot;
};
inline const MapImportRaTile* ra_table(const Info& out, size_t& n) {
    if (out.theater == "SNOW") { n = sizeof(MAP_IMPORT_RASNOW) / sizeof(MAP_IMPORT_RASNOW[0]); return MAP_IMPORT_RASNOW; }
    if (out.theater == "INTERIOR") { n = sizeof(MAP_IMPORT_RAINT) / sizeof(MAP_IMPORT_RAINT[0]); return MAP_IMPORT_RAINT; }
    n = sizeof(MAP_IMPORT_RATEMP) / sizeof(MAP_IMPORT_RATEMP[0]); return MAP_IMPORT_RATEMP;
}
inline bool rectangle(Map& m, Info& out) {
    if (!number(m.ini, "map", "x", 1, m.x) || !number(m.ini, "map", "y", 1, m.y) ||
        !number(m.ini, "map", "width", 0, out.width) || !number(m.ini, "map", "height", 0, out.height))
        return fail(out, "The map has invalid playable bounds.");
    if (m.x < 0 || m.y < 0 || out.width < 1 || out.height < 1 || m.x >= out.grid || m.y >= out.grid ||
        out.width > out.grid - m.x || out.height > out.grid - m.y)
        return fail(out, "The playable rectangle lies outside the map's " + std::to_string(out.grid) + " x " + std::to_string(out.grid) + " grid.");
    return true;
}
inline bool terrain(Map& m, Info& out) {
    const size_t cells = (size_t)out.grid * out.grid;
    m.tmpl.assign(cells, 255); m.icon.assign(cells, 0); m.land.assign(cells, 0); m.colours.resize(cells * 3);
    if (out.red_alert) {
        Bytes raw;
        if (!unpack(m.ini["mappack"], cells * 3, raw)) return fail(out, "Red Alert MapPack is incomplete or has invalid compressed terrain.");
        if (m.ini.count("overlaypack")) { Bytes overlay; if (!unpack(m.ini["overlaypack"], cells, overlay)) return fail(out, "Red Alert OverlayPack is incomplete or invalid."); }
        int format;
        if (!number(m.ini, "basic", "newiniformat", 0, format) || format < 0 || format > 3) return fail(out, "This Red Alert NewINIFormat is not supported yet.");
        size_t nt; const MapImportRaTile* table = ra_table(out, nt);
        std::map<unsigned, const MapImportRaTile*> lookup;
        for (size_t i = 0; i < nt; ++i) lookup[(unsigned)table[i].t * 256u + table[i].i] = &table[i];
        for (size_t c = 0; c < cells; ++c) {
            unsigned t = u16(raw, format >= 3 ? c * 2 : c * 3);
            unsigned i = raw[format >= 3 ? cells * 2 + c : c * 3 + 2];
            // The native engine deliberately treats 255 as clear despite its
            // enum slot also naming ARRO0003 (cell.cpp Get_Template_Info).
            if (t == 0xffff || t == 255 || t == 0) { t = 0; i = (unsigned)((c % out.grid & 3) | ((c / out.grid & 3) << 2)); }
            auto found = lookup.find(t * 256 + i);
            if (found == lookup.end()) {
                const int x = (int)c % out.grid, y = (int)c / out.grid;
                if (x >= m.x && y >= m.y && x < m.x + out.width && y < m.y + out.height)
                    return fail(out, "The Red Alert map uses a tile missing from its original theater: " + std::to_string(t) + "/" + std::to_string(i) + ". Custom theater support is not installed.");
                // Original Soviet 6 has stale bridge icons beyond its playable
                // rectangle. Keep the native MapPack byte-for-byte; only the
                // non-playable height adapter uses clear ground for these cells.
                ++m.ignored_border_tiles;
                found = lookup.find((unsigned)((x & 3) | ((y & 3) << 2)));
                if (found == lookup.end()) return fail(out, "The original theater has no clear-ground adapter.");
            }
            const MapImportRaTile& z = *found->second;
            m.tmpl[c] = z.ht; m.icon[c] = z.hi; m.land[c] = z.land;
            m.colours[c * 3] = z.r; m.colours[c * 3 + 1] = z.g; m.colours[c * 3 + 2] = z.b;
        }
    } else {
        if (m.version == 0) {
            if (m.bin.size() != 8192) return fail(out, "Original C&C maps need an 8192-byte .BIN. Extended maps must declare their format.");
            for (size_t c = 0; c < cells; ++c) { m.tmpl[c] = m.bin[c * 2]; m.icon[c] = m.bin[c * 2 + 1]; }
        } else if (m.version == 1) {
            if (m.bin.size() % 4) return fail(out, "The 128-cell C&C map has a truncated sparse .BIN record.");
            int last = -1;
            for (size_t p = 0; p < m.bin.size(); p += 4) {
                int c = u16(m.bin, p);
                if (c <= last || c >= (int)cells) return fail(out, "The 128-cell C&C map has an out-of-range or unordered .BIN record.");
                last = c; m.tmpl[c] = m.bin[p + 2]; m.icon[c] = m.bin[p + 3];
            }
        } else return fail(out, "This C&C map version is not supported by the importer yet (supported: original 64 and sparse 128).");
        for (size_t c = 0; c < cells; ++c) {
            const int t = m.tmpl[c], i = m.icon[c];
            if (t != 255 && (t >= EDIT_TEMPLATE_COUNT || (t != 0 && i >= EDIT_TEMPLATES[t].w * EDIT_TEMPLATES[t].h)))
                return fail(out, "The C&C map contains an invalid terrain template or icon.");
            const int land = t == 255 ? EDIT_LAND_CLEAR : edit_land_of(t, i);
            m.land[c] = land == EDIT_LAND_WATER ? 1 : land == EDIT_LAND_ROCK ? 2 : land == EDIT_LAND_ROAD ? 3 : land == EDIT_LAND_BEACH ? 4 : 0;
            // Terrain overview: consistent land colours, not an invented rendered screenshot.
            int r = out.theater == "DESERT" ? 171 : out.theater == "SNOW" ? 196 : 112;
            int g = out.theater == "DESERT" ? 149 : out.theater == "SNOW" ? 202 : 126;
            int b = out.theater == "DESERT" ? 104 : out.theater == "SNOW" ? 204 : 82;
            if (m.land[c] == 1) { r = 42; g = 79; b = 114; }
            else if (m.land[c] == 2) { r = 108; g = 98; b = 79; }
            else if (m.land[c] == 3) { r = 136; g = 117; b = 88; }
            else if (m.land[c] == 4) { r = 156; g = 145; b = 109; }
            m.colours[c * 3] = (unsigned char)r; m.colours[c * 3 + 1] = (unsigned char)g; m.colours[c * 3 + 2] = (unsigned char)b;
        }
    }
    return true;
}
inline std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> v; size_t p = 0;
    for (;;) { size_t e = s.find(',', p); v.push_back(trim(s.substr(p, e == std::string::npos ? e : e - p))); if (e == std::string::npos) return v; p = e + 1; }
}
inline void objects(Map& m, Info& out) {
    for (int k = 0; k < 8; ++k) {
        const std::string raw = value(m.ini, "waypoints", std::to_string(k).c_str());
        int cell;
        if (raw.empty()) continue;
        if (!integer(raw, cell)) { m.invalid_start = "Player start " + std::to_string(k) + " is not a cell number."; continue; }
        if (cell == -1) continue;
        if (cell < 0) { m.invalid_start = "Player start " + std::to_string(k) + " has an invalid negative cell."; continue; }
        int x = cell % m.stride, y = cell / m.stride;
        if (x < m.x || y < m.y || x >= m.x + out.width || y >= m.y + out.height) { m.invalid_start = "Player start " + std::to_string(k) + " lies outside the playable map."; continue; }
        // Preserve native slot order/count even when two slots share a cell:
        // the original RA SCM03EA itself contains this. Never silently renumber.
        if (std::find(m.start.begin(), m.start.end(), cell) != m.start.end()) m.duplicate_starts = true;
        m.start.push_back(cell);
    }
    out.starts = (int)m.start.size();
    for (const auto& kv : m.ini["structures"]) {
        const auto f = split(kv.second); int cell;
        if (f.size() < 4 || !integer(f[3], cell) || cell < 0) continue;
        int x = cell % m.stride, y = cell / m.stride;
        if (x >= out.grid || y >= out.grid) continue;
        const std::string code = upper(f[1]); std::vector<std::pair<int, int>> foot;
        if (out.red_alert) {
            for (const MapImportRaFoot& a : MAP_IMPORT_RA_FOOT) if (code == a.name)
                for (int j = 0; j < a.n; ++j) foot.push_back({x + a.dx[j], y + a.dy[j]});
        } else {
            for (const EditItem& a : EDIT_ITEMS) if (a.kind == EDIT_KIND_BUILDING && code == a.code)
                for (int j = 0; j < a.n; ++j) foot.push_back({x + a.dx[j], y + a.dy[j]});
        }
        if (foot.empty()) foot.push_back({x, y}); m.foot.push_back(foot);
    }
}
inline bool classify(Map& m, Info& out) {
    int multi = -1;
    std::string kind = lower(value(m.ini, "basic", "opencnc3dkind"));
    if (kind.empty()) kind = lower(value(m.ini, "basic", "cnc3dkind"));
    if (kind == "multiplayer" || kind == "multi" || kind == "skirmish") multi = 1;
    else if (kind == "singleplayer" || kind == "single" || kind == "mission") multi = 0;
    if (multi < 0) { const int solo = boolean(value(m.ini, "basic", "solomission")); if (solo >= 0) multi = !solo; }
    if (multi < 0) multi = boolean(value(m.ini, "basic", "multiplayeronly"));
    if (multi < 0) multi = boolean(value(m.ini, "basic", "multiplayer"));
    const std::string player = lower(value(m.ini, "basic", "player"));
    if (multi < 0 && player.size() == 6 && player.compare(0, 5, "multi") == 0 && player[5] >= '1' && player[5] <= '8') multi = 1;
    if (multi < 0 && !out.red_alert && (player == "goodguy" || player == "badguy")) multi = 0;
    // Original RA SCM01 and SCM05..12 have generic briefing text and a country
    // Player value. SCM identifies a multiplayer scenario before those weak
    // scripting clues; explicit type flags and TD Player still take precedence.
    if (multi < 0 && lower(stem(m.path)).compare(0, 3, "scm") == 0) multi = 1;
    const bool scripted = !m.ini["triggers"].empty() || !m.ini["trigs"].empty() || !m.ini["briefing"].empty();
    if (multi < 0 && scripted) multi = 0;
    // RA editors default Player=Greece even for multiplayer, so a country alone is not proof.
    if (multi < 0) multi = out.starts >= 2 ? 1 : 0;
    out.multiplayer = multi != 0; out.kind = out.multiplayer ? "Multiplayer" : "Singleplayer";
    if (out.multiplayer && !m.invalid_start.empty()) return fail(out, m.invalid_start + " Fix the multiplayer waypoints before importing.");
    std::vector<int> distinct = m.start; std::sort(distinct.begin(), distinct.end());
    distinct.erase(std::unique(distinct.begin(), distinct.end()), distinct.end());
    if (out.multiplayer && distinct.size() < 2) return fail(out, "This multiplayer map needs at least two distinct valid player starts.");
    if (out.multiplayer && m.duplicate_starts) out.warnings = "Some player start slots share a position; original native slot order and behavior are preserved.";
    return true;
}
inline void preview(const Map& m, Info& out) {
    const int scale = std::max(1, (std::max(out.width, out.height) + 127) / 128);
    out.preview_width = (out.width + scale - 1) / scale; out.preview_height = (out.height + scale - 1) / scale;
    out.preview_rgba.assign((size_t)out.preview_width * out.preview_height * 4, 255);
    for (int py = 0; py < out.preview_height; ++py) for (int px = 0; px < out.preview_width; ++px) {
        const size_t c = (size_t)(m.y + std::min(out.height - 1, py * scale)) * out.grid + m.x + std::min(out.width - 1, px * scale);
        const size_t p = ((size_t)py * out.preview_width + px) * 4;
        for (int k = 0; k < 3; ++k) out.preview_rgba[p + k] = m.colours[c * 3 + k];
    }
    if (out.multiplayer) for (int cell : m.start) {
        const int x = (cell % m.stride - m.x) / scale, y = (cell / m.stride - m.y) / scale;
        for (int yy = std::max(0, y - 1); yy <= std::min(out.preview_height - 1, y + 1); ++yy)
            for (int xx = std::max(0, x - 1); xx <= std::min(out.preview_width - 1, x + 1); ++xx) {
                size_t p = ((size_t)yy * out.preview_width + xx) * 4;
                out.preview_rgba[p] = 250; out.preview_rgba[p + 1] = 218; out.preview_rgba[p + 2] = 68;
            }
    }
}
inline bool load(const std::string& source, Map& m, Info& out) {
    out = Info(); m.path = source;
    if (ext(source) == ".bin") {
        bool ambiguous; m.path = sibling(source, ".ini", ambiguous);
        if (ambiguous) return fail(out, "Several case-variant INI files match this terrain.");
        if (m.path.empty()) {
            if (!recover_json(source, m.source, m.source_json, out)) return false;
            m.path = join(parent(source), stem(source) + ".ini");
        }
    }
    const std::string e = ext(m.path);
    if (e != ".ini" && e != ".mpr" && e != ".map") return fail(out, "Choose an extracted C&C .INI/.BIN pair or Red Alert .INI/.MPR/.MAP scenario.");
    if (m.source.empty() && !read(m.path, m.source, out)) return false;
    if (!parse(m.source, m.ini, out)) return false;
    if (m.ini.count("isomappack5") || m.ini.count("isomappack") || m.ini.count("tileset"))
        return fail(out, "This is a later-game map. This importer currently supports Tiberian Dawn and Red Alert 1.");
    out.red_alert = m.ini.count("mappack") != 0;
    out.game = out.red_alert ? "Red Alert" : "Tiberian Dawn";
    out.name = value(m.ini, "basic", "name"); if (out.name.empty() || lower(out.name) == "<none>") out.name = stem(m.path);
    out.theater = upper(value(m.ini, "map", "opencnc3dtheater"));
    if (out.theater.empty()) out.theater = upper(value(m.ini, "map", "cnc3dtheater"));
    if (out.theater.empty()) out.theater = upper(value(m.ini, "map", "theater"));
    if (out.theater == "TEMPERAT") out.theater = "TEMPERATE";
    if (out.red_alert) {
        if (out.theater != "TEMPERATE" && out.theater != "SNOW" && out.theater != "INTERIOR") return fail(out, "Unsupported Red Alert theater: " + out.theater);
        out.grid = m.stride = 128;
    } else {
        if (out.theater != "TEMPERATE" && out.theater != "DESERT" && out.theater != "WINTER" && out.theater != "SNOW" && out.theater != "SAND") return fail(out, "Unsupported C&C theater: " + out.theater);
        if (!number(m.ini, "map", "version", 0, m.version) || m.version < 0 || m.version > 1) return fail(out, "Supported C&C terrain formats are original 64 and Version=1 sparse 128; this map declares another version.");
        out.grid = m.stride = m.version == 1 ? 128 : 64;
        bool ambiguous; m.bin_path = sibling(m.path, ".bin", ambiguous);
        if (ambiguous) return fail(out, "Several case-variant .BIN files match this map; keep one unambiguous pair.");
        if (m.bin_path.empty()) return fail(out, "This C&C map needs its matching .BIN terrain file in the same folder.");
        if (!read(m.bin_path, m.bin, out)) return false;
    }
    if (!rectangle(m, out) || !terrain(m, out)) return false;
    objects(m, out); if (!classify(m, out)) return false;
    bool ambiguous; m.hgt_path = sibling(m.path, ".hgt", ambiguous);
    if (ambiguous) return fail(out, "Several case-variant .HGT files match this map.");
    if (!m.hgt_path.empty()) {
        if (!read(m.hgt_path, m.source_hgt, out)) return false;
        if (m.source_hgt.size() == (size_t)(out.grid + 1) * (out.grid + 1)) m.hgt = m.source_hgt;
        else {
            if (!out.warnings.empty()) out.warnings += " ";
            out.warnings += "The supplied heightmap has the wrong size; a new one will be generated and the original kept.";
        }
    }
    if (m.ignored_border_tiles) {
        if (!out.warnings.empty()) out.warnings += " ";
        out.warnings += std::to_string(m.ignored_border_tiles) + " unused border tiles use clear ground for height generation; original terrain is preserved.";
    }
    preview(m, out);
    out.message = out.game + " / " + out.kind + " / " + std::to_string(out.width) + " x " + std::to_string(out.height);
    if (out.recovered) out.message = "Missing INI: terrain-only multiplayer recovery. Objects, resources and mission scripts are unavailable.";
    else if (!out.warnings.empty()) out.message += ". " + out.warnings;
    return true;
}
inline void flatten_water(Bytes& corner, const Map& m, const Info& out) {
    const int w = out.grid, cw = w + 1;
    Bytes wet((size_t)cw * cw, 0), seen(wet.size(), 0);
    for (int y = 0; y <= w; ++y) for (int x = 0; x <= w; ++x) {
        bool all = true;
        for (int dy = -1; dy <= 0; ++dy) for (int dx = -1; dx <= 0; ++dx) {
            const int c = std::max(0, std::min(w - 1, y + dy)) * w + std::max(0, std::min(w - 1, x + dx));
            all = all && (m.tmpl[c] == 1 || m.tmpl[c] == 2);
        }
        wet[y * cw + x] = all;
    }
    for (int s = 0; s < (int)wet.size(); ++s) if (wet[s] && !seen[s]) {
        std::vector<int> body(1, s); seen[s] = 1; unsigned char lo = corner[s];
        for (size_t p = 0; p < body.size(); ++p) {
            const int c = body[p], x = c % cw, y = c / cw; lo = std::min(lo, corner[c]);
            const int ns[4] = {x ? c - 1 : -1, x < w ? c + 1 : -1, y ? c - cw : -1, y < w ? c + cw : -1};
            for (int n : ns) if (n >= 0 && wet[n] && !seen[n]) { seen[n] = 1; body.push_back(n); }
        }
        for (int c : body) corner[c] = lo;
    }
}
inline std::string marked_ini(const Map& m, const Info& out) {
    std::string source = text(m.source);
    // Native Westwood INI readers require '[' as the first byte. Keep the BOM in
    // the archived original, but remove the encoding marker in the playable copy.
    if (source.compare(0, 3, "\xef\xbb\xbf") == 0) source.erase(0, 3);
    const std::string nl = source.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    std::string result, sec; size_t p = 0; bool inserted = false;
    while (p < source.size()) {
        size_t end = source.find('\n', p); if (end == std::string::npos) end = source.size(); else ++end;
        const std::string whole = source.substr(p, end - p); std::string line = trim(whole); p = end;
        if (result.empty() && line.compare(0, 3, "\xef\xbb\xbf") == 0) line.erase(0, 3);
        if (!line.empty() && line[0] == '[') {
            const size_t close = line.find(']'); sec = close == std::string::npos ? "" : lower(trim(line.substr(1, close - 1)));
            // This checksum describes the original bytes, which the metadata
            // insertion changes. Native readers accept a missing digest; keep
            // the original block only in the exact source archive.
            if (sec == "digest") continue;
            result += whole;
            if (sec == "basic" && !inserted) {
                if (result.empty() || result.back() != '\n') result += nl;
                result += "OpenCNC3DKind=" + out.kind + nl + "OpenCNC3DImported=1" + nl;
                inserted = true;
            }
            continue;
        }
        if (sec == "digest") continue;
        const size_t eq = line.find('='); const std::string key = eq == std::string::npos ? "" : lower(trim(line.substr(0, eq)));
        if (sec == "basic" && (key == "opencnc3dkind" || key == "opencnc3dimported")) continue;
        result += whole;
    }
    return result;
}
inline bool publish(const std::string& src, const std::string& dst) {
#ifdef _WIN32
    return MoveFileW(wide(src).c_str(), wide(dst).c_str()) != 0;
#else
    if (link(src.c_str(), dst.c_str()) != 0) return false;
    unlink(src.c_str()); return true;
#endif
}
inline std::string scenario_code(const Bytes& ini, const Bytes& bin, const Bytes& hgt, unsigned attempt) {
    // A content name, not the user's next free slot: peers importing the same map
    // get the same lobby scenario and fingerprint regardless of import order.
    const std::string key = digest(ini) + ":" + digest(bin) + ":" + digest(hgt) + ":" + std::to_string(attempt);
    uint64_t h = hash(bytes(key)); std::string code = "IMP";
    static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    for (int k = 0; k < 5; ++k) { code.push_back(alphabet[h % 31]); h /= 31; }
    return code;
}
inline bool same_file(const std::string& p, const Bytes& expected) {
    Bytes got; Info ignored; return read(p, got, ignored) && got == expected;
}
inline void report(Progress cb, void* user, int n, const char* stage) { if (cb) cb(user, n, stage); }
} // detail

inline bool inspect_map(const std::string& source, Info& out) { detail::Map m; return detail::load(source, m, out); }

inline bool run_map(const std::string& source, const std::string& td_dir, const std::string& ra_dir,
                Info& out, Progress progress = NULL, void* user = NULL) {
    using namespace detail;
    report(progress, user, 3, "Reading original map");
    Map m; if (!load(source, m, out)) return false;
    report(progress, user, 18, "Checking terrain and player starts");
    const bool supplied_heights = !m.hgt.empty();
    if (!supplied_heights) {
        report(progress, user, 30, "Generating Auto Heightmap");
        if (out.red_alert && out.theater == "INTERIOR") m.hgt.assign((size_t)(out.grid + 1) * (out.grid + 1), 0);
        else {
            AutoHeightIn in; in.W = in.H = out.grid; in.tmpl = m.tmpl.data(); in.icon = m.icon.data();
            in.px = m.x; in.py = m.y; in.pw = out.width; in.ph = out.height; in.foot = m.foot;
            in.seed = (uint32_t)(hash(m.source) ^ hash(m.bin));
            if (out.multiplayer) for (int c : m.start) in.starts.push_back({c % m.stride, c / m.stride});
            AutoHeightOut heights; autoheight_generate(in, heights); m.hgt.swap(heights.corner);
            if (out.red_alert) flatten_water(m.hgt, m, out);
        }
    }
    if (m.hgt.size() != (size_t)(out.grid + 1) * (out.grid + 1)) return fail(out, "Auto Heightmap did not produce the complete corner grid.");
    report(progress, user, 70, supplied_heights ? "Preserving supplied heightmap" : "Auto Heightmap complete");
    const std::string dest = out.red_alert ? ra_dir : td_dir;
    if (dest.empty() || !mkdirs(dest)) return fail(out, "The user maps folder cannot be created or written.");
    const Bytes runtime_ini = bytes(marked_ini(m, out));
    std::string code, stage;
    for (unsigned n = 0; n < 1024; ++n) {
        const std::string name = scenario_code(runtime_ini, m.bin, m.source_hgt, n);
        const std::string prefix = join(dest, name);
        bool occupied = false;
        // Case-insensitive collision checks also protect imports copied from Windows.
        std::vector<std::string> names;
        if (list_dir(dest, names)) {
            const std::string target = lower(name);
            for (const std::string& entry : names) if (lower(entry).compare(0, target.size() + 1, target + ".") == 0) { occupied = true; break; }
        } else return fail(out, "The user maps folder cannot be scanned.");
        if (occupied) {
            Bytes old_height; Info ignored;
            const bool height_ok = supplied_heights ? same_file(prefix + ".HGT", m.hgt)
                : read(prefix + ".HGT", old_height, ignored) && old_height.size() == m.hgt.size();
            if (same_file(prefix + ".INI", runtime_ini) && height_ok &&
                (out.red_alert || same_file(prefix + ".BIN", m.bin))) {
                out.scenario = name; out.message = "Already imported " + out.name + ".";
                report(progress, user, 100, "Map already imported"); return true;
            }
            continue;
        }
        const std::string candidate = join(dest, "." + name + ".importing");
        if (make_dir(candidate) == 0) { code = name; stage = candidate; break; }
        if (errno != EEXIST) return fail(out, "The user maps folder is not writable.");
    }
    if (code.empty()) return fail(out, "All import names are occupied; no existing map was replaced.");
    out.scenario = code;
    struct File { std::string suffix; Bytes data; };
    std::vector<File> files;
    if (!out.red_alert) files.push_back({".BIN", m.bin});
    files.push_back({".HGT", m.hgt});
    files.push_back({m.source_json.empty() ? ".source" + ext(m.path) : ".recovered.ini", m.source});
    if (!m.source_json.empty()) files.push_back({".source.json", m.source_json});
    if (!out.red_alert) files.push_back({".source.bin", m.bin});
    if (!m.hgt_path.empty()) files.push_back({".source.hgt", m.source_hgt});
    // PPM is a portable lossless preview sidecar; the in-memory UI uses RGBA directly.
    std::string ppm = "P6\n" + std::to_string(out.preview_width) + " " + std::to_string(out.preview_height) + "\n255\n";
    Bytes preview_bytes = bytes(ppm);
    for (size_t p = 0; p < out.preview_rgba.size(); p += 4) preview_bytes.insert(preview_bytes.end(), out.preview_rgba.begin() + p, out.preview_rgba.begin() + p + 3);
    files.push_back({".preview.ppm", preview_bytes});
    std::string receipt = "OpenCNC3D native map import v1\nGame=" + out.game + "\nKind=" + out.kind + "\nSource=" + leaf(m.path) +
        "\nSourceFNV1a64=" + digest(m.source) + "\nTerrainFNV1a64=" + digest(m.bin) + "\nHeightFNV1a64=" + digest(m.hgt) +
        "\nHeight=" + (supplied_heights ? "preserved" : "auto-generated") + "\nGrid=" + std::to_string(out.grid) +
        "\nPlayable=" + std::to_string(m.x) + "," + std::to_string(m.y) + "," + std::to_string(out.width) + "," + std::to_string(out.height) +
        "\nSource scripts and rules retained; OpenCNC3DKind/OpenCNC3DImported metadata added; stale Digest block and UTF-8 BOM removed only from playable copy if present.\n" +
        "Warnings=" + out.warnings + "\n";
    files.push_back({".import.txt", bytes(receipt)});
    files.push_back({".INI", runtime_ini}); // catalog-visible commit, always last
    bool ok = true; std::vector<std::string> published;
    report(progress, user, 82, "Saving map and preserving original files");
    for (const File& f : files) if (!write(join(stage, code + f.suffix), f.data)) { ok = false; break; }
    if (ok) for (const File& f : files) {
        const std::string target = join(dest, code + f.suffix);
        if (!publish(join(stage, code + f.suffix), target)) { ok = false; break; }
        published.push_back(target);
    }
    if (!ok) for (const std::string& p : published) file_remove(p);
    for (const File& f : files) file_remove(join(stage, code + f.suffix));
    remove_dir(stage);
    if (!ok) { out.scenario.clear(); return fail(out, "Import could not be saved. Partial output was removed and existing maps were kept."); }
    out.message = "Imported " + out.name + " as " + out.game + " " + out.kind + ".";
    if (!out.warnings.empty()) out.message += " " + out.warnings;
    report(progress, user, 100, "Import complete"); return true;
}
} // namespace mapimport
#include "map_import_archive.h"
