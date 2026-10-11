"""URP's Gaussian bloom and its ACES tonemap (LDR grading mode), ported from the package
source line for line, as the reference the renderer's GLSL is checked against.

Sources (com.unity.render-pipelines.universal 17.x / core 17.x):
  Shaders/PostProcessing/Bloom.shader       FragPrefilter, FragBlurH, FragBlurV, Upsample
  Runtime/Passes/PostProcess/BloomPostProcessPass.cs   resolution, mip count, the chain
  Shaders/PostProcessing/Common.hlsl        ApplyTonemap (LDR grading)
  ShaderLibrary/Color.hlsl, ACES.hlsl       AcesTonemap and its helpers

    python3 urp_post.py <frame.f32> <unity_post.png>   compares one frame
"""
import numpy as np

# ------------------------------------------------------------------ sampling
def sample(img, u, v):
    """Bilinear, clamp to edge. img[h, w, c] with row 0 at the BOTTOM (v = 0)."""
    h, w = img.shape[:2]
    x = u * w - 0.5; y = v * h - 0.5
    x0 = np.floor(x).astype(int); y0 = np.floor(y).astype(int)
    fx = (x - x0)[..., None]; fy = (y - y0)[..., None]
    x0c = np.clip(x0, 0, w - 1); x1c = np.clip(x0 + 1, 0, w - 1)
    y0c = np.clip(y0, 0, h - 1); y1c = np.clip(y0 + 1, 0, h - 1)
    a = img[y0c, x0c]; b = img[y0c, x1c]; c = img[y1c, x0c]; d = img[y1c, x1c]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy

def grid(w, h):
    u = (np.arange(w) + 0.5) / w; v = (np.arange(h) + 0.5) / h
    return np.meshgrid(u, v)

def clamp_uv(u, v, tw, th):
    """ClampUVForBilinear: keep the footprint inside the texture."""
    return np.clip(u, 0.5 * tw, 1 - 0.5 * tw), np.clip(v, 0.5 * th, 1 - 0.5 * th)

def sample_bicubic(img, u, v):
    """Core's SampleTexture2DBicubic: the cubic B-spline as four bilinear taps
    (BicubicFilter), maxCoord 1."""
    h, w = img.shape[:2]
    x = u * w + 0.5; y = v * h + 0.5
    icx, icy = np.floor(x), np.floor(y)
    fx, fy = x - icx, y - icy
    def wo(f):
        r = 0.16666667 + f * (-0.5 + f * (0.5 - f * 0.16666667))
        mr = 0.66666667 + f * (-1.0 + 0.5 * f) * f
        ml = 0.16666667 + f * (0.5 + f * (0.5 - f * 0.5))
        l = 1.0 - mr - ml - r
        w0, w1 = r + mr, ml + l
        return w0, w1, -1.0 + mr / w0, 1.0 + l / w1
    wx0, wx1, ox0, ox1 = wo(fx)
    wy0, wy1, oy0, oy1 = wo(fy)
    def tap(ox, oy):
        uu = np.minimum((icx + ox - 0.5) / w, 1.0)
        vv = np.minimum((icy + oy - 0.5) / h, 1.0)
        return sample(img, uu, vv)
    return (wy0[..., None] * (wx0[..., None] * tap(ox0, oy0) + wx1[..., None] * tap(ox1, oy0)) +
            wy1[..., None] * (wx0[..., None] * tap(ox0, oy1) + wx1[..., None] * tap(ox1, oy1)))

# ------------------------------------------------------------------ bloom
# THE FILTER. The reference project's render pipeline asset carries a volume profile
# (Assets/Settings/SampleSceneProfile) that sets bloom scatter 0.5 -- which is where the
# 0.5 fitted here comes from -- and asks for high quality filtering (the 13 tap prefilter,
# bicubic upsampling). Measured against Unity's own frames, the plain filter fits better
# (explosion frames 0.53 / 1.5 levels mean / p99 against 0.76 / 2.0 with hq), so plain is
# the default and hq stays here only as the measured alternative.
def bloom(src, threshold=1.0, intensity=1.0, scatter_param=0.5, clampmax=65472.0,
          max_iter=6, hq=False):
    H, W = src.shape[:2]
    tw, th = max(1, W >> 1), max(1, H >> 1)
    mip_count = int(np.clip(int(np.floor(np.log2(max(tw, th)) - 1)), 1, max_iter))
    thr = threshold                     # GammaToLinearSpace(1) == 1
    knee = thr * 0.5
    scatter = 0.05 + (0.95 - 0.05) * scatter_param
    sizes = [(tw, th)]
    for i in range(1, mip_count):
        sizes.append((max(1, sizes[-1][0] >> 1), max(1, sizes[-1][1] >> 1)))
    # prefilter
    u, v = grid(tw, th)
    if hq:
        sw_, sh_ = 1.0 / W, 1.0 / H
        S = lambda ox, oy: sample(src, u + ox * sw_, v + oy * sh_)
        A, B, C = S(-1, -1), S(0, -1), S(1, -1)
        D, E = S(-0.5, -0.5), S(0.5, -0.5)
        F, G, Hh = S(-1, 0), S(0, 0), S(1, 0)
        I, J = S(-0.5, 0.5), S(0.5, 0.5)
        K, L, M = S(-1, 1), S(0, 1), S(1, 1)
        d0, d1 = 0.25 * 0.5, 0.25 * 0.125
        c = (D + E + I + J) * d0 + (A + B + G + F) * d1 + (B + C + Hh + G) * d1 \
            + (F + G + L + K) * d1 + (G + Hh + M + L) * d1
    else:
        c = sample(src, u, v)
    c = np.minimum(clampmax, c)
    br = c.max(-1, keepdims=True)
    soft = np.clip(br - thr + knee, 0.0, 2.0 * knee)
    soft = soft * soft / (4.0 * knee + 1e-4)
    mult = np.maximum(br - thr, soft) / np.maximum(br, 1e-4)
    down = [np.maximum(c * mult, 0)]
    up = [None] * mip_count
    wH = [0.01621622, 0.05405405, 0.12162162, 0.19459459, 0.22702703,
          0.19459459, 0.12162162, 0.05405405, 0.01621622]
    for i in range(1, mip_count):
        last = down[-1]
        sh, sw = last.shape[:2]
        w_, h_ = sizes[i]
        u, v = grid(w_, h_)
        tx = 2.0 / sw                    # _BlitTexture_TexelSize * 2 (source texel)
        acc = 0
        for k, wk in zip(range(-4, 5), wH):
            uu, vv = clamp_uv(u + k * tx, v, 1.0 / sw, 1.0 / sh)
            acc = acc + sample(last, uu, vv) * wk
        up[i] = acc
        ty = 1.0 / h_
        acc = 0
        for o, wk in ((-3.23076923, 0.07027027), (-1.38461538, 0.31621622), (0.0, 0.22702703),
                      (1.38461538, 0.31621622), (3.23076923, 0.07027027)):
            uu, vv = clamp_uv(u, v + o * ty, 1.0 / w_, 1.0 / h_)
            acc = acc + sample(up[i], uu, vv) * wk
        down.append(acc)
    for i in range(mip_count - 2, -1, -1):
        low = down[i + 1] if i == mip_count - 2 else up[i + 1]
        w_, h_ = sizes[i]
        u, v = grid(w_, h_)
        lowc = sample_bicubic(low, u, v) if hq else sample(low, u, v)
        up[i] = down[i] * (1 - scatter) + lowc * scatter
    res = down[0] if mip_count == 1 else up[0]
    u, v = grid(W, H)
    uu, vv = clamp_uv(u, v, 1.0 / res.shape[1], 1.0 / res.shape[0])
    return (sample_bicubic(res, uu, vv) if hq else sample(res, uu, vv)) * intensity

# ------------------------------------------------------------------ ACES
sRGB_2_AP0 = np.array([[0.4397010, 0.3829780, 0.1773350],
                       [0.0897923, 0.8134230, 0.0967616],
                       [0.0175440, 0.1115440, 0.8707040]])
AP0_2_AP1 = np.array([[1.4514393161, -0.2365107469, -0.2149285693],
                      [-0.0765537734, 1.1762296998, -0.0996759264],
                      [0.0083161484, -0.0060324498, 0.9977163014]])
AP1_2_XYZ = np.array([[0.6624541811, 0.1340042065, 0.1561876870],
                      [0.2722287168, 0.6740817658, 0.0536895174],
                      [-0.0055746495, 0.0040607335, 1.0103391003]])
XYZ_2_AP1 = np.array([[1.6410233797, -0.3248032942, -0.2364246952],
                      [-0.6636628587, 1.6153315917, 0.0167563477],
                      [0.0117218943, -0.0082844420, 0.9883948585]])
D60_2_D65 = np.array([[0.98722400, -0.00611327, 0.0159533],
                      [-0.00759836, 1.00186000, 0.0053302],
                      [0.00307257, -0.00509595, 1.0816800]])
XYZ_2_709 = np.array([[3.2409699419, -1.5373831776, -0.4986107603],
                      [-0.9692436363, 1.8759675015, 0.0415550574],
                      [0.0556300797, -0.2039769589, 1.0569715142]])
AP1_RGB2Y = np.array([0.272229, 0.674082, 0.0536895])

def mul(M, x):
    return x @ M.T

def aces_tonemap(lin):
    """ApplyTonemap(_TONEMAP_ACES): AcesTonemap(unity_to_ACES(x)), saturated."""
    aces = mul(sRGB_2_AP0, lin)
    r, g, b = aces[..., 0], aces[..., 1], aces[..., 2]
    mi = np.minimum(np.minimum(r, g), b); ma = np.maximum(np.maximum(r, g), b)
    sat = (np.maximum(ma, 1e-4) - np.maximum(mi, 1e-4)) / np.maximum(ma, 1e-2)
    k = np.maximum(b * (b - g) + g * (g - r) + r * (r - b), 0.0)
    yc = (b + g + r + 1.75 * np.sqrt(k)) / 3.0
    x = (sat - 0.4) / 0.2
    t = np.maximum(1.0 - np.abs(x / 2.0), 0.0)
    s = (1.0 + np.where(x >= 0, 1.0, -1.0) * (1.0 - t * t)) / 2.0
    gain = 0.05 * s; mid = 0.08
    glow = np.where(yc <= 2.0 / 3.0 * mid, gain,
                    np.where(yc >= 2.0 * mid, 0.0, gain * (mid / np.maximum(yc, 1e-9) - 0.5)))
    aces = aces * (1.0 + glow)[..., None]
    r, g, b = aces[..., 0], aces[..., 1], aces[..., 2]
    neutral = (r == g) & (g == b)
    hue = np.where(neutral, 0.0, np.degrees(np.arctan2(np.sqrt(3.0) * (g - b), 2.0 * r - g - b)))
    hue = np.where(hue < 0, hue + 360.0, hue)
    ch = hue - 0.0
    ch = np.where(ch < -180, ch + 360, np.where(ch > 180, ch - 360, ch))
    hw = np.clip(1.0 - np.abs(2.0 * ch / 135.0), 0, 1)
    hw = hw * hw * (3 - 2 * hw)          # smoothstep(0, 1, .)
    hw = hw * hw
    aces = aces.copy()
    aces[..., 0] = r + hw * sat * (0.03 - r) * (1.0 - 0.82)
    cg = np.maximum(0.0, mul(AP0_2_AP1, aces))
    y = (cg @ AP1_RGB2Y)[..., None]
    cg = y + (cg - y) * 0.96
    a_, b_, c_, d_, e_ = 0.0245786, 0.000090537, 0.983729, 0.4329510, 0.238081
    post = (cg * (cg + a_) - b_) / (cg * (c_ * cg + d_) + e_)
    XYZ = mul(AP1_2_XYZ, post)
    div = np.maximum(XYZ.sum(-1), 1e-4)
    xx, yy, Y = XYZ[..., 0] / div, XYZ[..., 1] / div, np.clip(XYZ[..., 1], 0, 65504.0)
    Y = np.power(Y, 0.9811)
    m = Y / np.maximum(yy, 1e-4)
    XYZ = np.stack([xx * m, Y, (1.0 - xx - yy) * m], -1)
    lcv = mul(XYZ_2_AP1, XYZ)
    y = (lcv @ AP1_RGB2Y)[..., None]
    lcv = y + (lcv - y) * 0.93
    XYZ = mul(D60_2_D65, mul(AP1_2_XYZ, lcv))
    return np.clip(mul(XYZ_2_709, XYZ), 0, 1)

def srgb_encode(x):
    x = np.clip(x, 0, 1)
    return np.where(x <= 0.0031308, x * 12.92, 1.055 * np.power(x, 1 / 2.4) - 0.055)

def load_f32(path):
    b = open(path, 'rb').read()
    w, h = np.frombuffer(b, '<u4', 2)
    return np.frombuffer(b, '<f4', w * h * 4, 8).reshape(h, w, 4)[..., :3].astype(np.float64)

def post(raw):
    return srgb_encode(aces_tonemap(raw + bloom(raw)))

if __name__ == '__main__':
    import sys
    from PIL import Image
    raw = load_f32(sys.argv[1])
    ours = post(raw)[::-1] * 255.0          # to top-down rows, like the PNG
    unity = np.asarray(Image.open(sys.argv[2]).convert('RGB')).astype(np.float64)
    h, w = unity.shape[:2]
    cy, cx = slice(h // 4, 3 * h // 4), slice(w // 4, 3 * w // 4)
    d = np.abs(ours - unity)[cy, cx]
    print(f'centre half: mean |diff| {d.mean():.2f} levels, p99 {np.percentile(d, 99):.1f}, max {d.max():.0f}; '
          f'unity mean {unity[cy, cx].mean():.1f} ours {ours[cy, cx].mean():.1f}')
    if len(sys.argv) > 3:
        Image.fromarray(np.clip(np.concatenate([unity, ours, np.abs(ours - unity) * 4], 1), 0, 255).astype(np.uint8)).save(sys.argv[3])
