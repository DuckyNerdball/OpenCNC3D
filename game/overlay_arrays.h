/* ---------------------------------------------------------------------------------- *
 *  GROUND OVERLAYS: IMMEDIATE MODE, OR THE SAME VERTICES DRAWN FROM ARRAYS.
 *
 *  The shroud blanket, the mottle patches, and the tiberium cracks and contact patches
 *  are glBegin/glEnd loops: a glColor4f, a glTexCoord2f and a glVertex3f for every
 *  vertex, up to three driver calls a vertex and tens of thousands a frame. They write
 *  through an OverlayArrays, which by default forwards every call to GL as it comes, so
 *  the default draw IS the immediate-mode loop, call for call.
 *
 *  With the overlay_arrays dial on, the same calls are collected instead and every
 *  glBegin/glEnd block is handed over as one glDrawArrays of the same primitive over the
 *  same vertices, so the rasteriser is given identical triangles with identical
 *  attributes and the picture does not change. That takes the GL calls of a frame down
 *  by about a third on a base view, but it has not made a frame faster anywhere it was
 *  measured. On the Mac the sections it touches move by a tenth of a millisecond, within
 *  their noise. On an NVIDIA card under Windows the per-call cost of these loops turned
 *  out to be small: the mottle and tiberium sections get a few hundredths of a
 *  millisecond cheaper and the frame does not. A loss of about a millisecond on the
 *  desert base, seen while the immediate-mode build always ran first and last in each
 *  round, was the size of the drift between those positions in runs of one and the same
 *  build, and was not there when the two alternated. That is why the dial is off by
 *  default and sits in the DEBUG group: it keeps the array path buildable and
 *  measurable, and it is not a speed setting.
 *
 *  The batch keeps immediate mode's own rules, so a loop reads the same either way:
 *  color() and texcoord() set a CURRENT value, vertex() appends a position together
 *  with whatever is current exactly as glVertex3f does, and bind_texture(), begin() and
 *  end() stand where glBindTexture, glBegin and glEnd stood. In the array path nothing
 *  reaches GL until flush(), which draws every block since the last flush in order,
 *  binding each block's texture first. A caller flushes before it changes any other
 *  state, because the blocks are drawn under the state current at the flush.
 *
 *  Two things immediate mode did implicitly are done explicitly in the array path:
 *
 *   - CLIENT ARRAY STATE. The arrays are switched on inside a glPushClientAttrib /
 *     glPopClientAttrib pair, and every array the batch does not carry is switched off
 *     for the draw (the same set imm_draw_triangles and the terrain cache switch off):
 *     an array an earlier pass left enabled would otherwise be read too, through a
 *     pointer that may be shorter than this batch. A bound GL_ARRAY_BUFFER is unbound
 *     for the draw, because a client pointer is read as an offset into it. Popping
 *     restores all of it, so no later pass sees a difference.
 *   - THE CURRENT COLOUR AND TEXTURE COORDINATE. After glEnd they hold the last values
 *     the loop set; after a glDrawArrays from an enabled array they are undefined. The
 *     batch remembers the last values set and flush() puts them back.
 *
 *  A batch that carries colour must set it before its first vertex (every loop that
 *  uses one does), because the array has no "colour from before the batch" to fall back
 *  on the way immediate mode does.
 *
 *  GL 1.1 entry points only (client arrays, GL_QUADS included), plus the multitexture
 *  client selector and the buffer unbind the rest of the renderer already uses.
 * ---------------------------------------------------------------------------------- */
#ifndef OPENCNC3D_OVERLAY_ARRAYS_H
#define OPENCNC3D_OVERLAY_ARRAYS_H

struct OverlayArrays {
    bool imm = true;                    /* forward every call to GL as it comes */
    bool withColour = false, withTex = false;
    bool colourSet = false, texSet = false;
    float c[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float t[2] = { 0.0f, 0.0f };
    int n = 0;                          /* vertices in the batch */
    /* Storage, not contents: it only grows, and n says how much of it is in use. Every
       stream the batch carries holds room for the same number of vertices. A vertex is
       then a plain copy of its floats, where a push_back or a range insert per vertex
       cost more on the Mac than the immediate-mode calls it replaced. */
    std::vector<float> pos, col, tex;
    /* The blocks not yet drawn: one per begin()/end() pair, with the texture bound
       before it (bindTex says whether one was asked for). */
    struct Block { GLenum mode; int first, n; bool bindTex; GLuint tex; };
    std::vector<Block> blocks;
    size_t drawn = 0;                   /* blocks already handed to GL by flush() */
    bool pendingBind = false;
    GLuint pendingTex = 0;

    /* Starts an empty batch and picks the path from the dial. The storage is kept, so
       a batch held in a static is not reallocated frame after frame. */
    void reset(bool colour, bool texcoord)
    {
        imm = g_fx.overlay_arrays == 0;
        withColour = colour; withTex = texcoord;
        colourSet = texSet = false;
        n = 0;
        blocks.clear();
        drawn = 0;
        pendingBind = false;
        if (imm) return;
        const size_t cap = pos.size() / 3;
        if (withColour && col.size() < cap * 4) col.resize(cap * 4);
        if (withTex && tex.size() < cap * 2)    tex.resize(cap * 2);
    }
    int count() const { return n; }
    void bind_texture(GLuint name)
    {
        if (imm) { glBindTexture(GL_TEXTURE_2D, name); return; }
        pendingBind = true; pendingTex = name;
    }
    void begin(GLenum mode)
    {
        if (imm) { glBegin(mode); return; }
        blocks.push_back({ mode, n, 0, pendingBind, pendingTex });
        pendingBind = false;
    }
    void end(void)
    {
        if (imm) { glEnd(); return; }
        blocks.back().n = n - blocks.back().first;
    }
    void color(float r, float g, float b, float a)
    {
        if (imm) { glColor4f(r, g, b, a); return; }
        c[0] = r; c[1] = g; c[2] = b; c[3] = a; colourSet = true;
    }
    void texcoord(float s, float tt)
    {
        if (imm) { glTexCoord2f(s, tt); return; }
        t[0] = s; t[1] = tt; texSet = true;
    }
    void vertex(float x, float y, float z)
    {
        if (imm) { glVertex3f(x, y, z); return; }
        const size_t i = (size_t)n;
        if (pos.size() < (i + 1) * 3) grow(i + 1);
        float* p = &pos[i * 3];
        p[0] = x; p[1] = y; p[2] = z;
        if (withColour) {
            float* q = &col[i * 4];
            q[0] = c[0]; q[1] = c[1]; q[2] = c[2]; q[3] = c[3];
        }
        if (withTex) {
            float* q = &tex[i * 2];
            q[0] = t[0]; q[1] = t[1];
        }
        n++;
    }
    /* Room for at least `need` vertices in every stream the batch carries, doubling, so
       a resize happens a handful of times in a batch's life and never per vertex. */
    void grow(size_t need)
    {
        size_t cap = pos.size() / 3;
        cap = cap ? cap * 2 : 1024;
        while (cap < need) cap *= 2;
        pos.resize(cap * 3);
        if (withColour) col.resize(cap * 4);
        if (withTex)    tex.resize(cap * 2);
    }
    void flush(void);
};

/* The binding the arrays found, so overlay_arrays_end can say it is known again. */
static GLuint g_overlayArraysBuffer = 0;

/* Points GL's client arrays at the batch, with nothing else enabled. Every draw of
   the batch goes between this and overlay_arrays_end. */
static void overlay_arrays_begin(const OverlayArrays& a)
{
    typedef void (APIENTRY *BindBuffer)(GLenum, GLuint);
    static BindBuffer bindBuffer = (BindBuffer)SDL_GL_GetProcAddress("glBindBuffer");
    gls_texture_limits();
    g_overlayArraysBuffer = gls_array_binding();
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    if (g_overlayArraysBuffer && bindBuffer) bindBuffer(GL_ARRAY_BUFFER, 0);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_INDEX_ARRAY);
    glDisableClientState(GL_EDGE_FLAG_ARRAY);
    for (int unit = 1; unit < g_glsClientLimit; ++unit) {
        glClientActiveTexture(GL_TEXTURE0 + unit);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    glClientActiveTexture(GL_TEXTURE0);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, a.pos.empty() ? NULL : &a.pos[0]);
    if (a.withColour) {
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(4, GL_FLOAT, 0, a.col.empty() ? NULL : &a.col[0]);
    } else {
        glDisableClientState(GL_COLOR_ARRAY);
    }
    if (a.withTex) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, a.tex.empty() ? NULL : &a.tex[0]);
    } else {
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
}

/* Restores every client array, and the buffer binding, to what overlay_arrays_begin
   found. The pop already puts the binding back; it is bound again by name anyway, as
   the other array paths do, and then the state cache knows it without asking. */
static void overlay_arrays_end(void)
{
    typedef void (APIENTRY *BindBuffer)(GLenum, GLuint);
    static BindBuffer bindBuffer = (BindBuffer)SDL_GL_GetProcAddress("glBindBuffer");
    glPopClientAttrib();
    if (g_overlayArraysBuffer && bindBuffer) bindBuffer(GL_ARRAY_BUFFER, g_overlayArraysBuffer);
    g_glsArray = g_overlayArraysBuffer;
    g_glsArrayKnown = true;
}

/* Draws every block since the last flush, in order, each after its texture bind, then
   puts back the current colour and texture coordinate the immediate loop would have
   left. A block that drew no vertices still binds its texture, as its glBindTexture
   did before an empty glBegin/glEnd, and the client arrays are not set up at all when
   no block has a vertex. Nothing to do in immediate mode, where every call has
   already reached GL. */
inline void OverlayArrays::flush(void)
{
    if (imm) return;
    bool any = false;
    for (size_t i = drawn; i < blocks.size(); ++i)
        if (blocks[i].n > 0) any = true;
    if (any) overlay_arrays_begin(*this);
    for (; drawn < blocks.size(); ++drawn) {
        const Block& b = blocks[drawn];
        if (b.bindTex) glBindTexture(GL_TEXTURE_2D, b.tex);
        if (b.n > 0) glDrawArrays(b.mode, b.first, b.n);
    }
    if (any) overlay_arrays_end();
    if (colourSet) glColor4f(c[0], c[1], c[2], c[3]);
    if (texSet)    glTexCoord2f(t[0], t[1]);
}

#endif /* OPENCNC3D_OVERLAY_ARRAYS_H */
