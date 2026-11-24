/* ---------------------------------------------------------------------------------- *
 *  THE LEDGER'S COUNT OF THE TEXTURES ON THE CARD, and its line at a mission's end.
 *
 *  The memory ledger (memledger.h, memledger_scene.h) reads named amounts off the
 *  structures that hold them. Two more readings live here:
 *
 *    water_gl      what the water's mission caches hold on the card (water_mod.h);
 *                  always in the line
 *    gl_tex_live   every texture the GL context holds, summed over every level (and
 *    gl_tex_bytes  the same in bytes, exactly), and
 *    gl_tex_count  how many there are: every texture name the context answers for is
 *                  asked its levels' sizes and formats. A texel is counted at the size
 *                  the format asks the card for (four bytes for RGB, as everywhere in the
 *                  ledger); render buffers and vertex buffers are not textures and are
 *                  not counted. Only with OPENCNC3D_MEMLEDGER_GLSCAN=1, or at a mission's
 *                  end below, because asking every name costs a few tenths of a second.
 *
 *  OPENCNC3D_MEMLEDGER_SHUTDOWN=1 (read once) prints the whole ledger, with the texture
 *  count, as the last thing game_shutdown does: MEM|ledger|shutdown<N>|..., N counting
 *  the missions this process has ended. That is the one moment between two missions in
 *  every way a process runs them (the menu shell's round trips, a script's `remission`,
 *  a renderer's own exit), so what one mission leaves behind for the next can be read.
 *
 *  Both are test switches; neither draws anything or changes what is drawn.
 * ---------------------------------------------------------------------------------- */
#ifndef OPENCNC3D_MEMLEDGER_GL_H
#define OPENCNC3D_MEMLEDGER_GL_H

static void memledger_collect(void);   /* memledger_scene.h */

/* bytes the card is asked for per texel of an internal format */
static int memledger_gl_texel_bytes(GLint f)
{
    switch (f) {
    case GL_ALPHA: case 0x803C: case GL_LUMINANCE: case 0x8040: case 0x8229: case GL_INTENSITY: case 0x804B:
        return 1;
    case GL_LUMINANCE_ALPHA: case 0x8045: case 0x822B: case 0x822A: case 0x822D:
        return 2;
    case 0x805B: case 0x881A: case 0x881B: case 0x8230:
        return 8;
    case 0x8814: case 0x8815:
        return 16;
    default:
        return 4;
    }
}

/* the bytes of every level of the texture bound to `target` (a cube map's six faces) */
static long long memledger_gl_bound_bytes(GLenum target)
{
    long long bytes = 0;
    const bool cube = target == GL_TEXTURE_CUBE_MAP;
    for (int face = 0; face < (cube ? 6 : 1); face++) {
        const GLenum q = cube ? (GLenum)(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face) : target;
        for (int level = 0; level < 16; level++) {
            GLint w = 0, h = 0, d = 1, f = 0, compressed = 0;
            glGetTexLevelParameteriv(q, level, GL_TEXTURE_WIDTH, &w);
            glGetTexLevelParameteriv(q, level, GL_TEXTURE_HEIGHT, &h);
            if (w <= 0 || h <= 0) break;
            if (target == GL_TEXTURE_3D || target == 0x8C1A /* 2D array */)
                glGetTexLevelParameteriv(q, level, GL_TEXTURE_DEPTH, &d);
            glGetTexLevelParameteriv(q, level, GL_TEXTURE_INTERNAL_FORMAT, &f);
            glGetTexLevelParameteriv(q, level, GL_TEXTURE_COMPRESSED, &compressed);
            if (compressed) {
                GLint size = 0;
                glGetTexLevelParameteriv(q, level, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &size);
                bytes += size;
            } else {
                bytes += (long long)w * h * (d > 0 ? d : 1) * memledger_gl_texel_bytes(f);
            }
            if (target == 0x84F5 /* rectangle */) break;
        }
    }
    return bytes;
}

/* Every texture of the current context: its bytes, and how many there are in *count.
   Names are asked in order until 65536 in a row are not textures. A name is bound to
   each target in turn until one takes it; the binds go around the state cache (the
   parenthesised name), since a refused bind would teach it a binding that never
   happened, and the bindings of texture unit 0 are put back the same way, so the cache's
   view stays the truth. Any GL error the asking raised is cleared. */
static long long memledger_gl_scan(long long* count)
{
    static const GLenum targets[5] = { GL_TEXTURE_2D, 0x8C1A /* 2D array */, GL_TEXTURE_3D,
                                       GL_TEXTURE_CUBE_MAP, 0x84F5 /* rectangle */ };
    static const GLenum bindings[5] = { GL_TEXTURE_BINDING_2D, 0x8C1D, GL_TEXTURE_BINDING_3D,
                                        GL_TEXTURE_BINDING_CUBE_MAP, 0x84F6 };
    GLint active = GL_TEXTURE0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glActiveTexture(GL_TEXTURE0);
    GLint was[5] = { 0, 0, 0, 0, 0 };
    for (int t = 0; t < 5; t++) glGetIntegerv(bindings[t], &was[t]);
    while (glGetError() != GL_NO_ERROR) {}
    long long bytes = 0, n = 0;
    unsigned run = 0;
    for (GLuint name = 1; run < 65536u && name < (1u << 22); name++) {
        if (!glIsTexture(name)) { run++; continue; }
        run = 0; n++;
        for (int t = 0; t < 5; t++) {
            (glBindTexture)(targets[t], name);
            if (glGetError() != GL_NO_ERROR) continue;
            bytes += memledger_gl_bound_bytes(targets[t]);
            break;
        }
    }
    for (int t = 0; t < 5; t++) (glBindTexture)(targets[t], (GLuint)was[t]);
    while (glGetError() != GL_NO_ERROR) {}
    glActiveTexture((GLenum)active);
    if (count) *count = n;
    return bytes;
}

/* The readings this header adds to the ledger line; `scan` asks every texture too. */
static void memledger_gl_note(bool scan)
{
    water_memledger_note();
    static const bool asked = [](){ const char* p = getenv("OPENCNC3D_MEMLEDGER_GLSCAN"); return p && *p && strcmp(p, "0"); }();
    if (!scan && !asked) return;
    long long n = 0;
    const long long b = memledger_gl_scan(&n);
    memledger_bytes("gl_tex_live", b);
    memledger_count("gl_tex_bytes", b);   /* the same, exact, for comparing two moments */
    memledger_count("gl_tex_count", n);
}

/* The last thing game_shutdown does, with OPENCNC3D_MEMLEDGER_SHUTDOWN=1. */
static void memledger_shutdown_note(void)
{
    static const bool on = [](){ const char* p = getenv("OPENCNC3D_MEMLEDGER_SHUTDOWN"); return p && *p && strcmp(p, "0"); }();
    if (!on) return;
    static int missions = 0;
    missions++;
    memledger_collect();
    memledger_gl_note(true);
    char label[32];
    snprintf(label, sizeof label, "shutdown%d", missions);
    memledger_print(stdout, label);
}

#endif
