#!/usr/bin/env python3
"""TDAG - TamaDigi Action Grid. The captured Time Stranger animations, laid out
under the sprite actions TamaDigi actually plays (sdmon.h PMD_*), each cell
labelled with the slot and the clip behind it. A second band holds captures with
no TamaDigi slot yet. Every cell loops on its own length.

    python3 -I tdag.py agumon 24 12
"""
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, '..', 'renders', 'actions')
OUT = os.path.join(HERE, 'sheets')
FONT = '/System/Library/Fonts/Helvetica.ttc'
BG, PANEL, NEW, SPARE = '#0b1418', '#22303a', '#3a6b4a', '#4a3a24'
CELL, LAB = 150, 34
COLS = 6
EYES = {0: 'open', 1: 'closed', 2: 'smile', 3: 'angry'}

# (TamaDigi slot, capture, mirrored, kind) - kind: 'have' = existing slot,
# 'new' = needs a firmware change, 'spare' = captured, no slot yet.
TDAG = [
    ('Idle', 'idle', False, 'have'),
    ('Walk left', 'walk', False, 'have'),
    ('Walk right', 'walk', True, 'have'),
    ('Sleep', 'sleep', False, 'have'),
    ('Eat', 'eat', False, 'have'),
    ('Hurt', 'hurt', False, 'have'),
    ('Sad (new)', 'sad', False, 'new'),
    ('Attack (physical)', 'attack', False, 'have'),
    ('Attack (special, new)', 'attack2', False, 'new'),
    ('Pose / cheer', 'cheer', False, 'have'),
    ('Nod', 'nod', False, 'have'),
    ('Deep breath', 'yawn', False, 'have'),
    ('Hop (unplayed)', 'victory', False, 'new'),   # the slot exists; TamaDigi's code never plays it
]
SPARES = [('no slot: refuse', 'refuse'),
          ('no slot: knocked down', 'knockdown'), ('no slot: get up', 'getup')]


def magick(*a):
    subprocess.run(['magick', *a], check=True)


def eyes_note(e):
    if isinstance(e, list):
        return 'eyes ' + '>'.join(EYES[r] for _, r in e)
    return f'eyes {EYES[e]}'


slug, rates = sys.argv[1], [int(r) for r in sys.argv[2:]] or [24, 12]
meta = json.load(open(os.path.join(SRC, slug, 'bounds.json')))
cells_spec = [(lab, cap, flip, kind) for lab, cap, flip, kind in TDAG] + \
             [(lab, cap, False, 'spare') for lab, cap in SPARES]
boxes = [b for _, cap, _, _ in cells_spec for b in meta[cap]['bounds'] if b]
x0, y0 = min(b['x'] for b in boxes), min(b['y'] for b in boxes)
x1, y1 = max(b['x'] + b['w'] for b in boxes), max(b['y'] + b['h'] for b in boxes)
crop = f'{x1 - x0}x{y1 - y0}+{x0}+{y0}'

work = os.path.join(HERE, 'work', 'tdag', slug)
shutil.rmtree(work, ignore_errors=True)
os.makedirs(work)
seq = {}
for ci, (lab, cap, flip, kind) in enumerate(cells_spec):
    m = meta[cap]
    tint = {'have': PANEL, 'new': NEW, 'spare': SPARE}[kind]
    sub = f"{m['clip'].split('_', 1)[1]}{' mirrored' if flip else ''} - {eyes_note(m['eyes'])}"
    seq[ci] = []
    for i in range(m['frames']):
        dst = os.path.join(work, f'c{ci:02d}_{i:03d}.png')
        magick(os.path.join(SRC, slug, f'{cap}_{i:02d}.png'), '+gravity', '-crop', crop, '+repage',
               *(['-flop'] if flip else []), '-resize', f'{CELL - 10}x{CELL - 10}',
               '-background', BG, '-gravity', 'south', '-extent', f'{CELL}x{CELL}', '-flatten', '+gravity',
               '(', '-size', f'{CELL}x{LAB}', f'xc:{tint}', '-font', FONT, '-fill', 'white', '-pointsize', '13',
               '-gravity', 'north', '-annotate', '+0+2', lab, '-fill', '#b8c4ca', '-pointsize', '9',
               '-annotate', '+0+19', sub, '+gravity', ')', '-append', '+repage',
               '-bordercolor', BG, '-border', '1', dst)
        seq[ci].append(dst)

os.makedirs(OUT, exist_ok=True)
main_ids = list(range(len(TDAG)))
spare_ids = list(range(len(TDAG), len(cells_spec)))
width = COLS * (CELL + 2)
for fps in rates:
    step = 24 // fps
    s = {ci: v[::step] for ci, v in seq.items()}
    n = max(len(v) for v in s.values())
    fdir = os.path.join(work, f'f{fps}')
    os.makedirs(os.path.join(fdir, 'frames'))

    def band(ids, k, name):
        rows = []
        for r, start in enumerate(range(0, len(ids), COLS)):
            ri = os.path.join(fdir, f'{name}{r}.png')
            magick(*[s[c][k % len(s[c])] for c in ids[start:start + COLS]], '+append',
                   '-background', BG, '-gravity', 'west', '-extent', f'{width}x{CELL + LAB + 2}', '+gravity', ri)
            rows.append(ri)
        return rows

    for k in range(n):
        title = os.path.join(fdir, 'title.png')
        magick('-size', f'{width}x30', f'xc:{BG}', '-font', FONT, '-pointsize', '16', '-fill', 'white',
               '-gravity', 'center', '-annotate', '+0+0', f'TDAG - {slug} - {fps} fps', title)
        sep = os.path.join(fdir, 'sep.png')
        magick('-size', f'{width}x24', f'xc:{BG}', '-font', FONT, '-pointsize', '12', '-fill', '#d9b77a',
               '-gravity', 'west', '-annotate', '+8+0', 'Captured, no TamaDigi slot yet', sep)
        magick(title, *band(main_ids, k, 'm'), sep, *band(spare_ids, k, 's'), '-append', '+repage',
               os.path.join(fdir, 'frames', f'{k:03d}.png'))
    frames = sorted(os.path.join(fdir, 'frames', f) for f in os.listdir(os.path.join(fdir, 'frames')))
    gif = os.path.join(OUT, f'tdag_{slug}_{fps}fps.gif')
    magick('-delay', str(round(100 / fps)), '-loop', '0', *frames, '-layers', 'Optimize', gif)
    print(gif, f'{n} frames, {os.path.getsize(gif) // 1024} KB')
