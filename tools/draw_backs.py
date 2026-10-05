#!/usr/bin/env python3
"""Draws the twelve battle backgrounds into tools/backs/*.png.

    python3 tools/draw_backs.py && python3 tools/gen_backs.py

Six biomes x day/night, 240x112, flat pixel art generated from code -- so they
are this project's own. A faint data grid in the sky says "Digital World".
"""
import math
import os
import random

from PIL import Image, ImageDraw

W, H = 240, 112
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'backs')

# per biome: sky top, sky bottom, far hills, near ground, ground detail, accent
BIOMES = {
    'MEADOW':   ('#6fb7e8', '#c9ecf7', '#8cc47a', '#6aa84f', '#4f8a3c', '#f4e27a'),
    'BEACH':    ('#5aa8e6', '#bfe8f7', '#3f8fc9', '#e8d39a', '#c9b27a', '#ffffff'),
    'FOREST':   ('#7fb8d8', '#d2ecd8', '#3d7a4a', '#5c8f3e', '#2f5f33', '#9b6b3c'),
    'VOLCANO':  ('#e08a5a', '#f6c98a', '#6b4a3a', '#7a5a46', '#4a332a', '#ff6a2a'),
    'MOUNTAIN': ('#7ab0d8', '#d6e6ef', '#8a8fa0', '#9a8a6a', '#6f6450', '#f2f2f2'),
    'SNOW':     ('#8fb8d8', '#e4eff6', '#b8c8d8', '#eef4f8', '#c4d4e2', '#3f6f4a'),
}


def rgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


def mix(a, b, t):
    return tuple(int(x + (y - x) * t) for x, y in zip(a, b))


def night(c):
    r, g, b = c
    return (int(r * 0.30), int(g * 0.34), int(b * 0.52 + 20))


def scene(name, is_night):
    top, bot, far, ground, detail, accent = map(rgb, BIOMES[name])
    tone = night if is_night else (lambda c: c)
    rnd = random.Random(name)
    im = Image.new('RGB', (W, H))
    d = ImageDraw.Draw(im)
    horizon = 62
    # banded sky (6 bands: flat pixel-art gradient)
    for y in range(horizon):
        t = int(y / horizon * 6) / 5
        d.line([(0, y), (W, y)], fill=tone(mix(top, bot, t)))
    # the data grid
    gridc = tone(mix(bot, top, 0.35))
    for x in range(0, W, 16):
        for y in range(4, horizon - 6, 3):
            im.putpixel((x, y), gridc)
    for y in range(8, horizon - 6, 16):
        for x in range(0, W, 3):
            im.putpixel((x, y), gridc)
    # sun / moon and stars
    if is_night:
        for _ in range(28):
            im.putpixel((rnd.randrange(W), rnd.randrange(horizon - 8)), (230, 236, 255))
        d.ellipse([190, 10, 206, 26], fill=(236, 236, 214))
        d.ellipse([195, 8, 211, 24], fill=tone(mix(top, bot, 0.2)))
    else:
        d.ellipse([186, 10, 204, 28], fill=(255, 236, 150))
    # far silhouette
    farc = tone(far)
    pts = [(0, horizon)]
    for x in range(0, W + 8, 8):
        if name in ('MOUNTAIN', 'VOLCANO', 'SNOW'):
            hgt = 22 + 16 * abs(math.sin(x / 31.0)) + rnd.randrange(4)
        elif name == 'BEACH':
            hgt = 3
        else:
            hgt = 8 + 6 * math.sin(x / 23.0) + rnd.randrange(3)
        pts.append((x, horizon - int(hgt)))
    pts.append((W, horizon))
    d.polygon(pts, fill=farc)
    if name == 'VOLCANO':
        d.polygon([(70, horizon), (110, 22), (130, 22), (170, horizon)], fill=tone(rgb('#5a3a2e')))
        d.polygon([(110, 22), (130, 22), (124, 34), (116, 34)], fill=tone(accent))
    if name in ('SNOW', 'MOUNTAIN'):
        for x in range(0, W, 8):
            y = min(py for px, py in pts if abs(px - x) <= 4)
            d.line([(x - 3, y + 2), (x + 3, y + 2)], fill=tone(rgb('#f6fbff')))
    # ground
    d.rectangle([0, horizon, W, H], fill=tone(ground))
    for y in range(horizon + 6, H, 9):
        off = (y * 7) % 13
        for x in range(off, W, 13):
            d.line([(x, y), (x + 4, y)], fill=tone(detail))
    if name == 'BEACH':
        d.rectangle([0, horizon, W, horizon + 14], fill=tone(rgb('#3f8fc9')))
        for x in range(0, W, 12):
            d.line([(x, horizon + 4), (x + 5, horizon + 4)], fill=tone(accent))
            d.line([(x + 6, horizon + 10), (x + 10, horizon + 10)], fill=tone(accent))
    if name in ('MEADOW', 'FOREST'):
        for _ in range(40):
            x, y = rnd.randrange(W), rnd.randrange(horizon + 4, H - 2)
            d.line([(x, y), (x, y - 3)], fill=tone(detail))
            d.line([(x + 2, y), (x + 3, y - 2)], fill=tone(detail))
        if name == 'MEADOW':
            for _ in range(10):
                x, y = rnd.randrange(W), rnd.randrange(horizon + 6, H - 4)
                d.rectangle([x, y, x + 1, y + 1], fill=tone(accent))
    if name in ('FOREST', 'SNOW'):
        for x in list(range(4, 70, 18)) + list(range(176, W, 18)):
            base = horizon + 2 + (x % 5)
            d.rectangle([x + 5, base - 8, x + 7, base], fill=tone(rgb('#6b4a2a')))
            d.polygon([(x, base - 7), (x + 6, base - 30), (x + 12, base - 7)], fill=tone(accent if name == 'SNOW' else far))
    if name == 'VOLCANO':
        for _ in range(12):
            x, y = rnd.randrange(W), rnd.randrange(horizon + 6, H - 4)
            d.line([(x, y), (x + 6, y)], fill=tone(accent))
    if name == 'MOUNTAIN':
        for _ in range(14):
            x, y = rnd.randrange(W), rnd.randrange(horizon + 6, H - 6)
            d.polygon([(x, y + 5), (x + 4, y), (x + 9, y + 5)], fill=tone(detail))
    return im


def main():
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith('.png'):
            os.remove(os.path.join(OUT, f))
    for name in BIOMES:
        for n in (False, True):
            path = os.path.join(OUT, name + (' NIGHT' if n else '') + '.png')
            scene(name, n).save(path)
    print('drew %d scenes into %s' % (len(BIOMES) * 2, OUT))


if __name__ == '__main__':
    main()
