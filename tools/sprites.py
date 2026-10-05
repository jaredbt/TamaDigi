#!/usr/bin/env python3
"""Sprite workshop for TamaDigi: the UI icons, the egg, the poop and the heart.

No Digimon are drawn here -- every creature comes from the SD card.

Los sprites 32x32 se CONSTRUYEN con primitivas (elipses con sombreado
automatico en el borde inferior-derecho, contorno automatico de silueta)
y detalles a pixel (ojos, boca, manchas). El script valida, renderiza un
contact-sheet PNG para revision visual, y emite los arrays C y JS:

  python3 tools/sprites.py        # valida + renderiza tools/sheet.png
  python3 tools/sprites.py emit   # regenera species.h y emitted_sprites.js
"""
# Pillow is imported inside render(), the only thing that needs it: at module
# level it made `emit` fail outright on a machine without PIL, for a header that
# is pure text.

PALETTE = {
    'k': '#1b1b25',  # contorno / ojos
    'w': '#ffffff',  # brillo de ojo
    'y': '#f8d990',  # crema (barriga, placa caparazon)
    'Y': '#e0b860',  # crema sombra
    'o': '#f5863d',  # naranja
    'O': '#d65f28',  # naranja sombra
    'r': '#e8503a',  # rojo (llama, charmeleon)
    'R': '#b53224',  # rojo sombra
    'f': '#ffd95e',  # amarillo llama
    't': '#8fd6b4',  # menta (bulbasaur)
    'T': '#5fae8c',  # menta sombra
    'g': '#58b868',  # verde bulbo
    'G': '#3c8a4c',  # verde bulbo sombra
    'd': '#3f7e62',  # manchas bulbasaur
    'p': '#f08aa4',  # rosa flor
    'P': '#c75f80',  # rosa sombra
    'b': '#7cc4ea',  # azul (squirtle)
    'B': '#4f93c4',  # azul sombra
    'N': '#3a6fa0',  # azul blastoise
    'M': '#2a5278',  # azul blastoise sombra
    'c': '#b07a45',  # marron caparazon
    'C': '#7e5530',  # marron sombra
    'l': '#9aa9e0',  # lavanda (wartortle)
    'L': '#6f7cb8',  # lavanda sombra
    's': '#aab0bc',  # gris canon
    'S': '#787e8c',  # gris sombra
}

W = H = 32


class G:
    def __init__(self, size=W):
        self.size = size
        self.g = [['.'] * size for _ in range(size)]

    def disk(self, cx, cy, rx, ry, ch, shade=None):
        for y in range(self.size):
            for x in range(self.size):
                dx, dy = (x - cx) / rx, (y - cy) / ry
                n = dx * dx + dy * dy
                if n <= 1.0:
                    c = ch
                    if shade and n >= 0.45 and (dx + dy) > 0.42:
                        c = shade
                    self.g[y][x] = c

    def rect(self, x0, y0, x1, y1, ch):
        for y in range(max(0, y0), min(self.size, y1 + 1)):
            for x in range(max(0, x0), min(self.size, x1 + 1)):
                self.g[y][x] = ch

    def px(self, x, y, ch):
        if 0 <= x < self.size and 0 <= y < self.size:
            self.g[y][x] = ch

    def erase(self, cx, cy, rx, ry):
        for y in range(self.size):
            for x in range(self.size):
                dx, dy = (x - cx) / rx, (y - cy) / ry
                if dx * dx + dy * dy <= 1.0:
                    self.g[y][x] = '.'

    def outline(self):
        out = [row[:] for row in self.g]
        for y in range(self.size):
            for x in range(self.size):
                if self.g[y][x] == '.':
                    continue
                for nx, ny in ((x-1, y), (x+1, y), (x, y-1), (x, y+1)):
                    if not (0 <= nx < self.size and 0 <= ny < self.size) or self.g[ny][nx] == '.':
                        out[y][x] = 'k'
                        break
        self.g = out

    def eye(self, x, y):
        # ojo 3x4 con brillo
        self.rect(x, y, x + 2, y + 3, 'k')
        self.px(x, y + 1, 'w')
        self.px(x + 1, y, 'w')

    def smile(self, cx, row):
        self.px(cx - 3, row, 'k')
        self.rect(cx - 2, row + 1, cx + 2, row + 1, 'k')
        self.px(cx + 3, row, 'k')

    def toes(self, x, y):
        self.px(x, y, 'k')

    def rows(self):
        return [''.join(r) for r in self.g]


# ----------------------------------------------------------- shared art

def egg():
    g = G()
    g.disk(16, 17, 8.6, 11.0, 'y', 'Y')
    g.outline()
    g.rect(12, 11, 13, 12, 'g'); g.px(12, 13, 'g')
    g.rect(19, 16, 20, 17, 'g'); g.px(20, 18, 'g')
    g.rect(13, 22, 14, 23, 'g')
    return g.rows()


def poop():
    g = G()
    g.disk(16, 24, 7.0, 4.4, 'c', 'C')
    g.disk(16, 18, 5.0, 3.4, 'c', 'C')
    g.disk(16, 13, 3.0, 2.4, 'c', 'C')
    g.px(16, 9, 'c'); g.px(17, 10, 'c')
    g.outline()
    return g.rows()


def heart():
    return [
        "................................", "................................",
        "................................", "................................",
        "................................", "................................",
        "........krrrrk....krrrrk........", ".......krrrrrrk..krrrrrrk.......",
        "......krrrrrrrrrrrrrrrrrrk......", ".....krrrrrrrrrrrrrrrrrrrrk.....",
        ".....krrwwrrrrrrrrrrrrrrRrk.....", ".....krwwrrrrrrrrrrrrrrRRrk.....",
        "......krrrrrrrrrrrrrrrrrrk......", ".......krrrrrrrrrrrrrrrrk.......",
        "........krrrrrrrrrrrrrrk........", ".........krrrrrrrrrrrrk.........",
        "..........krrrrrrrrrrk..........", "...........krrrrrrrrk...........",
        "............krrrrrrk............", ".............krrrrk.............",
        "..............krrk..............", "...............kk...............",
        "................................", "................................",
        "................................", "................................",
        "................................", "................................",
        "................................", "................................",
        "................................", "................................",
    ]


# ------------------------------------------------------------ iconos botones

def icon_food():
    """Meat on the bone, the V-Pet's own food icon."""
    g = G(16)
    g.disk(7, 7, 5.0, 4.2, 'c', 'C')
    g.rect(10, 9, 12, 11, 'y')
    g.disk(13, 12, 1.6, 1.6, 'y', 'Y')
    g.disk(11, 13, 1.4, 1.4, 'y', 'Y')
    g.outline()
    g.px(5, 5, 'w'); g.px(4, 6, 'w')
    return g.rows()

def icon_play():
    """A plain toy ball, two-tone."""
    g = G(16)
    g.disk(8, 8, 6.2, 6.2, 'b', 'B')
    for y in range(16):
        for x in range(16):
            if g.g[y][x] != '.' and 6 <= x + (y // 3) - 2 <= 9:
                g.g[y][x] = 'f'
    g.outline()
    g.px(5, 4, 'w'); g.px(4, 5, 'w')
    return g.rows()

def icon_light():
    g = G(16)
    g.disk(8, 8, 5.8, 5.8, 'f')
    g.erase(11, 5.5, 4.6, 4.6)
    g.outline()
    g.px(12, 3, 'f'); g.px(13, 5, 'f')
    return g.rows()


def icon_berry_blue():
    """A fish."""
    g = G(16)
    g.disk(7, 8, 5.0, 3.0, 'b', 'B')
    g.rect(12, 6, 12, 10, 'b'); g.rect(13, 5, 13, 11, 'b'); g.rect(14, 4, 14, 12, 'b')
    g.outline()
    g.px(4, 7, 'k'); g.px(6, 6, 'w')
    return g.rows()

def icon_berry_green():
    """A mushroom."""
    g = G(16)
    g.disk(8, 7, 6.0, 4.0, 'r', 'R')
    g.erase(8, 12, 9, 3)
    g.rect(6, 9, 10, 13, 'y')
    g.outline()
    g.px(6, 5, 'w'); g.px(10, 6, 'w'); g.px(8, 4, 'w')
    return g.rows()

def icon_candy():
    """Protein: the V-Pet's strength food, a capsule."""
    g = G(16)
    g.disk(5.5, 8, 3.4, 3.4, 'p', 'P')
    g.disk(10.5, 8, 3.4, 3.4, 'w', 'Y')
    g.rect(6, 5, 10, 11, 'p'); g.rect(8, 5, 10, 11, 'w')
    g.outline()
    g.px(5, 6, 'w')
    return g.rows()

def icon_clean():
    g = G(16)
    g.disk(7, 10, 4.6, 4.4, 'b', 'B')
    g.disk(12, 4.5, 2.2, 2.2, 'b', 'B')
    g.disk(3.5, 4, 1.4, 1.4, 'b')
    g.outline()
    g.px(5, 8, 'w'); g.px(6, 9, 'w'); g.px(11, 4, 'w')
    return g.rows()


def icon_train():
    return [
        "................", "................", "................",
        ".kk..........kk.", ".kNk........kNk.", "kNNk........kNNk",
        "kNNkkkkkkkkkkNNk", "kNNkMMMMMMMMkNNk", "kNNkMMMMMMMMkNNk",
        "kNNkkkkkkkkkkNNk", "kNNk........kNNk", ".kNk........kNk.",
        ".kk..........kk.", "................", "................",
        "................",
    ]


SPRITES = {
    "ICON_FOOD": icon_food(),
    "ICON_PLAY": icon_play(),
    "ICON_LIGHT": icon_light(),
    "ICON_CLEAN": icon_clean(),
    "ICON_TRAIN": icon_train(),
    "ICON_BERRY_B": icon_berry_blue(),
    "ICON_BERRY_G": icon_berry_green(),
    "ICON_CANDY": icon_candy(),
    "EGG": egg(),
    "POOP": poop(),
    "HEART": heart(),
}

def validate():
    ok = True
    for name, rows in SPRITES.items():
        n = len(rows)
        for i, row in enumerate(rows):
            if len(row) != n:
                print(f"{name} fila {i}: {len(row)} chars (esperaba {n})")
                ok = False
            bad = set(row) - set(PALETTE) - {'.'}
            if bad:
                print(f"{name} fila {i}: chars desconocidos {bad}")
                ok = False
    return ok


def render(path="tools/sheet.png", scale=10, percol=3):
    from PIL import Image, ImageDraw
    pad = 16
    names = list(SPRITES)
    ncols = percol
    nrows = (len(names) + ncols - 1) // ncols
    bgs = ['#f2efe1', '#141828']
    cw, chh = W * scale + pad, H * scale + pad + 18
    img = Image.new('RGB', (len(bgs) * (ncols * cw + pad) + pad, nrows * chh + pad), '#888888')
    d = ImageDraw.Draw(img)
    for bi, bg in enumerate(bgs):
        gx = pad + bi * (ncols * cw + pad)
        for i, name in enumerate(names):
            ox = gx + (i % ncols) * cw
            oy = pad + (i // ncols) * chh
            d.rectangle([ox, oy, ox + W * scale, oy + H * scale], fill=bg)
            sc = scale * W // len(SPRITES[name])
            for r, row in enumerate(SPRITES[name]):
                for c, ch in enumerate(row):
                    if ch in PALETTE:
                        d.rectangle([ox + c * sc, oy + r * sc,
                                     ox + (c + 1) * sc - 1, oy + (r + 1) * sc - 1],
                                    fill=PALETTE[ch])
            d.text((ox, oy + H * scale + 3), name, fill='#000')
    img.save(path)
    print(f"guardado {path}")


def rgb565(hexcol):
    r, g, b = int(hexcol[1:3], 16), int(hexcol[3:5], 16), int(hexcol[5:7], 16)
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


UI_COLORS = {
    'UI_BG_DAY': '#f2efe1', 'UI_BG_NIGHT': '#141828',
    'UI_INK': '#2a2a36', 'UI_INK_NIGHT': '#d8dcf0',
    # Secondary text. UI_TRACK was doing this job as well as being a FILL, and
    # a fill that reads on cream is 1.31:1 as text -- effectively invisible,
    # which is why every "tap: back" hint was unreadable. One constant, two
    # jobs; this splits them. 4.56:1 on the cream panel.
    'UI_INK_SOFT': '#5f5747',
    'UI_TEXT_OK': '#21783b', 'UI_TEXT_WARN': '#935706',
    'UI_TEXT_BAD': '#bd301b',
    'UI_TRACK': '#d8d2bd',
    'UI_GROOVE_DAY': '#2a2a36', 'UI_GROOVE_NIGHT': '#d8dcf0',
    'UI_SCROLL_DAY': '#8d8573', 'UI_SCROLL_NIGHT': '#6b7188',
    'UI_BAR_OK': '#58b868', 'UI_BAR_WARN': '#e8a23c', 'UI_BAR_BAD': '#e8503a',
    # The four care stats have their own HUE, because colour is the only thing
    # that tells four identical arcs apart at a glance. Severity moved to the
    # label. An ink outline separates each bright hue from the scene.
    'UI_CARE_FOOD': '#e07b26', 'UI_CARE_JOY': '#df5481',
    'UI_CARE_ENE': '#28ae98', 'UI_CARE_HYG': '#4b8ed1',
    'UI_WHITE': '#ffffff',
}


def emit_c(path="species.h"):
    out = []
    out.append("#pragma once\n#include <stdint.h>\n\n")
    out.append("// Generated by tools/sprites.py -- edit there and run:\n")
    out.append("//   python3 tools/sprites.py emit\n\n")
    out.append(f"#define SPRITE_W {W}\n#define SPRITE_H {H}\n\n")
    for name, hexcol in UI_COLORS.items():
        out.append(f"#define {name} 0x{rgb565(hexcol):04X}  // {hexcol}\n")
    out.append("\n")
    out.append("// caracter de sprite -> RGB565\n")
    out.append("static inline uint16_t spriteColor(char ch) {\n  switch (ch) {\n")
    for ch, hexcol in PALETTE.items():
        out.append(f"    case '{ch}': return 0x{rgb565(hexcol):04X};  // {hexcol}\n")
    out.append("    default: return 0;\n  }\n}\n\n")
    for name, rows in SPRITES.items():
        out.append(f"static const char* const SPR_{name}[{len(rows)}] = {{  // {len(rows)}x{len(rows)}\n")
        for row in rows:
            out.append(f'  "{row}",\n')
        out.append("};\n\n")
    open(path, 'w').write(''.join(out))
    print(f"guardado {path}")


def emit_js(path="tools/emitted_sprites.js"):
    import json
    data = {'palette': PALETTE, 'sprites': SPRITES}
    open(path, 'w').write("const SPRITE_DATA=" + json.dumps(data, separators=(',', ':')) + ";\n")
    print(f"guardado {path}")


if __name__ == '__main__':
    import sys
    if not validate():
        print("-- errores de validacion --")
        sys.exit(1)
    # The preview sheet is a nicety; regenerating species.h is not. Without this
    # a missing Pillow took the header down with it.
    try:
        render()
    except ImportError:
        print("PIL no instalado: se omite tools/sheet.png (pip install pillow)")
    if 'emit' in sys.argv:
        emit_c()
        emit_js()
