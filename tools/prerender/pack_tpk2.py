#!/usr/bin/env python3
"""Pack pre-rendered Time Stranger action frames (from actions.mjs) into the
TPK2 sprite format the firmware already reads (sdmon.cpp), so they can be tried
in the emulator or on a board without any firmware change.

    python3 -I tools/prerender/pack_tpk2.py agumon 3 out_dir [--gamma 0.5] [--ref-units 3.25]
                                            [--ref-px 100] [--max-frames 24] [--rim]

writes out_dir/pNNN.bin for that dex number.

Size follows the Digimon's real in-game height (capture px / the capture's
px-per-unit, recorded by actions.mjs), compressed so a roster spanning ~6x in
height stays readable: stored idle height = ref_px * (H / ref_units) ** gamma.
The defaults put Greymon (3.25 units) at 100 px, ~200 px on the panel, and
Koromon (0.53) at ~40 px, 2.5x smaller instead of the game's 6x.

Limits it works within, all from sdmon.cpp / TamaDigi.ino as they stand:
  - 24 frames per action (PmdAct.ms[24]): clips are thinned to fit, the frame
    time stretched to keep their length
  - 255 colours, shared by every action (index 0xFF = transparent)
  - 3 MB per file (PmdMon::load)
  - only the existing slots (PMD_IDLE..PMD_SIT); sad and the special attack have
    none yet, so they are left out, as is any action the Digimon has no clip for
  - drawn at a whole-number scale picked from the idle canvas (drawPmdCore:
    170 / idle height, clamped to 2..5), which would blow every pet up to the
    same size: so the idle canvas is padded to at least MIN_IDLE_H, pinning the
    scale at 2x and letting the real size difference show
No outline by default (--rim restores pack_sprites.py's pale one, for night skies).
Sprites face LEFT. Needs ImageMagick (`magick`) and plain Python.
Output is built from Bandai Namco's models: never commit it (renders/ is ignored).
"""
import json
import os
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, 'renders', 'actions')
RIM = (0xF4, 0xF6, 0xEE)        # the packer's pale rim (pack_sprites.py), legible on the night sky
MAX_BYTES = 3 * 1024 * 1024
MIN_IDLE_H = 86                 # 170 / 86 = 1, clamped up to 2: every pet drawn at 2x

# PMD action id -> (capture, mirrored). Ids from sdmon.h's PMD_* enum.
SLOTS = [
    (0, 'idle', False),      # PMD_IDLE
    (1, 'walk', False),      # PMD_WALKL
    (2, 'walk', True),       # PMD_WALKR
    (3, 'sleep', False),     # PMD_SLEEP
    (4, 'eat', False),       # PMD_EAT
    (5, 'hurt', False),      # PMD_HURT
    (6, 'attack', False),    # PMD_ATTACK
    (7, 'cheer', False),     # PMD_POSE
    (8, 'victory', False),   # PMD_HOP
    (9, 'nod', False),       # PMD_NOD
    (10, 'yawn', False),     # PMD_BREATH
]


def run(*a):
    return subprocess.run(['magick', *a], check=True, capture_output=True).stdout


def main():
    args = sys.argv[1:]
    slug, dex, out = args[0], int(args[1]), args[2]
    opt = lambda k, d: type(d)(args[args.index(k) + 1]) if k in args else d
    gamma, ref_units, ref_px = opt('--gamma', 0.5), opt('--ref-units', 3.25), opt('--ref-px', 100.0)
    max_frames = opt('--max-frames', 24)
    rim = '--rim' in args
    meta = json.load(open(os.path.join(SRC, slug, 'bounds.json')))

    # Real height in world units, from the idle capture and its scale.
    idle_view = meta['idle'].get('view')
    if not idle_view:
        sys.exit(f'{slug}: no capture scale in bounds.json; run actions.mjs --measure first')
    idle_h = max(b['h'] for b in meta['idle']['bounds'] if b)
    units = idle_h / idle_view['pxPerUnit']
    stored_idle = ref_px * (units / ref_units) ** gamma
    px_per_unit = stored_idle / units              # stored px per world unit, this Digimon
    print(f'{slug}: {units:.2f} units tall -> idle {stored_idle:.0f} px stored, ~{2 * stored_idle:.0f} px on screen')
    # Horizontal anchor: the idle's centre, so actions don't jump sideways
    # (drawPmdCore centres each action's canvas on the pet's x).
    ib = [b for b in meta['idle']['bounds'] if b]
    cx = (min(b['x'] for b in ib) + max(b['x'] + b['w'] for b in ib)) / 2

    tmp = tempfile.mkdtemp(prefix='tpk2_')
    acts = []          # (aid, w, h, ms list, [frame pixel rows of RGBA tuples])
    for aid, cap, flip in SLOTS:
        if cap not in meta:
            print(f'  note: no {cap} capture; slot {aid} left empty (the device falls back to idle)')
            continue
        m = meta[cap]
        k = px_per_unit / (m.get('view') or idle_view)['pxPerUnit']   # capture px -> stored px
        idxs = list(range(m['frames']))
        step = 1
        while len(idxs[::step]) > max_frames:
            step += 1
        idxs = idxs[::step]
        ms = round(m['ms'] * step)
        bs = [m['bounds'][i] for i in idxs if m['bounds'][i]]
        half = max(max(cx - b['x'], b['x'] + b['w'] - cx) for b in bs)
        acx = cx
        if 2 * half * k + 2 > 255:
            # Travels too far sideways to stay centred on the idle within 255 px:
            # centre this action on its own bounds instead (it shifts as it plays).
            x0b, x1b = min(b['x'] for b in bs), max(b['x'] + b['w'] for b in bs)
            acx, half = (x0b + x1b) / 2, (x1b - x0b) / 2
            print(f'  note: {cap} recentred on itself (too wide to anchor on the idle)')
        y0 = min(b['y'] for b in bs)
        y1 = max(b['y'] + b['h'] for b in bs)
        crop = f'{round(2 * half)}x{y1 - y0}+{round(acx - half)}+{y0}'
        w = max(1, round(2 * half * k)) + (2 if rim else 0)
        body_h = max(1, round((y1 - y0) * k)) + (2 if rim else 0)
        # Pad the idle canvas (transparent rows on top; the firmware anchors by the
        # lowest opaque row) so the firmware's scale stays at 2x for small pets.
        h = max(body_h, MIN_IDLE_H) if aid == 0 else body_h
        if w > 255 or h > 255:
            sys.exit(f'{cap}: {w}x{h} exceeds the 255 px TPK2 limit; lower --ref-px')
        frames = []
        for i in idxs:
            png = os.path.join(SRC, slug, f'{cap}_{i:02d}.png')
            raw = run(png, '-crop', crop, '+repage', *(['-flop'] if flip else []),
                      '-filter', 'box', '-resize', f'{w - (2 if rim else 0)}x{body_h - (2 if rim else 0)}!',
                      *(['-bordercolor', 'none', '-border', '1'] if rim else []),
                      '-background', 'none', '-gravity', 'south', '-extent', f'{w}x{h}', '+gravity',
                      '-depth', '8', 'rgba:-')
            px = [tuple(raw[j:j + 4]) for j in range(0, len(raw), 4)]
            frames.append(px)
        acts.append((aid, w, h, [ms] * len(frames), frames))

    # Hard alpha, plus the pale rim around the silhouette (as pack_sprites does).
    for aid, w, h, ms, frames in acts:
        for f, px in enumerate(frames):
            solid = [p[3] >= 128 for p in px]
            out_px = []
            for j, p in enumerate(px):
                if solid[j]:
                    out_px.append(p[:3])
                elif rim and any(0 <= (y := j // w + dy) < h and 0 <= (x := j % w + dx) < w and solid[y * w + x]
                                 for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1))):
                    out_px.append(RIM)
                else:
                    out_px.append(None)
            frames[f] = out_px

    # One shared palette of <= 255 colours: quantise every opaque pixel together.
    flat = [p for _, _, _, _, frames in acts for px in frames for p in px if p is not None]
    strip = os.path.join(tmp, 'all.rgb')
    with open(strip, 'wb') as fh:
        fh.write(bytes(c for p in flat for c in p))
    q = run('-size', f'{len(flat)}x1', '-depth', '8', f'rgb:{strip}', '+dither', '-colors', '255',
            '-depth', '8', 'rgb:-')
    quant = [tuple(q[j:j + 3]) for j in range(0, len(q), 3)]
    pal, index = [], {}
    rgb565 = lambda c: (c[0] >> 3) << 11 | (c[1] >> 2) << 5 | (c[2] >> 3)
    qi = iter(quant)
    blocks = []
    for aid, w, h, ms, frames in acts:
        data = bytearray()
        for px in frames:
            for p in px:
                if p is None:
                    data.append(0xFF)
                    continue
                c = rgb565(next(qi))
                if c not in index:
                    index[c] = len(pal)
                    pal.append(c)
                data.append(index[c])
        blocks.append((aid, w, h, ms, bytes(data)))
    if len(pal) > 255:
        sys.exit(f'{len(pal)} colours after RGB565 rounding')

    os.makedirs(out, exist_ok=True)
    path = os.path.join(out, f'p{dex:03d}.bin')
    with open(path, 'wb') as fh:
        fh.write(b'TPK2')
        fh.write(struct.pack('<BH', len(blocks), len(pal)))
        for c in pal:
            fh.write(struct.pack('<H', c))
        for aid, w, h, ms, data in blocks:
            fh.write(struct.pack('<4B', aid, w, h, len(ms)))
            fh.write(struct.pack(f'<{len(ms)}H', *ms))
            fh.write(data)
    size = os.path.getsize(path)
    print(f'{path}: {size // 1024} KB, {len(pal)} colours')
    for aid, w, h, ms, data in blocks:
        print(f'  act {aid:2d}  {w:3d}x{h:<3d} {len(ms):2d} frames x {ms[0]} ms')
    if size > MAX_BYTES:
        sys.exit(f'{size} bytes > the 3 MB PmdMon::load limit')


if __name__ == '__main__':
    main()
