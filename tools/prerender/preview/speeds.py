#!/usr/bin/env python3
"""Side-by-side GIF of one action captured at different speeds.

    python3 -I speeds.py koromon walk out.gif 0.75=actions_speed075 0.6=actions_speed06 0.5=actions

Each label=dir is a capture folder under tools/prerender/renders. All cells share
one crop box and play at 24 fps, each looping on its own length.
"""
import json
import os
import shutil
import subprocess
import sys

R = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'renders')
FONT = '/System/Library/Fonts/Helvetica.ttc'
BG = '#0b1418'
slug, action, out, *pairs = sys.argv[1:]
work = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'work', 'speeds')
shutil.rmtree(work, ignore_errors=True)
os.makedirs(os.path.join(work, 'f'))

seqs, boxes = {}, []
for p in pairs:
    label, d = p.split('=', 1)
    meta = json.load(open(os.path.join(R, d, slug, 'bounds.json')))[action]
    seqs[f'{label}x'] = [os.path.join(R, d, slug, f'{action}_{i:02d}.png') for i in range(meta['frames'])]
    boxes += meta['bounds']
x0, y0 = min(b['x'] for b in boxes), min(b['y'] for b in boxes)
x1, y1 = max(b['x'] + b['w'] for b in boxes), max(b['y'] + b['h'] for b in boxes)
crop = f'{x1 - x0}x{y1 - y0}+{x0}+{y0}'

for k in range(96):                       # 4 s at 24 fps
    cells = []
    for j, (name, s) in enumerate(seqs.items()):
        c = os.path.join(work, f'c{j}.png')
        subprocess.run(['magick', s[k % len(s)], '+gravity', '-crop', crop, '+repage', '-resize', '200x200',
                        '-background', BG, '-gravity', 'south', '-extent', '220x210', '-flatten', '+gravity',
                        '(', '-size', '220x26', f'xc:{BG}', '-font', FONT, '-pointsize', '15', '-fill', '#d8dee2',
                        '-gravity', 'center', '-annotate', '+0+0',
                        f'{name}  ({len(s) / 24:.2f}s)', ')', '-append', '+repage', c], check=True)
        cells.append(c)
    subprocess.run(['magick', *cells, '+append', '+repage', os.path.join(work, 'f', f'{k:03d}.png')], check=True)
frames = sorted(os.path.join(work, 'f', f) for f in os.listdir(os.path.join(work, 'f')))
subprocess.run(['magick', '-delay', '4', '-loop', '0', *frames, '-layers', 'Optimize', out], check=True)
print(out)
