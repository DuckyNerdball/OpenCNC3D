#!/usr/bin/env python3
"""Build 256px temperate cells from committed original high resolution artwork.

The original generated images supply the RGB image. Approved 128px cells only
supply cliff/road material footprints and narrow cliff/road exit guides.
No model call or 128px enlargement is used as the interior image source.
"""
from pathlib import Path
import json
import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[2]
GRASS = ROOT / 'art/generated/temperate_grass'
CLIFF = ROOT / 'art/generated/temperate_cliffs/approved_style_v1'
ROAD = ROOT / 'art/generated/temperate_roads/approved_style_v1'
OUT = ROOT / 'art/generated/temperate_256'
N = 256
LUMA = np.array([.21, .72, .07], dtype=np.float32)


def rgb(path, size=None):
    image = Image.open(path).convert('RGB')
    if size and image.size != size:
        image = image.resize(size, Image.Resampling.LANCZOS)
    return np.asarray(image, dtype=np.float32)


def gray(path):
    return np.asarray(Image.open(path).convert('L').resize((N, N), Image.Resampling.BICUBIC), dtype=np.float32)


def blur(a, radius):
    mode = 'L' if a.ndim == 2 else 'RGB'
    image = Image.fromarray(np.uint8(np.clip(np.rint(a), 0, 255)), mode)
    return np.asarray(image.filter(ImageFilter.GaussianBlur(radius)), dtype=np.float32)


def smoothstep(a, b, x):
    t = np.clip((x-a)/(b-a), 0, 1)
    return t*t*(3-2*t)


def seam(source, guide, width=8):
    """Keep approved cell exits while leaving nearly all RGB from source."""
    h, w = source.shape[:2]
    y, x = np.ogrid[:h, :w]
    distance = np.minimum(np.minimum(x, w-1-x), np.minimum(y, h-1-y))
    weight = smoothstep(0, width, distance)
    if source.ndim == 3:
        weight = weight[..., None]
    return source*weight + guide*(1-weight)


def periodic_patch(a, width=16):
    """Keep source detail while giving every opposite edge the same texels."""
    a = a.copy()
    for d in range(width):
        w = 1-smoothstep(0, width, d)
        shared = (a[:, d].copy()+a[:, -1-d].copy())*.5
        a[:, d] += (shared-a[:, d])*w
        a[:, -1-d] += (shared-a[:, -1-d])*w
        shared = (a[d].copy()+a[-1-d].copy())*.5
        a[d] += (shared-a[d])*w
        a[-1-d] += (shared-a[-1-d])*w
    a[:, -1] = a[:, 0]
    a[-1] = a[0]
    return a


def road_edge_material():
    """The approved periodic gravel shared by all Temperate road exits."""
    reference = rgb(ROAD/'D02/D02_assembled.png', (2*N, 2*N))
    reference_road = mask(reference, 'road') > .6
    edge_mean = reference[reference_road].mean(axis=0)
    d15_source = rgb(ROAD/'D15/source_gpt_image.png', (3*N, 3*N))
    edge_patch = d15_source[448:448+N, 160:160+N]
    return periodic_patch(edge_patch-edge_patch.mean(axis=(0, 1))+edge_mean), edge_mean


def road_gravel_material():
    """Grass-free D15 gravel detail, graded to the shared road exit colour."""
    _, edge_mean = road_edge_material()
    d15 = rgb(ROAD/'D15/source_gpt_image.png', (3*N, 3*N))
    patch = d15[256:256+N, 128:128+N].copy()
    green = smoothstep(3, 12, patch[..., 1]-patch[..., 0]) * \
            smoothstep(3, 12, patch[..., 1]-patch[..., 2])
    detail = patch-blur(patch, 12)
    patch = patch*(1-green[..., None]) + \
            (patch[green < .2].mean(axis=0)+detail*.55)*green[..., None]
    patch = edge_mean + (patch-blur(patch, 16))*.88
    return periodic_patch(patch, 5)


def join_internal_edges(a, columns, rows, icons, feather=4):
    """Give adjacent icons one shared edge without blurring their interiors."""
    a = a.copy()
    used = set(icons)
    channels = (a.ndim == 3)
    weight = smoothstep(0, feather-1, np.arange(feather, dtype=np.float32))
    if channels:
        weight = weight[None, :, None]
    else:
        weight = weight[None, :]
    for row in range(rows):
        for column in range(1, columns):
            left, right = row*columns+column-1, row*columns+column
            if left not in used or right not in used:
                continue
            x, y = column*N, row*N
            common = (a[y:y+N, x-1].copy()+a[y:y+N, x].copy())*.5
            if channels:
                common = common[:, None, :]
            else:
                common = common[:, None]
            a[y:y+N, x-feather:x] += (common-a[y:y+N, x-feather:x])*weight
            a[y:y+N, x:x+feather] += (common-a[y:y+N, x:x+feather])*weight[:, ::-1]
    if channels:
        weight = weight.transpose(1, 0, 2)
    else:
        weight = weight.T
    for row in range(1, rows):
        for column in range(columns):
            top, bottom = (row-1)*columns+column, row*columns+column
            if top not in used or bottom not in used:
                continue
            x, y = column*N, row*N
            common = (a[y-1, x:x+N].copy()+a[y, x:x+N].copy())*.5
            if channels:
                common = common[None, :, :]
            else:
                common = common[None, :]
            a[y-feather:y, x:x+N] += (common-a[y-feather:y, x:x+N])*weight
            a[y:y+feather, x:x+N] += (common-a[y:y+feather, x:x+N])*weight[::-1]
    return a


def mask(base, kind):
    r, g = base[..., 0], base[..., 1]
    if kind == 'road':
        return smoothstep(-3, 8, r-g)*smoothstep(35, 65, r)
    return smoothstep(-2, 8, r-g)*smoothstep(35, 68, r)


def save(path, a, mode):
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(np.uint8(np.clip(np.rint(a), 0, 255)), mode).save(path)


def normal(height, detail, material, micro):
    # Macro height follows approved connected edges; high resolution image detail
    # creates the micro normals used by real-time lighting.
    world = (blur(height, 2)-128)/127*.36
    world += (detail @ LUMA)*material*micro
    dy, dx = np.gradient(world, 1/255, 1/255)
    n = np.stack((-dx, np.ones_like(dx), -dy), axis=-1)
    n /= np.maximum(np.linalg.norm(n, axis=-1, keepdims=True), 1e-6)
    return n*127.5+127.5


def grass_grade(source):
    return (source - [53.8, 69.9, 35.9])*[.76, .76, .78]+[44.5, 58.1, 43.4]


def rock_grade(source):
    grass = smoothstep(4, 18, source[..., 1]-source[..., 0])[..., None]
    stone = (source-[63.9, 69.2, 43.9])*[1.19, 1.21, 1.16]+[58, 61, 45]
    return stone*(1-grass)+grass_grade(source)*grass


def main():
    grass_source = grass_grade(rgb(GRASS/'v1/source_gpt_image.png', (4*N, 4*N)))
    # Keep the source's original 4x4 image continuous. The former per-cell
    # 8px blend to the 128px guide flattened grass detail into visible lines.
    # Only the outside of the full 4x4 field needs to wrap when it repeats.
    grass_source = periodic_patch(grass_source, 3)
    grass_tiles = []
    for icon in range(16):
        x, y = (icon%4)*N, (icon//4)*N
        color = grass_source[y:y+N, x:x+N]
        grass_tiles.append(color)
        save(OUT/'grass'/f'CLEAR1_{icon:02}.png', color, 'RGB')

    cliff_count = 0
    for number in range(1, 39):
        name = f'S{number:02}'
        folder = CLIFF/'color_matched'/name
        with Image.open(folder/f'{name}_assembled.png') as image:
            cols, rows = image.width//128, image.height//128
        shape = (cols*N, rows*N)
        source_path = (CLIFF/name/'source_gpt_image.png' if number != 3 else
                       ROOT/'art/generated/temperate_cliffs/s03/v5/source_gpt_image.png')
        source = rock_grade(rgb(source_path, shape))
        for tile in sorted(folder.glob(f'{name}_??.png')):
            icon = int(tile.stem[-2:])
            x, y = (icon%cols)*N, (icon//cols)*N
            # The matched 128px tile defines the approved cliff outline only.
            guide = rgb(tile, (N, N))
            coverage = mask(guide, 'cliff')
            grass = grass_tiles[icon%16]
            color = source[y:y+N, x:x+N]*coverage[..., None]+grass*(1-coverage[..., None])
            color = seam(color, guide, 3)
            stem = f'{name}_{icon:02}'
            target = OUT/'cliffs'/name
            height_base = gray(folder/f'{stem}_height.png')
            detail = color-blur(color, 3)
            height = np.clip(height_base+(detail @ LUMA)*coverage*.14, 0, 255)
            save(target/f'{stem}.png', color, 'RGB')
            save(target/f'{stem}_height.png', height, 'L')
            save(target/f'{stem}_normal.png', normal(height, detail, coverage, .00008), 'RGB')
            cliff_count += 1

    # All separately authored road templates share this edge material. The
    # interior of each remains its own original generated high-resolution art.
    edge_patch, edge_mean = road_edge_material()
    manifest = json.loads((ROAD/'manifest.json').read_text())
    road_count = 0
    for name, info in manifest.items():
        width, height_cells = info['width'], info['height']
        side = max(width, height_cells)
        shape = (side*N, side*N)
        own = ROAD/name/'source_gpt_image.png'
        source_path = own if own.exists() else (ROAD/'D15/source_gpt_image.png' if side >= 3 else
                                                   ROOT/'art/generated/temperate_roads/d02_gravel/source_gpt_image.png')
        # Work on the entire source/template, never on separate icons. A road
        # image and its normal field must continue through internal cell edges.
        source = rgb(source_path, shape)[:height_cells*N, :width*N]
        guide = rgb(ROAD/name/f'{name}_assembled.png', shape)[:height_cells*N, :width*N]
        coverage = join_internal_edges(mask(guide, 'road'), width, height_cells, info['icons'])
        source_road = mask(source, 'road') > .5
        target_road = coverage > .5
        selected = source_road & target_road
        if selected.sum() < 64:
            selected = source_road
        if selected.sum() < 64:
            raise ValueError(f'{name}: original source has no gravel')
        target_mean = edge_mean
        source_mean = source[selected].mean(axis=0)
        graded = source-source_mean+target_mean
        # A generated source may put grass under an approved road footprint.
        # Replace those pixels with fine source-image variation around gravel's
        # mean, without carrying the grass's dark green value into a black slot.
        luma = source @ LUMA
        fine_source = luma-blur(luma, 18)
        gravel_fill = target_mean+fine_source[..., None]*[.9, .9, .8]
        confidence = smoothstep(.25, .75, mask(source, 'road'))[..., None]
        gravel = graded*confidence+gravel_fill*(1-confidence)
        grass = grass_source[:height_cells*N, :width*N]
        color = gravel*coverage[..., None]+grass*(1-coverage[..., None])
        # Match all road exits to one original-source gravel strip. Grassy
        # borders retain the approved edge pixel, while road-to-road joins no
        # longer change color every time two different templates meet. The
        # transition is three pixels; a wider blend reads as a blurry grid.
        common_gravel = np.tile(edge_patch, (height_cells, width, 1))
        edge_target = common_gravel*coverage[..., None]+guide*(1-coverage[..., None])
        color = join_internal_edges(
            seam(color, edge_target, 3), width, height_cells, info['icons'])
        height_base = np.full((height_cells*N, width*N), 128, dtype=np.float32)
        rough_base = np.full_like(height_base, 255)
        for icon in info['icons']:
            x, y = (icon%width)*N, (icon//width)*N
            box = np.s_[y:y+N, x:x+N]
            original = ROAD/name/f'{name}_{icon:02}'
            height_base[box] = gray(Path(str(original)+'_height.png'))
            rough_base[box] = gray(Path(str(original)+'_roughness.png'))
        detail = color-blur(color, 3)
        fine = detail @ LUMA
        height = join_internal_edges(
            seam(np.clip(blur(height_base, 2)+fine*coverage*.17, 0, 255),
                 128-8*coverage, 18), width, height_cells, info['icons'])
        rough = join_internal_edges(
            rough_base-fine*coverage*.32, width, height_cells, info['icons'])
        normals = normal(height, detail, coverage, .0001)
        for icon in info['icons']:
            x, y = (icon%width)*N, (icon//width)*N
            box = np.s_[y:y+N, x:x+N]
            stem = f'{name}_{icon:02}'
            target_dir = OUT/'roads'/name
            save(target_dir/f'{stem}.png', color[box], 'RGB')
            save(target_dir/f'{stem}_height.png', height[box], 'L')
            save(target_dir/f'{stem}_normal.png', normals[box], 'RGB')
            save(target_dir/f'{stem}_roughness.png', rough[box], 'L')
            road_count += 1

    if (cliff_count, road_count) != (157, 206):
        raise ValueError(f'expected 157 cliffs/206 roads, got {cliff_count}/{road_count}')
    print(f'256px original-source cells: 16 grass, {cliff_count} cliffs, {road_count} roads')


if __name__ == '__main__':
    main()
