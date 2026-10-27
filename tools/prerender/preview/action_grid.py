#!/usr/bin/env python3
"""Animated preview of a Digimon's captured action set: every action looping at
once in a labelled grid, thinned from the 24 fps capture to a chosen rate.

    python3 -I action_grid.py agumon 24 12 8      # one GIF per rate

All frames of every action share one crop box and one scale, so sizes compare.
The background is the dark night sky TamaDigi draws its pets on.
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
BG = '#0b1418'
CELL = 150
COLS = 5
CAPTURE_FPS = 24


def magick(*a):
    subprocess.run(['magick', *a], check=True)


slug, rates = sys.argv[1], [int(r) for r in sys.argv[2:]] or [24, 12, 8]
meta = json.load(open(os.path.join(SRC, slug, 'bounds.json')))
actions = [a for a, m in meta.items() if m['frames']]
boxes = [b for a in actions for b in meta[a]['bounds'] if b]
x0, y0 = min(b['x'] for b in boxes), min(b['y'] for b in boxes)
x1, y1 = max(b['x'] + b['w'] for b in boxes), max(b['y'] + b['h'] for b in boxes)
crop = f'{x1 - x0}x{y1 - y0}+{x0}+{y0}'

# Each source frame -> one prepared cell (crop, scale, label), reused across rates.
work = os.path.join(HERE, 'work', 'grid', slug)
shutil.rmtree(work, ignore_errors=True)
os.makedirs(work)
cells = {}
for a in actions:
    cells[a] = []
    for i in range(meta[a]['frames']):
        src = os.path.join(SRC, slug, f'{a}_{i:02d}.png')
        dst = os.path.join(work, f'{a}_{i:02d}.png')
        magick(src, '-crop', crop, '+repage', '-resize', f'{CELL - 10}x{CELL - 28}',
               '-background', BG, '-gravity', 'south', '-extent', f'{CELL}x{CELL - 18}',
               '(', '-size', f'{CELL}x18', f'xc:{BG}', '-font', FONT, '-pointsize', '12',
               '-fill', '#c8d0d4', '-gravity', 'center', '-annotate', '+0+0', a, ')',
               '-append', '-bordercolor', '#22303a', '-border', '1', dst)
        cells[a].append(dst)

os.makedirs(OUT, exist_ok=True)
for fps in rates:
    step = CAPTURE_FPS // fps                       # 24->1, 12->2, 8->3
    seqs = {a: cells[a][::step] for a in actions}
    n = max(len(s) for s in seqs.values())
    fdir = os.path.join(work, f'f{fps}')
    frames_dir = os.path.join(fdir, 'frames')
    os.makedirs(frames_dir)
    rows = [actions[i:i + COLS] for i in range(0, len(actions), COLS)]
    for k in range(n):
        row_imgs = []
        for r, row in enumerate(rows):
            ri = os.path.join(fdir, f'row{r}.png')
            pad = [] if len(row) == COLS else ['-background', BG, '-gravity', 'west',
                                               '-extent', f'{COLS * (CELL + 2)}x{CELL + 2}']
            magick(*[seqs[a][k % len(seqs[a])] for a in row], '+append', *pad, ri)
            row_imgs.append(ri)
        title = os.path.join(fdir, 'title.png')
        magick('-size', f'{COLS * (CELL + 2)}x26', f'xc:{BG}', '-font', FONT, '-pointsize', '15',
               '-fill', 'white', '-gravity', 'center', '-annotate', '+0+0', f'{slug}  -  {fps} fps', title)
        # +repage: -append keeps the title strip's 760x26 page geometry, which the
        # GIF optimizer then clips every frame to.
        magick(title, *row_imgs, '-append', '+repage', os.path.join(frames_dir, f'{k:03d}.png'))
    gif = os.path.join(OUT, f'{slug}_actions_{fps}fps.gif')
    delay = round(100 / fps)                        # GIF delays are in 1/100 s
    # An explicit, sorted list: magick's own '*.png' globbing misread this path.
    frames = [os.path.join(frames_dir, f) for f in sorted(os.listdir(frames_dir))]
    magick('-delay', str(delay), '-loop', '0', *frames, '-layers', 'Optimize', gif)
    print(gif, f'{n} frames, {os.path.getsize(gif) // 1024} KB')
