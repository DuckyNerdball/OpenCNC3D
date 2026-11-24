/* ---------------------------------------------------------------------------------- *
 *  THE MEMORY LEDGER: named numbers about what the renderer is holding, for tests.
 *
 *  A key-to-value table any code can write: a byte count ("unity_gl" bytes of Unity
 *  flipbooks on the card), a plain count ("readbacks", whole-texture glGetTexImage
 *  calls), or a short text ("decal_sets"). Nothing reads it back to make a decision; it
 *  exists so a test can ask what a scene holds and compare two builds or two settings.
 *  The `memledger` script verb prints the whole table as one line:
 *
 *      MEM|ledger|<label>|key=value|key=value|...
 *
 *  keys in alphabetical order, byte values in MB (2^20 bytes) with one decimal, counts
 *  as integers. memledger_scene.h fills in the values that are read off the live state
 *  just before that line (what the scene holds now), and adds the process's own commit
 *  numbers on Windows.
 *
 *  Three test hooks live here too, because everything that tests memory needs them:
 *
 *  - THE READBACK COUNT. Every glGetTexImage in this translation unit passes through
 *    gl_state_cache.h's macro, which calls memledger_readback_note: a whole level of a
 *    texture copied back to system memory, counted with the file and line that asked.
 *    The texture hash below reads textures back too, and does not count (it calls the
 *    function unwrapped), so switching it on does not change the count it reports.
 *
 *  - FAILURE INJECTION, OPENCNC3D_MEM_FAIL=site[:n][,site[:n]...]. A site that calls
 *    memfail_point("site") throws std::bad_alloc on its n-th call (default 1), once,
 *    and prints MEM|fail-injected first. Unset, a call is one test of a flag that is
 *    false. It exists so the path a refused allocation takes can be run on purpose on a
 *    machine that has memory to spare.
 *
 *  - TEXTURE HASHES, OPENCNC3D_MEMDEBUG_TEXHASH=1. texhash_gl reads every level of a
 *    texture back and prints a 64-bit FNV-1a of its bytes as a TEXHASH| line;
 *    texhash_mem does the same for a block of memory. Two builds that are meant to
 *    upload the same pixels by different routes can then be compared exactly. Off, both
 *    return at once. On, they read whole textures back and cost time and memory: a test
 *    switch, never a player's.
 *
 *  None of this draws anything or changes what is drawn.
 * ---------------------------------------------------------------------------------- */
#ifndef OPENCNC3D_MEMLEDGER_H
#define OPENCNC3D_MEMLEDGER_H

enum { MEMLEDGER_BYTES = 0, MEMLEDGER_COUNT = 1, MEMLEDGER_TEXT = 2 };
struct MemLedgerEntry { int kind; long long value; std::string text; };
static std::map<std::string, MemLedgerEntry>& memledger_table(void)
{
    static std::map<std::string, MemLedgerEntry> table;
    return table;
}
static void memledger_put(const char* key, int kind, long long value)
{
    MemLedgerEntry& e = memledger_table()[key];
    e.kind = kind; e.value = value; e.text.clear();
}
/* set a byte count, set a plain count, add to either, set a text */
static void memledger_bytes(const char* key, long long bytes) { memledger_put(key, MEMLEDGER_BYTES, bytes); }
static void memledger_count(const char* key, long long n)     { memledger_put(key, MEMLEDGER_COUNT, n); }
static void memledger_add(const char* key, long long delta, int kind = MEMLEDGER_BYTES)
{
    std::map<std::string, MemLedgerEntry>& t = memledger_table();
    std::map<std::string, MemLedgerEntry>::iterator it = t.find(key);
    if (it == t.end()) { memledger_put(key, kind, delta); return; }
    it->second.value += delta;
}
/* a high-water mark: the value is raised to `bytes` if that is more */
static void memledger_peak(const char* key, long long bytes)
{
    std::map<std::string, MemLedgerEntry>& t = memledger_table();
    std::map<std::string, MemLedgerEntry>::iterator it = t.find(key);
    if (it == t.end()) { memledger_put(key, MEMLEDGER_BYTES, bytes); return; }
    if (bytes > it->second.value) it->second.value = bytes;
}
static void memledger_text(const char* key, const std::string& s)
{
    MemLedgerEntry& e = memledger_table()[key];
    e.kind = MEMLEDGER_TEXT; e.value = 0; e.text = s;
}
static long long memledger_get(const char* key)
{
    std::map<std::string, MemLedgerEntry>& t = memledger_table();
    std::map<std::string, MemLedgerEntry>::iterator it = t.find(key);
    return it == t.end() ? 0 : it->second.value;
}
/* The table as one line, to `to`, with `label` after MEM|ledger (empty is allowed). A
   text value has '|' and '=' replaced, so the line always splits the same way. */
static void memledger_print(FILE* to, const char* label)
{
    std::string line = "MEM|ledger|";
    line += label ? label : "";
    char num[64];
    std::map<std::string, MemLedgerEntry>& t = memledger_table();
    for (std::map<std::string, MemLedgerEntry>::iterator it = t.begin(); it != t.end(); ++it) {
        line += "|"; line += it->first; line += "=";
        const MemLedgerEntry& e = it->second;
        if (e.kind == MEMLEDGER_BYTES) {
            snprintf(num, sizeof num, "%.1f", (double)e.value / 1048576.0);
            line += num;
        } else if (e.kind == MEMLEDGER_COUNT) {
            snprintf(num, sizeof num, "%lld", e.value);
            line += num;
        } else {
            for (size_t k = 0; k < e.text.size(); k++)
                line += (e.text[k] == '|' || e.text[k] == '=') ? '/' : e.text[k];
        }
    }
    fprintf(to, "%s\n", line.c_str());
    fflush(to);
}

/* ---- whole-texture readbacks, counted with where they were asked ---- */
struct MemReadbackSite { const char* file; int line; long long n; };
static std::vector<MemReadbackSite>& memledger_readback_sites(void)
{
    static std::vector<MemReadbackSite> sites;
    return sites;
}
static void memledger_readback_note(const char* file, int line)
{
    memledger_add("readbacks", 1, MEMLEDGER_COUNT);
    std::vector<MemReadbackSite>& s = memledger_readback_sites();
    const char* base = strrchr(file, '/');
    base = base ? base + 1 : file;
    for (size_t i = 0; i < s.size(); i++)
        if (s[i].line == line && !strcmp(s[i].file, base)) { s[i].n++; return; }
    MemReadbackSite site = { base, line, 1 };
    s.push_back(site);
}
/* "file:line*n,file:line*n" in the order they were first asked, for the ledger line */
static std::string memledger_readback_where(void)
{
    std::string out;
    char one[128];
    const std::vector<MemReadbackSite>& s = memledger_readback_sites();
    for (size_t i = 0; i < s.size(); i++) {
        snprintf(one, sizeof one, "%s%s:%d*%lld", i ? "," : "", s[i].file, s[i].line, s[i].n);
        out += one;
    }
    return out;
}

/* ---- failure injection: OPENCNC3D_MEM_FAIL=site[:n][,site[:n]...] ---- */
struct MemFailSite { std::string name; long long at, seen; };
static std::vector<MemFailSite>& memfail_sites(void)
{
    static std::vector<MemFailSite> sites;
    return sites;
}
static bool memfail_armed(void)
{
    static int armed = -1;
    if (armed >= 0) return armed != 0;
    armed = 0;
    const char* spec = getenv("OPENCNC3D_MEM_FAIL");
    if (!spec || !*spec) return false;
    std::string all = spec;
    size_t from = 0;
    while (from <= all.size()) {
        size_t comma = all.find(',', from);
        if (comma == std::string::npos) comma = all.size();
        std::string one = all.substr(from, comma - from);
        from = comma + 1;
        if (one.empty()) continue;
        MemFailSite s;
        const size_t colon = one.find(':');
        s.name = one.substr(0, colon);
        s.at = colon == std::string::npos ? 1 : atoll(one.c_str() + colon + 1);
        if (s.at < 1) s.at = 1;
        s.seen = 0;
        memfail_sites().push_back(s);
        printf("MEM|fail-armed|site=%s|call=%lld\n", s.name.c_str(), s.at);
    }
    fflush(stdout);
    armed = memfail_sites().empty() ? 0 : 1;
    return armed != 0;
}
/* Call where a named allocation is about to be made. Throws std::bad_alloc on the armed
   call of an armed site; otherwise returns. */
static void memfail_point(const char* site)
{
    if (!memfail_armed()) return;
    std::vector<MemFailSite>& s = memfail_sites();
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i].name != site) continue;
        if (++s[i].seen != s[i].at) return;
        printf("MEM|fail-injected|site=%s|call=%lld\n", site, s[i].at);
        fflush(stdout);
        throw std::bad_alloc();
    }
}

/* ---- texture hashes: OPENCNC3D_MEMDEBUG_TEXHASH=1 ---- */
static bool texhash_on(void)
{
    static const bool on = [](){ const char* p = getenv("OPENCNC3D_MEMDEBUG_TEXHASH"); return p && *p && strcmp(p, "0"); }();
    return on;
}
static unsigned long long texhash_fnv(const void* p, size_t n, unsigned long long h = 1469598103934665603ULL)
{
    const unsigned char* b = (const unsigned char*)p;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ULL; }
    return h;
}
/* TEXHASH|<name>|mem|bytes=N|fnv=H (or |empty) */
static void texhash_mem(const char* name, const void* p, size_t n)
{
    if (!texhash_on()) return;
    if (!p || !n) printf("TEXHASH|%s|mem|empty\n", name);
    else printf("TEXHASH|%s|mem|bytes=%zu|fnv=%016llx\n", name, n, texhash_fnv(p, n));
    fflush(stdout);
}
/* TEXHASH|<name>|gl|levels=L|w=W|h=H|ifmt=0xF|bytes=N|fnv=H, every level read back as
   RGBA (16-bit RGBA for a 16-bit texture), or |none for no texture. The caller's 2D
   binding and the pack state are handed back. The read goes around the readback count
   (the parenthesised name skips gl_state_cache.h's macro). */
static void texhash_gl(const char* name, GLuint tex)
{
    if (!texhash_on()) return;
    if (!tex || !glIsTexture(tex)) { printf("TEXHASH|%s|gl|none\n", name); fflush(stdout); return; }
    GLint was = 0, align = 4, rowLen = 0, skipRows = 0, skipPix = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &was);
    glGetIntegerv(GL_PACK_ALIGNMENT, &align);
    glGetIntegerv(GL_PACK_ROW_LENGTH, &rowLen);
    glGetIntegerv(GL_PACK_SKIP_ROWS, &skipRows);
    glGetIntegerv(GL_PACK_SKIP_PIXELS, &skipPix);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    GLint w0 = 0, h0 = 0, ifmt = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w0);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h0);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &ifmt);
    const bool wide = ifmt == GL_RGBA16 || ifmt == GL_RGB16 || ifmt == 0x822C /* RG16 */;
    unsigned long long h = 1469598103934665603ULL;
    size_t total = 0;
    int levels = 0;
    std::vector<unsigned char> px;
    for (int level = 0; level < 16; level++) {
        GLint w = 0, hh = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_WIDTH, &w);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_HEIGHT, &hh);
        if (w <= 0 || hh <= 0) break;
        const size_t n = (size_t)w * hh * (wide ? 8 : 4);
        px.assign(n, 0);
        (glGetTexImage)(GL_TEXTURE_2D, level, GL_RGBA, wide ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE, &px[0]);
        h = texhash_fnv(&px[0], n, h);
        total += n;
        levels++;
    }
    std::vector<unsigned char>().swap(px);
    glPixelStorei(GL_PACK_ALIGNMENT, align);
    glPixelStorei(GL_PACK_ROW_LENGTH, rowLen);
    glPixelStorei(GL_PACK_SKIP_ROWS, skipRows);
    glPixelStorei(GL_PACK_SKIP_PIXELS, skipPix);
    glBindTexture(GL_TEXTURE_2D, (GLuint)was);
    printf("TEXHASH|%s|gl|levels=%d|w=%d|h=%d|ifmt=0x%x|bytes=%zu|fnv=%016llx\n",
           name, levels, w0, h0, (unsigned)ifmt, total, h);
    fflush(stdout);
}

#endif
