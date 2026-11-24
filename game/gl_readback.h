/* ---------------------------------------------------------------------------------- *
 *  READING A TEXTURE BACK IN BANDS OF ROWS.
 *
 *  A few ground and water builds need the pixels of a texture that only the card holds
 *  (the sheet the ground is drawing, the soft-coast sheet). glGetTexImage hands back the
 *  whole of level 0 in one call, into a whole copy the caller holds. Measured on a Mac,
 *  the driver then also keeps a system-memory copy of every texture read that way for
 *  as long as the texture lives: 145 MB for Red Alert's 6240 x 6084 ground sheet, 240 MB
 *  over Tiberian Dawn's reads of one mission. On a 64-bit Windows driver a private region
 *  of the sheet's size was measured beside each such read, kept to the mission's end.
 *  Here the same bytes are read a band of rows at a time, which the Mac driver was
 *  measured to keep no copy of: the texture is attached to a framebuffer object kept for
 *  this alone and glReadPixels reads one band, then the next, into the caller's memory,
 *  which need not hold the whole texture either. Level 0 of a texture attached as a colour
 *  buffer is the colour buffer, so a band read gives exactly the bytes glGetTexImage
 *  gives for those rows: same rows, same order, same format.
 *
 *  What a read changes and hands back: the framebuffer binding (through the state cache's
 *  own wrappers, fx_gl.h, so the cache follows), and the pack alignment, row length and
 *  skips (set to tightly packed rows for the read, then restored). The read buffer is the
 *  read framebuffer's own state, so the caller's framebuffer keeps its own throughout; the
 *  read framebuffer's is set once, to its one attachment. The texture is detached after
 *  every read, so it is never left attached anywhere. The 2D binding of texture unit 0 is
 *  borrowed for a moment to ask the texture's format, and handed back.
 *
 *  WHEN A BAND READ CANNOT BE TRUSTED, the reader reads the texture whole with one
 *  glGetTexImage, which is what every caller did before, and serves the rest of the read
 *  from that copy (a READBACK|whole-fallback line names the reason):
 *    - the framebuffer object cannot be used: the entry points are missing, or the
 *      driver reports the attachment incomplete or raises an error attaching it;
 *    - the texture is not stored as eight-bit RGBA. Only that format is read through the
 *      framebuffer: for a luminance or two-channel texture glReadPixels and glGetTexImage
 *      are specified to fill the channels differently;
 *    - the driver raises an error on a band read. The error queue is emptied before the
 *      attachment and read after every band, so an error is this reader's own, and the
 *      band that raised it is served from the whole copy like every band after it;
 *    - an alpha-only read (GL_ALPHA) disagrees with the alpha of the same row read as
 *      RGBA. The first band of every alpha read is checked that way, one extra row.
 *
 *  THE EDITOR keeps the whole reads. It rewrites the ground in place and rebuilds the
 *  ground surface and the sea from it on every frame it draws, so there a band read costs
 *  its time on every frame: measured on a Mac, band reads made a frame of Tiberian Dawn's
 *  editor on the beach about 30 ms slower (174 ms to between 200 and 211) and Red Alert's
 *  about 40 ms slower (of about 590).
 *  gl_readback_whole() is therefore true while the editor is open, and the editor reads
 *  exactly as it always did, the memory those reads take included.
 *
 *  SWITCHES, environment variables read once:
 *    OPENCNC3D_READBACK_WHOLE=1   every caller takes its whole-texture readback exactly as
 *                                 it used to, for comparing the two routes;
 *    OPENCNC3D_READBACK_VERIFY=1  (test only) every banded read also reads its texture
 *                                 whole and compares each band with it, printing one
 *                                 READBACK|verify line a texture, so one run of a scene
 *                                 proves a driver's band reads with no second run to
 *                                 compare against. It holds the whole copy while it reads;
 *    OPENCNC3D_READBACK_INJECT=kind:n[,kind:n]  (test only) makes the n-th attachment
 *                                 (incomplete), band read (error) or alpha check (alpha)
 *                                 of the process fail, so each way out above can be run
 *                                 on a driver that never fails. An injected band read is
 *                                 a real glReadPixels with a format the driver must refuse.
 *
 *  Counted in the memory ledger (memledger.h): readback_banded, the textures read this
 *  way, and readback_bands, the band reads. A fallback's glGetTexImage is counted as a
 *  readback like any other, and so is a verify run's whole read.
 * ---------------------------------------------------------------------------------- */
#ifndef OPENCNC3D_GL_READBACK_H
#define OPENCNC3D_GL_READBACK_H

static bool gl_readback_switch(const char* name)
{
    const char* p = getenv(name);
    return p && *p && strcmp(p, "0");
}

/* Defined beside the map editor (cnc_eyes.cpp, after edit_mod.h): true while it is open. */
static bool gl_readback_editor_open(void);

/* Should a caller read its texture back whole, as before? Asked at every read. */
static bool gl_readback_whole(void)
{
    static const bool on = gl_readback_switch("OPENCNC3D_READBACK_WHOLE");
    return on || gl_readback_editor_open();
}

static bool gl_readback_verify(void)
{
    static const bool on = gl_readback_switch("OPENCNC3D_READBACK_VERIFY");
    return on;
}

/* OPENCNC3D_READBACK_INJECT: true on the armed call of `kind` ("incomplete", "error",
   "alpha"), once. */
static bool gl_readback_inject(const char* kind)
{
    struct Site { char kind[16]; long long at, seen; };
    static std::vector<Site> sites;
    static bool parsed = false;
    if (!parsed) {
        parsed = true;
        const char* p = getenv("OPENCNC3D_READBACK_INJECT");
        while (p && *p) {
            const char* comma = strchr(p, ',');
            const size_t n = comma ? (size_t)(comma - p) : strlen(p);
            Site s;
            size_t k = 0;
            while (k < n && p[k] != ':' && k + 1 < sizeof s.kind) { s.kind[k] = p[k]; k++; }
            s.kind[k] = 0;
            const char* colon = (const char*)memchr(p, ':', n);
            s.at = colon ? atoll(colon + 1) : 1;
            if (s.at < 1) s.at = 1;
            s.seen = 0;
            if (s.kind[0]) {
                sites.push_back(s);
                printf("READBACK|inject-armed|kind=%s|call=%lld\n", s.kind, s.at);
                fflush(stdout);
            }
            p = comma ? comma + 1 : p + n;
        }
    }
    for (size_t i = 0; i < sites.size(); i++) {
        if (strcmp(sites[i].kind, kind)) continue;
        if (++sites[i].seen != sites[i].at) continue;
        printf("READBACK|inject|kind=%s|call=%lld\n", kind, sites[i].at);
        fflush(stdout);
        return true;
    }
    return false;
}

/* bytes a texel takes in a read of `format` as GL_UNSIGNED_BYTE */
static int gl_readback_texel_bytes(GLenum format)
{
    switch (format) {
    case GL_ALPHA: case GL_LUMINANCE: case GL_RED: return 1;
    case GL_LUMINANCE_ALPHA: return 2;
    case GL_RGB: return 3;
    default: return 4;
    }
}

/* the internal formats read through the framebuffer: eight-bit RGBA (GL_RGBA8, GL_RGBA,
   and the legacy component count 4) */
static bool gl_readback_rgba8(GLint ifmt)
{
    return ifmt == GL_RGBA8 || ifmt == GL_RGBA || ifmt == 4;
}

static GLuint g_glReadbackFbo = 0;
static SDL_GLContext g_glReadbackContext = NULL;   /* the context that name belongs to */

/* One texture being read band by band. begin() attaches it (or, when that cannot work,
   reads it whole into `whole`), rows() copies any band of rows into the caller's memory,
   end() detaches it and hands every piece of state back. */
struct GlBandReader {
    GLuint tex;
    int w, h, bpp;
    GLenum format;
    GLint ifmt;
    bool open, fbo, alphaChecked;
    GLint prevFb, align, rowLen, skipRows, skipPix;
    std::vector<unsigned char> whole;   /* only once the reader has fallen back */
    std::vector<unsigned char> check;   /* only under OPENCNC3D_READBACK_VERIFY */
    long long verifyBands, verifyBad;
    int verifyFirst;

    GlBandReader() : tex(0), w(0), h(0), bpp(4), format(GL_RGBA), ifmt(0), open(false), fbo(false),
                     alphaChecked(false), prevFb(0), align(4), rowLen(0), skipRows(0), skipPix(0),
                     verifyBands(0), verifyBad(0), verifyFirst(-1) {}
    ~GlBandReader() { end(); }

    /* level 0 whole, as glGetTexImage gives it, into `out` (w * h * bpp bytes) */
    void read_whole_into(std::vector<unsigned char>& out)
    {
        out.assign((size_t)w * h * bpp, 0);
        GLint active = GL_TEXTURE0, bound = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
        glBindTexture(GL_TEXTURE_2D, tex);
        glGetTexImage(GL_TEXTURE_2D, 0, format, GL_UNSIGNED_BYTE, &out[0]);
        glBindTexture(GL_TEXTURE_2D, (GLuint)bound);
        glActiveTexture((GLenum)active);
    }

    /* the framebuffer cannot be trusted for this texture: from here on every band comes
       from one whole read, the route every caller took before */
    void fall_back(const char* why, GLenum err)
    {
        if (fbo) {
            fx_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
            fx_glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFb);
            fbo = false;
        }
        read_whole_into(whole);
        std::vector<unsigned char>().swap(check);
        fprintf(stderr, "READBACK|whole-fallback|reason=%s|err=0x%x|tex=%u|%dx%d|format=0x%x|ifmt=0x%x\n",
                why, (unsigned)err, tex, w, h, (unsigned)format, (unsigned)ifmt);
    }

    bool begin(GLuint t, int width, int height, GLenum fmt)
    {
        end();
        tex = t; w = width; h = height; format = fmt; bpp = gl_readback_texel_bytes(fmt);
        if (!tex || w <= 0 || h <= 0) return false;
        open = true;
        alphaChecked = false;
        verifyBands = verifyBad = 0; verifyFirst = -1;
        glGetIntegerv(GL_PACK_ALIGNMENT, &align);
        glGetIntegerv(GL_PACK_ROW_LENGTH, &rowLen);
        glGetIntegerv(GL_PACK_SKIP_ROWS, &skipRows);
        glGetIntegerv(GL_PACK_SKIP_PIXELS, &skipPix);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        glPixelStorei(GL_PACK_SKIP_ROWS, 0);
        glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
        {
            GLint active = GL_TEXTURE0, bound = 0;
            glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
            glActiveTexture(GL_TEXTURE0);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
            glBindTexture(GL_TEXTURE_2D, tex);
            ifmt = 0;
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &ifmt);
            glBindTexture(GL_TEXTURE_2D, (GLuint)bound);
            glActiveTexture((GLenum)active);
        }
        fbo = false;
        const char* why = "no-framebuffer-objects";
        GLenum err = GL_NO_ERROR;
        if (!gl_readback_rgba8(ifmt)) {
            why = "format";
        } else if (fx_gl_ready && fx_glGenFramebuffers && fx_glBindFramebuffer && fx_glFramebufferTexture2D
                   && fx_glCheckFramebufferStatus) {
            /* whatever an earlier call left in the queue is not this reader's */
            for (int e = 0; e < 64 && glGetError() != GL_NO_ERROR; e++) { }
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFb);
            if (g_glReadbackContext != SDL_GL_GetCurrentContext()) g_glReadbackFbo = 0;
            if (!g_glReadbackFbo) {
                g_glReadbackContext = SDL_GL_GetCurrentContext();
                fx_glGenFramebuffers(1, &g_glReadbackFbo);
                fx_glBindFramebuffer(GL_FRAMEBUFFER, g_glReadbackFbo);
                glReadBuffer(GL_COLOR_ATTACHMENT0);
            } else {
                fx_glBindFramebuffer(GL_FRAMEBUFFER, g_glReadbackFbo);
            }
            fx_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
            fbo = fx_glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
            why = "incomplete";
            if (fbo && (err = glGetError()) != GL_NO_ERROR) { fbo = false; why = "attach-error"; }
            if (fbo && gl_readback_inject("incomplete")) fbo = false;
            if (!fbo) {
                fx_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
                fx_glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFb);
            }
        }
        if (!fbo) fall_back(why, err);
        else if (gl_readback_verify()) read_whole_into(check);
        memledger_add("readback_banded", 1, MEMLEDGER_COUNT);
        return true;
    }

    /* rows [y0, y0 + n) of level 0, tightly packed, into out (n * w * bpp bytes) */
    void rows(int y0, int n, unsigned char* out)
    {
        if (!open || n <= 0) return;
        if (y0 < 0) { out += (size_t)(-y0) * w * bpp; n += y0; y0 = 0; }
        if (y0 + n > h) n = h - y0;
        if (n <= 0) return;
        if (fbo) {
            /* an injected failure asks for format 0, which a driver must refuse */
            const GLenum asked = gl_readback_inject("error") ? (GLenum)0 : format;
            glReadPixels(0, y0, w, n, asked, GL_UNSIGNED_BYTE, out);
            const GLenum err = glGetError();
            if (err != GL_NO_ERROR) fall_back("read-error", err);
            else memledger_add("readback_bands", 1, MEMLEDGER_COUNT);
        }
        if (fbo && format == GL_ALPHA && !alphaChecked) {
            /* the band's first row once more as RGBA: its alpha must be what was just read */
            alphaChecked = true;
            std::vector<unsigned char> rgba((size_t)w * 4);
            glReadPixels(0, y0, w, 1, GL_RGBA, GL_UNSIGNED_BYTE, &rgba[0]);
            const GLenum err = glGetError();
            bool same = err == GL_NO_ERROR;
            for (int x = 0; same && x < w; x++) same = rgba[(size_t)x * 4 + 3] == out[x];
            if (same && gl_readback_inject("alpha")) same = false;
            if (!same) fall_back(err != GL_NO_ERROR ? "alpha-check-error" : "alpha-mismatch", err);
        }
        if (!fbo) {
            memcpy(out, &whole[(size_t)y0 * w * bpp], (size_t)n * w * bpp);
        } else if (!check.empty()) {
            const size_t stride = (size_t)w * bpp;
            verifyBands++;
            if (memcmp(out, &check[(size_t)y0 * stride], (size_t)n * stride)) {
                verifyBad++;
                for (int r = 0; r < n && verifyFirst < 0; r++)
                    if (memcmp(out + (size_t)r * stride, &check[(size_t)(y0 + r) * stride], stride))
                        verifyFirst = y0 + r;
            }
        }
    }

    void end(void)
    {
        if (!open) return;
        if (fbo) {
            fx_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
            fx_glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFb);
            if (gl_readback_verify())
                fprintf(stderr, "READBACK|verify|tex=%u|%dx%d|format=0x%x|bands=%lld|bad=%lld|first_bad_row=%d\n",
                        tex, w, h, (unsigned)format, verifyBands, verifyBad, verifyFirst);
        }
        glPixelStorei(GL_PACK_ALIGNMENT, align);
        glPixelStorei(GL_PACK_ROW_LENGTH, rowLen);
        glPixelStorei(GL_PACK_SKIP_ROWS, skipRows);
        glPixelStorei(GL_PACK_SKIP_PIXELS, skipPix);
        std::vector<unsigned char>().swap(whole);
        std::vector<unsigned char>().swap(check);
        open = fbo = false;
    }
};

/* The whole of level 0 of `tex` (w x h texels) as `format`, read `rows` rows at a time
   into out (w * h * texel bytes, tightly packed). */
static void gl_read_tex_rows(GLuint tex, int w, int h, GLenum format, unsigned char* out, int rows = 256)
{
    GlBandReader r;
    if (!r.begin(tex, w, h, format)) return;
    if (rows < 1) rows = 1;
    const size_t stride = (size_t)w * r.bpp;
    for (int y = 0; y < h; y += rows)
        r.rows(y, std::min(rows, h - y), out + (size_t)y * stride);
    r.end();
}

/* The read framebuffer belongs to the context; a mission's end gives it back. */
static void gl_readback_free(void)
{
    if (g_glReadbackFbo && fx_glDeleteFramebuffers && g_glReadbackContext == SDL_GL_GetCurrentContext())
        fx_glDeleteFramebuffers(1, &g_glReadbackFbo);
    g_glReadbackFbo = 0;
    g_glReadbackContext = NULL;
}

#endif
