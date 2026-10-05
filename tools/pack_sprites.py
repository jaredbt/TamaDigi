#!/usr/bin/env python3
"""Turns Digimon Color sprite sheets into the firmware's animated TPK2 sprites.

    python3 tools/pack_sprites.py           # fetch (once), then pack everything
    python3 tools/pack_sprites.py --offline # only use what is already cached

SOURCE ART
  The full-colour, animated dot sprites of Digital Monster COLOR, as published
  in furudbat/wayland-vpets (assets/dmc/<Name>.png, one horizontal sheet per
  Digimon) -- downloaded once into tools/sprites_src/ (gitignored). They are
  (c) Bandai -- see CREDITS.md. The column count of each sheet comes from that
  project's generated header (include/embedded_assets/dmc/dmc.hpp).

  The frames follow the V-Pet's own order:
     0 idle 1    1 idle 2    2 angry     3 down      4 happy    5 eat 1
     6 sleep     7 refuse    8 sad       9 lose 1   10 eat 2   11 lose 2
    12 attack   (a 9-column sheet stops after "sad"; missing ones fall back)

  Your own art wins: drop a sheet or a single frame into
  tools/sprites_src/extra/<name>.png (lower case display name, e.g.
  poyomon.png) -- any integer scale of a 16x16 dot grid.

WHAT IT WRITES
  tools/sdcard/mons/pNNN.bin    normal colours
  tools/sdcard/mons/psNNN.bin   the rare colour variant (hue-shifted)
  noart.h                       Digimon with no art: never hatched or offered

  Then run make_thumbs.py and pack_bundle.py (build_web.sh does all three).

LOOK
  The original colours, the original black outline, and a 1-px pale rim
  outside it so the outline still reads on the dark night sky. Upscaled x2 so
  the panel's integer zoom lands on ~160 px.
"""
import colorsys
import os
import re
import struct
import subprocess
import sys
from math import gcd

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import digimon_data as D

SRC = os.path.join(HERE, 'sprites_src', 'dmc')
EXTRA = os.path.join(HERE, 'sprites_src', 'extra')
OUT = os.path.join(HERE, 'sdcard', 'mons')
RAW = 'https://raw.githubusercontent.com/furudbat/wayland-vpets/main/'
UP = 2
RIM = (0xf4, 0xf6, 0xee)
VARIANT_HUE = 0.42      # how far the colour variant turns the hue wheel

F_IDLE1, F_IDLE2, F_ANGRY, F_DOWN, F_HAPPY, F_EAT1, F_SLEEP, F_REFUSE, F_SAD, \
    F_LOSE1, F_EAT2, F_LOSE2, F_ATTACK = range(13)


def curl(url, dst):
    if os.path.exists(dst) and os.path.getsize(dst) > 0:
        return True
    if '--offline' in sys.argv:
        return False
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    r = subprocess.run(['curl', '-fsSL', '--retry', '2', '-o', dst, url], capture_output=True)
    if r.returncode != 0 and os.path.exists(dst):
        os.remove(dst)
    return r.returncode == 0


def sheet_columns():
    """name -> columns, from wayland-vpets' generated header."""
    hdr = os.path.join(SRC, 'dmc.hpp')
    if not curl(RAW + 'include/embedded_assets/dmc/dmc.hpp', hdr):
        return {}
    text = open(hdr).read()
    names = dict(re.findall(r'DMC_([A-Z0-9_]+)_NAME_ARR\[\] [A-Z_]+ = "([^"]+)";', text))
    cols = dict(re.findall(r'DMC_([A-Z0-9_]+)_SPRITE_SHEET_COLS [A-Z_]+ = (\d+);', text))
    return {names[k]: int(v) for k, v in cols.items() if k in names}


def source_for(num, cols):
    """(path, columns) for this Digimon, or (None, 0)."""
    _n, _v, stem, disp, *_ = D.by_num()[num]
    extra = os.path.join(EXTRA, disp.lower().replace('.', '') + '.png')
    if os.path.exists(extra):
        im = Image.open(extra)
        return extra, max(1, round(im.width / im.height))
    dst = os.path.join(SRC, stem + '.png')
    if not curl(RAW + 'assets/dmc/' + stem.replace(' ', '%20') + '.png', dst):
        return None, 0
    return dst, cols.get(stem, 13)


def frames_of(path, ncols):
    """Split a sheet into per-frame colour grids on the dot lattice. Returns a
    list of dicts {(x, y): (r, g, b)} in one shared coordinate system."""
    im = Image.open(path).convert('RGBA')
    w, h = im.size
    px = im.load()
    on = lambda x, y: px[x, y][3] > 128
    # dot size: the gcd of every horizontal and vertical run of opaque colour
    g = 0
    for y in range(h):
        run, prev = 0, None
        for x in range(w + 1):
            c = px[x, y][:3] if x < w and on(x, y) else None
            if c is not None and c == prev:
                run += 1
            else:
                if run:
                    g = gcd(g, run)
                run = 1 if c is not None else 0
            prev = c
    g = max(1, min(g, 8))
    fw = w / ncols
    frames = []
    for i in range(ncols):
        x0, x1 = int(round(i * fw)), int(round((i + 1) * fw))
        # lattice origin for this frame: first opaque pixel modulo the dot size
        ox = oy = None
        for y in range(h):
            for x in range(x0, x1):
                if on(x, y):
                    ox, oy = (x - x0) % g, y % g
                    break
            if ox is not None:
                break
        if ox is None:
            frames.append({})
            continue
        cells = {}
        for cy, y in enumerate(range(oy, h, g)):
            for cx, x in enumerate(range(x0 + ox, x1, g)):
                sx, sy = min(w - 1, x + g // 2), min(h - 1, y + g // 2)
                if on(sx, sy):
                    cells[(cx + ox // g, cy)] = px[sx, sy][:3]
        frames.append(cells)
    return frames


def normalise(frames):
    """One canvas for every frame: the union bounding box, plus a margin for
    the rim, the hop and the lunge."""
    pts = [p for f in frames for p in f]
    x0, x1 = min(p[0] for p in pts), max(p[0] for p in pts)
    y0, y1 = min(p[1] for p in pts), max(p[1] for p in pts)
    W, H = x1 - x0 + 1, y1 - y0 + 1
    out = []
    for f in frames:
        grid = [[None] * W for _ in range(H)]
        for (x, y), c in f.items():
            grid[y - y0][x - x0] = c
        out.append(grid)
    return out


def upscale_rim(grid, pad=3):
    """x2, with a 1-px pale rim around the silhouette and room to move."""
    H, W = len(grid), len(grid[0])
    BW, BH = W * UP + 2 * pad * UP, H * UP + 2 * pad * UP
    big = [[None] * BW for _ in range(BH)]
    for y in range(H):
        for x in range(W):
            c = grid[y][x]
            if c is None:
                continue
            for dy in range(UP):
                for dx in range(UP):
                    big[(y + pad) * UP + dy][(x + pad) * UP + dx] = c
    rim = []
    for y in range(BH):
        for x in range(BW):
            if big[y][x] is not None:
                continue
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < BH and 0 <= xx < BW and big[yy][xx] not in (None, RIM):
                        rim.append((x, y))
                        break
                else:
                    continue
                break
    for x, y in rim:
        big[y][x] = RIM
    return big


def shift(frame, dx, dy):
    h, w = len(frame), len(frame[0])
    out = [[None] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            sx, sy = x - dx, y - dy
            if 0 <= sx < w and 0 <= sy < h:
                out[y][x] = frame[sy][sx]
    return out


def mirror(frame):
    return [list(reversed(r)) for r in frame]


def actions(fr):
    """The TPK2 action set, from the V-Pet frames. Sprites face LEFT."""
    get = lambda i, fb=F_IDLE1: fr[i] if i < len(fr) and fr[i] is not None else fr[fb]
    idle1, idle2 = get(F_IDLE1), get(F_IDLE2)
    happy, eat1, eat2 = get(F_HAPPY), get(F_EAT1), get(F_EAT2, F_EAT1)
    sleep, angry, lose = get(F_SLEEP, F_DOWN), get(F_ANGRY), get(F_LOSE1, F_ANGRY)
    attack, refuse, sad = get(F_ATTACK, F_ANGRY), get(F_REFUSE), get(F_SAD)
    return [
        (0, [idle1, idle2], [500, 500]),                                  # Idle
        (1, [idle1, idle2], [260, 260]),                                  # WalkL
        (2, [mirror(idle1), mirror(idle2)], [260, 260]),                  # WalkR
        (3, [sleep, sleep], [900, 900]),                                  # Sleep
        (4, [eat1, eat2, eat1, eat2], [200, 200, 200, 200]),              # Eat
        (5, [shift(lose, -2, 0), shift(lose, 2, 0), angry], [90, 90, 220]),  # Hurt
        (6, [idle1, shift(attack, -6, 0), attack, idle1], [120, 140, 200, 160]),  # Attack
        (7, [happy, idle1, happy, idle1], [300, 300, 300, 300]),          # Pose: cheer
        (9, [happy, idle1], [300, 400]),                                  # Nod
        (10, [idle1, refuse, sad, idle1], [300, 300, 400, 300]),          # DeepBreath
    ]


def hue_shift(c):
    r, g, b = (v / 255 for v in c)
    h, l, s = colorsys.rgb_to_hls(r, g, b)
    if s < 0.15 or l < 0.08 or l > 0.92:      # keep black, white and greys
        return c
    r, g, b = colorsys.hls_to_rgb((h + VARIANT_HUE) % 1.0, l, s)
    return (int(r * 255), int(g * 255), int(b * 255))


def rgb565(c):
    r, g, b = c
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


def write_tpk2(path, acts, recolour=None):
    pal, idx = [], {}
    def key(c):
        c = recolour(c) if recolour and c != RIM else c
        k = rgb565(c)
        if k not in idx:
            idx[k] = len(pal)
            pal.append(k)
        return idx[k]
    blocks = []
    for aid, frames, ms in acts:
        h, w = len(frames[0]), len(frames[0][0])
        data = bytearray()
        for fr in frames:
            for row in fr:
                for c in row:
                    data.append(0xFF if c is None else key(c))
        blocks.append((aid, w, h, frames, ms, bytes(data)))
    if len(pal) > 255:
        raise SystemExit('%s: %d colours' % (path, len(pal)))
    with open(path, 'wb') as fh:
        fh.write(b'TPK2')
        fh.write(struct.pack('<BH', len(blocks), len(pal)))
        for c in pal:
            fh.write(struct.pack('<H', c))
        for aid, w, h, frames, ms, data in blocks:
            fh.write(struct.pack('<4B', aid, w, h, len(frames)))
            fh.write(struct.pack('<%dH' % len(frames), *ms))
            fh.write(data)


def main():
    os.makedirs(OUT, exist_ok=True)
    cols = sheet_columns()
    noart, done = [], 0
    for num, *_ in D.ROSTER:
        disp = D.by_num()[num][3]
        path, ncols = source_for(num, cols)
        if not path:
            noart.append(num)
            print(f'  {num:3d} {disp:16s} -- no art')
            for pre in ('p', 'ps'):
                stale = os.path.join(OUT, f'{pre}{num:03d}.bin')
                if os.path.exists(stale):
                    os.remove(stale)
            continue
        frames = [f for f in frames_of(path, ncols)]
        grids = normalise([f if f else frames[0] for f in frames])
        big = [upscale_rim(g) for g in grids]
        acts = actions(big)
        write_tpk2(os.path.join(OUT, f'p{num:03d}.bin'), acts)
        write_tpk2(os.path.join(OUT, f'ps{num:03d}.bin'), acts, hue_shift)
        done += 1
        print(f'  {num:3d} {disp:16s} {len(grids[0][0])}x{len(grids[0])} dots, {ncols} frames')
    with open(os.path.join(HERE, '..', 'noart.h'), 'w') as f:
        f.write('// GENERATED by tools/pack_sprites.py. Do not edit.\n//\n'
                '// Digimon with no sprite yet. They keep their dex numbers but never\n'
                '// hatch, are never offered as a digivolution and never appear in\n'
                '// the wild: a creature that can only draw as a number is a defect.\n'
                '#pragma once\n#include <stdint.h>\n\n')
        f.write('#define NO_ART_COUNT %d\n' % len(noart))
        f.write('static const int16_t NO_ART[NO_ART_COUNT ? NO_ART_COUNT : 1] = {\n  %s\n};\n\n'
                % (', '.join(map(str, noart)) or '0'))
        f.write('static inline bool speciesHasArt(int16_t d) {\n'
                '  for (int i = 0; i < NO_ART_COUNT; i++)\n'
                '    if (NO_ART[i] == d) return false;\n'
                '  return true;\n}\n')
    print(f'{done} packed, {len(noart)} without art: {noart}')


if __name__ == '__main__':
    main()
