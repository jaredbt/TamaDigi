#!/usr/bin/env python3
"""Background mockups for TamaDigi's main screen (466x466 round panel): an
original V-pet-style printed landscape (B') and a neutral studio backdrop (A),
each by day and by night, with Koromon/Agumon/Greymon at their packed sizes.
Mockups only: composited outside the firmware, using its layout constants
(HORIZON 232, PET_GROUND 304, CX 233) and the real bottom panel from an
emulator render.

    WASH=35 NIGHT_WASH=45 SAT=66 python3 -I make_backdrops.py

The approved settings (2026-10-27) are the ones above; landscape_day.png and
landscape_night.png in this folder are its output (as land_day.png and
land_night.png under work/bg/). The mockup half also needs emulator main-screen
renders in ../emu (for the button panel) and captured pets from
tools/prerender/renders; the landscape itself is original art (no game assets).
"""
import json
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
CAP = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools', 'prerender', 'renders', 'actions')
EMU = os.path.join(HERE, '..', 'emu')
OUT = os.path.join(HERE, 'sheets')
W = os.path.join(HERE, 'work', 'bg')
HORIZON, GROUND, CX, PANEL_Y = 232, 304, 233, 312
PETS = {'koromon': 40, 'agumon': 52, 'greymon': 100}     # stored idle px (pack_tpk2.py), shown at 2x
WASH = int(os.environ.get('WASH', 22))       # % of white blended over the landscape
NIGHT_WASH = int(os.environ.get('NIGHT_WASH', WASH))   # the night version's own base wash
SAT = int(os.environ.get('SAT', 100))        # landscape saturation, %
SUFFIX = os.environ.get('SUFFIX', '')        # appended to the output sheet names
# FLOAT=1: no cream panel behind the buttons; the backdrop runs to the bottom of
# the screen and the buttons, labels and page dots sit directly on it.
FLOAT = os.environ.get('FLOAT') == '1'
PANEL_CREAM = '#F6EEE6'                      # the panel fill in the emulator render
os.makedirs(W, exist_ok=True)
os.makedirs(OUT, exist_ok=True)


def magick(*a):
    subprocess.run(['magick', *a], check=True)


def landscape(dst, wash):
    """Pale printed-card landscape: brushed sky, soft hills, little teardrop trees."""
    mvg = os.path.join(W, 'land.mvg')
    trees = []
    for x, y, s in [(70, 214, 1.0), (92, 218, 0.8), (150, 206, 1.1), (171, 210, 0.85), (300, 212, 1.0),
                    (322, 216, 0.75), (388, 205, 1.05), (410, 210, 0.8), (240, 220, 0.7)]:
        trees.append(f"fill '#6f9864' ellipse {x},{y} {5 * s:.1f},{9 * s:.1f} 0,360")
        trees.append(f"stroke '#5e7f52' stroke-width 1.5 line {x},{y + 8 * s:.0f} {x},{y + 13 * s:.0f} stroke none")
    with open(mvg, 'w') as f:
        f.write('\n'.join([
            "viewbox 0 0 466 466",
            # brushed light streaks across the sky
            "fill 'rgba(255,255,255,0.35)'",
            "push graphic-context rotate -7 ellipse 120,70 170,10 0,360 pop graphic-context",
            "push graphic-context rotate -7 ellipse 330,95 150,7 0,360 pop graphic-context",
            "push graphic-context rotate -7 ellipse 180,140 200,6 0,360 pop graphic-context",
            "fill 'rgba(255,255,255,0.22)'",
            "push graphic-context rotate -7 ellipse 360,160 130,5 0,360 pop graphic-context",
            # rolling hills, far then near
            "fill '#a9c79c' path 'M -20,232 C 40,170 140,165 220,215 C 260,232 300,232 300,232 Z'",
            "fill '#9cbf8f' path 'M 180,232 C 260,180 380,175 486,212 L 486,232 Z'",
            "fill '#93b885' path 'M -20,232 C 100,205 200,208 300,228 C 380,215 440,212 486,220 L 486,236 L -20,236 Z'",
            *trees,
        ]))
    magick('-size', f'466x{HORIZON}', 'gradient:#9dbedf-#e6eff0',
           '(', '-size', f'466x{466 - HORIZON}', 'gradient:#a8cc8e-#86b46c', ')', '-append', '+repage',
           '-draw', f'@{mvg}',
           '(', '+clone', '-fill', 'white', '-colorize', '100', ')', '-compose', 'blend',
           '-define', f'compose:args={wash}', '-composite',          # wash out: printed card behind an LCD
           '-modulate', f'100,{SAT}', dst)                          # and a little faded, like old print


def studio(dst):
    """Neutral backdrop: soft radial light, a horizon only implied by a floor tone."""
    magick('-size', '466x466', 'radial-gradient:#f1efea-#c9cdd3',
           '(', '-size', f'466x{466 - GROUND + 40}', 'gradient:#dcdad4-#c4c6c8', ')',
           '-geometry', f'+0+{GROUND - 40}', '-compose', 'over', '-composite', dst)


def night(src, dst, studio_bg):
    # Evening, not midnight: a cool dusky tint that keeps the pet readable.
    if studio_bg:
        magick(src, '-fill', '#3e4a66', '-colorize', '42', '-modulate', '108,90', dst)
    else:  # a deeper blue night for the landscape, still well short of black
        magick(src, '-fill', '#24306a', '-colorize', '52', '-modulate', '100,95', dst)


def pet(slug, stored):
    """Idle frame 0 at its packed size, hard alpha, doubled like the firmware draws it."""
    m = json.load(open(os.path.join(CAP, slug, 'bounds.json')))
    b = m['idle']['bounds'][0]
    dst = os.path.join(W, f'{slug}.png')
    magick(os.path.join(CAP, slug, 'idle_00.png'), '-crop', f"{b['w']}x{b['h']}+{b['x']}+{b['y']}", '+repage',
           '-filter', 'box', '-resize', f'x{stored}',
           '(', '+clone', '-alpha', 'extract', '-threshold', '50%', ')', '-compose', 'copy_opacity', '-composite',
           '-filter', 'point', '-resize', '200%', dst)
    return dst


def screen(bg, slug, stored, dst, shadow):
    p = pet(slug, stored)
    w, h = (int(v) for v in subprocess.run(['magick', 'identify', '-format', '%w %h', p],
                                            capture_output=True, text=True, check=True).stdout.split())
    panel = os.path.join(EMU, 'agumon_day.png')
    layers = [bg]
    if shadow:
        layers += ['(', '-size', '466x466', 'xc:none', '-fill', 'rgba(40,40,50,0.22)', '-draw',
                   f'ellipse {CX},{GROUND} {w // 2},{max(4, w // 12)} 0,360', '-blur', '0x3', ')',
                   '-compose', 'over', '-composite']
    magick(*layers, p, '-geometry', f'+{CX - w // 2}+{GROUND - h}', '-compose', 'over', '-composite',
           '(', panel, '-crop', f'466x{466 - PANEL_Y}+0+{PANEL_Y}', '+repage',
           *(['-alpha', 'set', '-fuzz', '1%', '-transparent', PANEL_CREAM] if FLOAT else []), ')',
           '-geometry', f'+0+{PANEL_Y}', '-compose', 'over', '-composite',
           '-alpha', 'set',
           '(', '-size', '466x466', 'xc:none', '-fill', 'white', '-draw', f'circle {CX},233 {CX},0', ')',
           '-compose', 'dstin', '-composite',
           '-background', '#1b1f24', '-compose', 'over', '-flatten', dst)


landscape(os.path.join(W, 'land_day.png'), WASH)
landscape(os.path.join(W, 'land_night_base.png'), NIGHT_WASH)
night(os.path.join(W, 'land_night_base.png'), os.path.join(W, 'land_night.png'), False)
studio(os.path.join(W, 'studio_day.png'))
night(os.path.join(W, 'studio_day.png'), os.path.join(W, 'studio_night.png'), True)

FONT = '/System/Library/Fonts/Helvetica.ttc'
rows = []
for name, bg, shadow in [("B'  V-pet landscape - day", 'land_day', False),
                         ("B'  V-pet landscape - night", 'land_night', False),
                         ('A  studio - day', 'studio_day', True),
                         ('A  studio - night', 'studio_night', True)]:
    shots = []
    for slug, stored in PETS.items():
        d = os.path.join(W, f'{bg}_{slug}.png')
        screen(os.path.join(W, f'{bg}.png'), slug, stored, d, shadow)
        shots.append(d)
    row = os.path.join(W, f'row_{bg}.png')
    magick('(', '-size', '1410x30', 'xc:#1b1f24', '-font', FONT, '-pointsize', '18', '-fill', 'white',
           '-gravity', 'west', '-annotate', '+10+0', name, ')',
           '(', *shots, '-background', '#1b1f24', '-splice', '4x0', '+append', ')', '-append', row)
    rows.append(row)
for i, (a, b) in enumerate([(rows[0], rows[1]), (rows[2], rows[3])]):
    dst = os.path.join(OUT, [f'backdrop_vpet{SUFFIX}.png', f'backdrop_studio{SUFFIX}.png'][i])
    magick(a, b, '-background', '#1b1f24', '-append', dst)
    print(dst)
