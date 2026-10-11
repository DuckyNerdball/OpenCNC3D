/* ================================================================================== *
 *  sky3d_mod.h -- the sky the authored models are lit by, from sky.pack.
 *
 *  WHAT THIS IS
 *
 *  A cube map and two convolutions of it, baked by game/bake_sky.py and read here. It
 *  is not drawn: nothing in this file puts a pixel on the screen. It exists so that the
 *  authored-model shader can answer the half of its lighting equation that is not the
 *  sun, with a measurement of an actual sky instead of a guess.
 *
 *  WHAT IT REPLACES, AND WHY THE GUESS WAS NOT GOOD ENOUGH. The shader's ambient was a
 *  sky colour from above and a bounce colour from below, blended on the surface normal.
 *  For a diffuse surface that is a reasonable approximation of a clear day. For a METAL
 *  it is not an approximation of anything: metal has no diffuse term, so what it shows
 *  is whatever it reflects, and a two-colour blend gives it two colours to reflect. The
 *  roughness floor dial exists purely to stop polished texels reading as black dots,
 *  which is the guess admitting in public that it was one.
 *
 *  THE TWO HALVES, and they are stored differently on purpose:
 *
 *    THE ROUGHNESS CHAIN, as cube mip levels. Level 0 is the sky, each level after it
 *    is the sky blurred by the reflection cone of one roughness, and the shader picks a
 *    level instead of integrating a lobe per pixel. This is the standard split-sum
 *    prefilter; the baker carries the other half of the explanation.
 *
 *    THE IRRADIANCE, as NINE SPHERICAL-HARMONIC COEFFICIENTS rather than a second cube.
 *    Irradiance is smooth by construction -- it is the whole sky averaged under a cosine
 *    lobe -- so a texture to hold it would store almost nothing at a resolution where
 *    its cube seams are visible, and would cost a texture unit to sample. Nine numbers
 *    reconstruct it to within a percent and evaluate in nine multiply-adds. The cosine
 *    convolution is folded into the coefficients by the baker, so what arrives here is
 *    already irradiance and this file cannot get that wrong.
 *
 *  ENHANCED ONLY, like everything it feeds. Nothing here is reached with the chain off.
 *
 *  TIER 1: ABSENT, and completely so. A cube map is OpenGL 1.3 and Glide has no such
 *  thing at any texture size; the shader that would sample it does not exist there
 *  either. Win98 draws the cartridge's own models with the console's baked lighting,
 *  which is the picture it has always had. See the row in docs/tier1-gap.md.
 *
 *  NO NEW GL ENTRY POINT. Cube maps added tokens and no functions: glBindTexture,
 *  glTexImage2D and glTexParameteri are all OpenGL 1.1 and take the new targets as
 *  arguments. The tokens arrive on Windows through the <GL/glext.h> that the compat
 *  header already includes. That is the whole reason this could be a small file.
 * ================================================================================== */
#ifndef OPENCNC3D_SKY3D_MOD_H
#define OPENCNC3D_SKY3D_MOD_H

/* ---- the file -----------------------------------------------------------------------
       char magic[8]   "CNC3DSKY"
       u32  version, faceSize, mips, shCount, reserved[4]
       f32  sh[shCount][4]      nine RGB coefficients, w unused, already convolved
       then mips x 6 faces of RGB8, each (faceSize >> level) square,
            in GL's own face order so they upload in a loop
   ---------------------------------------------------------------------------------- */

#define SKY3D_MAGIC "CNC3DSKY"

static GLuint g_sky3dTex   = 0;    /* the cube map, 0 when there is none    */
static int    g_sky3dMips  = 0;
static int    g_sky3dFace  = 0;
static float  g_sky3dSH[9][3];     /* irradiance, already cosine-convolved   */
static bool   g_sky3dHave  = false;

/* The highest mip index the shader may ask for. Handed to it rather than derived there,
   because a shader that guesses the level count of a texture it did not load is a
   shader that samples a level which does not exist the day the baker changes. */
static inline float sky3d_max_lod(void)
{
    return g_sky3dMips > 0 ? (float)(g_sky3dMips - 1) : 0.0f;
}

static void sky3d_load(const char* path)
{
    if (!path || !*path)
        return;
    FILE* f = fopen(path, "rb");
    if (!f) {
        /* Not an error and not worth a warning: a build without a sky pack draws the
           two-colour fill, which is a complete picture and was the only picture until
           recently. The observer line in the model dump is where its absence shows. */
        return;
    }
    char magic[8];
    unsigned h[8];
    if (fread(magic, 1, 8, f) != 8 || memcmp(magic, SKY3D_MAGIC, 8) != 0
        || fread(h, 4, 8, f) != 8) {
        fprintf(stderr, "sky: %s is not a sky pack\n", path);
        fclose(f);
        return;
    }
    if (h[0] != 1) {
        fprintf(stderr, "sky: %s is format %u and this build reads 1 -- re-run "
                        "game/bake_sky.py\n", path, h[0]);
        fclose(f);
        return;
    }
    const int face = (int)h[1], mips = (int)h[2], nsh = (int)h[3];
    if (face < 4 || mips < 1 || mips > 16 || nsh != 9) {
        fprintf(stderr, "sky: %s has %d mips of %d and %d harmonics, which is outside "
                        "what this reads\n", path, mips, face, nsh);
        fclose(f);
        return;
    }
    for (int i = 0; i < 9; i++) {
        float c[4];
        if (fread(c, 4, 4, f) != 4) { fclose(f); return; }
        g_sky3dSH[i][0] = c[0]; g_sky3dSH[i][1] = c[1]; g_sky3dSH[i][2] = c[2];
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    /* THE LEVEL RANGE IS DECLARED, not left to the driver to work out. The chain stops
       at four texels a side rather than at one, so without this a driver is entitled to
       call the texture incomplete and sample black -- which looks exactly like a sky
       that failed to load and is a miserable thing to debug. */
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, mips - 1);

    /* THREE CHANNELS. A sky has no alpha and the baker does not write one; reading
       four here is what made a 1.5 MB pack look truncated at the first level. */
    std::vector<unsigned char> buf;
    bool ok = true;
    for (int m = 0; m < mips && ok; m++) {
        const int n = face >> m;
        buf.resize((size_t)n * n * 3);
        for (int fi = 0; fi < 6 && ok; fi++) {
            if (fread(&buf[0], 1, buf.size(), f) != buf.size()) { ok = false; break; }
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + fi, m, GL_RGB8, n, n, 0,
                         GL_RGB, GL_UNSIGNED_BYTE, &buf[0]);
        }
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    fclose(f);

    if (!ok) {
        fprintf(stderr, "sky: %s ends early -- truncated, or the baker is still "
                        "writing it\n", path);
        glDeleteTextures(1, &tex);
        return;
    }
    g_sky3dTex  = tex;
    g_sky3dMips = mips;
    g_sky3dFace = face;
    g_sky3dHave = true;
    printf("SKY|%s|%d faces of %d|%d levels|ambient %.2f %.2f %.2f\n",
           path, 6, face, mips, g_sky3dSH[0][0], g_sky3dSH[0][1], g_sky3dSH[0][2]);
    fflush(stdout);
}

/* Called from game_shutdown, because game_boot loads the sky every mission and a second
   load would otherwise orphan the first cube map rather than replace it. */
static void sky3d_free(void)
{
    if (g_sky3dTex)
        glDeleteTextures(1, &g_sky3dTex);
    g_sky3dTex  = 0;
    g_sky3dMips = 0;
    g_sky3dFace = 0;
    g_sky3dHave = false;
}

#endif /* OPENCNC3D_SKY3D_MOD_H */
