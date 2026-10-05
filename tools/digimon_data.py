# -*- coding: utf-8 -*-
"""THE single source of game data for TamaDigi.

Everything the firmware knows about a Digimon comes from here: the roster,
which V-Pet version it belongs to, its stage, attribute and element, its base
stats, its attacks and -- most importantly -- its DIGIVOLUTION BRANCHES.

    python3 tools/gen_dex.py     # -> dex.h   (roster, branches, element chart)
    python3 tools/gen_moves.py   # -> moves.h, move_names.h (attacks, learnsets)

Versions
--------
The "regions" of the old engine are the five Digital Monster V-Pet versions. A
version owns a contiguous block of dex numbers, and its Baby I is the only thing
its Digi-Egg can hatch into. Everything after that is decided by raising it.

  Ver.1  Botamon line    Ver.2  Punimon line    Ver.3  Poyomon line
  Ver.4  Yuramon line    Ver.5  Zurumon line

Each runs Baby I -> Baby II -> Rookie -> Champion -> Ultimate -> Mega. The tree
shapes and the Mega forms follow Digital Monster COLOR (via the evolution data
in furudbat/wayland-vpets); the sprites are that device's full-colour animated
dot sprites. The branch CONDITIONS are this game's own, tuned to its
1-level-per-hour clock. (One deliberate fix: the Ver.1 Ultimate is the Virus
MetalGreymon, as on the 1997 V-Pet, so it leads on to BlitzGreymon.)

Digivolution
------------
A stage lasts a fixed time (levels are hours since hatching). When the time is
up the Digimon is OFFERED a digivolution, and WHICH form it becomes is the
first rule in its list whose conditions hold:

  care mistakes   a stat that hit 10 or below (60 min cooldown)
  training        trAtk + trDef + trSpe
  wins            battles won while this was the Digimon being raised

A rule with no matching branch means it stays as it is -- that is how a
Champion that never fights stays a Champion. The last rule of every Rookie is
a catch-all, so a Rookie always becomes SOMETHING.
"""

# ---------------------------------------------------------------------------
# Stages. The index is stored in dex.h (DexEntry.stage).
STAGES = ['BABY1', 'BABY2', 'ROOKIE', 'CHAMPION', 'ULTIMATE', 'MEGA']
# The level (= hours since hatching + 1) at which each stage BEGINS. A form is
# offered its digivolution once it reaches the next stage's entry level.
STAGE_ENTRY = [1, 2, 7, 25, 49, 61]

# Attributes: the Vaccine > Virus > Data > Vaccine triangle. FREE is neutral
# (all babies are Free).
ATTRS = ['VACCINE', 'DATA', 'VIRUS', 'FREE']

# Elements, the "types" of the battle engine. Order = index in dex.h.
ELEMENTS = ['neutral', 'fire', 'water', 'plant', 'electric', 'wind',
            'earth', 'light', 'dark']

# Attacker element -> {defender element: multiplier}. Unlisted = 1x. There are
# no immunities on purpose: a Digimon is never told an attack "has no effect",
# it is just a bad matchup.
CHART = {
    'fire':     {'plant': 2, 'wind': 2, 'fire': .5, 'water': .5, 'earth': .5},
    'water':    {'fire': 2, 'earth': 2, 'water': .5, 'plant': .5},
    'plant':    {'water': 2, 'earth': 2, 'fire': .5, 'plant': .5, 'wind': .5},
    'electric': {'water': 2, 'wind': 2, 'electric': .5, 'earth': .5, 'plant': .5},
    'wind':     {'plant': 2, 'earth': 2, 'electric': .5, 'wind': .5},
    'earth':    {'fire': 2, 'electric': 2, 'plant': .5, 'wind': .5},
    'light':    {'dark': 2, 'light': .5},
    'dark':     {'light': 2, 'dark': .5},
}

# Accent colour per element (UI) and the background biome it lives in:
# 0 meadow, 1 beach, 2 forest, 3 volcano, 4 mountain, 5 snow.
ELEMENT_ACCENT = {
    'neutral': '#8a8a6a', 'fire': '#e8503a', 'water': '#4f93c4',
    'plant': '#3c8a4c', 'electric': '#b8960b', 'wind': '#7f8fd0',
    'earth': '#b08a3d', 'light': '#c9a227', 'dark': '#6a5a9e',
}
ELEMENT_BIOME = {
    'neutral': 0, 'fire': 3, 'water': 1, 'plant': 2, 'electric': 0,
    'wind': 4, 'earth': 4, 'light': 0, 'dark': 2,
}

# ---------------------------------------------------------------------------
# Base stats come from the stage and a profile, so the roster stays coherent
# without inventing 60 rows of numbers by hand. Order: hp, atk, def, spe, spa, spd
STAGE_BASE = [18, 30, 48, 72, 94, 114]
PROFILES = {
    'bal':   (1.00, 1.00, 1.00, 1.00, 1.00, 1.00),
    'phys':  (1.00, 1.25, 1.05, 0.95, 0.80, 0.95),
    'spec':  (0.95, 0.80, 0.95, 1.00, 1.25, 1.05),
    'tank':  (1.20, 0.95, 1.30, 0.70, 0.85, 1.10),
    'fast':  (0.90, 1.05, 0.80, 1.35, 1.05, 0.85),
    'weak':  (0.80, 0.75, 0.75, 0.80, 0.75, 0.75),  # Numemon & co.: the "fail" forms
}

# ---------------------------------------------------------------------------
# The roster.
#
# key: (num, version, sprite, display, stage, attr, element, profile, moves)
#   sprite   the Digimon Color sprite sheet's name (tools/pack_sprites.py)
#   display  <= 15 chars, ASCII upper case (the bitmap font has no accents)
#   moves    signature attacks, strongest last; generic ones are added by stage
#
# Display names are the English ones, which is also what the German dub uses.
V1, V2, V3, V4, V5 = 'VER.1', 'VER.2', 'VER.3', 'VER.4', 'VER.5'

ROSTER = [
    # ---------------- Ver.1 --------------------------------------------------
    (1,  V1, 'Botamon',               'BOTAMON',       0, 'FREE',    'neutral',  'bal',  []),
    (2,  V1, 'Koromon',               'KOROMON',       1, 'FREE',    'neutral',  'bal',  []),
    (3,  V1, 'Agumon',                'AGUMON',        2, 'VACCINE', 'fire',     'phys', ['PEPPER BRTH']),
    (4,  V1, 'Betamon',               'BETAMON',       2, 'VIRUS',   'water',    'spec', ['ELEC SHOCK', 'WATER JET']),
    (5,  V1, 'Greymon',               'GREYMON',       3, 'VACCINE', 'fire',     'phys', ['GREAT HORNS', 'MEGA FLAME']),
    (6,  V1, 'Tyranomon',             'TYRANNOMON',    3, 'DATA',    'fire',     'tank', ['TAIL SMASH', 'BLAZE BLAST']),
    (7,  V1, 'Devimon',               'DEVIMON',       3, 'VIRUS',   'dark',     'spec', ['TOUCH EVIL', 'DEATH CLAW']),
    (8,  V1, 'Meramon',               'MERAMON',       3, 'DATA',    'fire',     'fast', ['ROAR FIRE', 'BURN FIST']),
    (9,  V1, 'Airdramon',             'AIRDRAMON',     3, 'VACCINE', 'wind',     'fast', ['WING SLASH', 'SPIN NEEDLE']),
    (10, V1, 'Seadramon',             'SEADRAMON',     3, 'DATA',    'water',    'spec', ['WATER JET', 'ICE BLAST']),
    (11, V1, 'Numemon',               'NUMEMON',       3, 'VIRUS',   'dark',     'weak', ['NUME SLUDGE']),
    (12, V1, 'Metal Greymon (Virus)', 'METALGREYMON',  4, 'VIRUS',   'fire',     'phys', ['MEGA CLAW', 'GIGA DESTROY']),
    (13, V1, 'Mamemon',               'MAMEMON',       4, 'DATA',    'neutral',  'fast', ['MAME PUNCH', 'SMILEY BOMB']),
    (14, V1, 'Monzaemon',             'MONZAEMON',     4, 'VACCINE', 'light',    'tank', ['LOVELY PUNCH', 'HEARTS ATK']),
    (15, V1, 'Blitz Greymon',         'BLITZGREYMON',  5, 'VIRUS',   'fire',     'phys', ['TRIDENT ARM', 'BLITZ CANNON']),
    (16, V1, 'Bancho Mamemon',        'BANCHOMAMEMON', 5, 'DATA',    'neutral',  'fast', ['BANCHO PUNCH', 'GUTS BOMB']),
    (17, V1, 'Shin Monzaemon',        'SHINMONZAEMON', 5, 'VACCINE', 'light',    'tank', ['HUG TACKLE', 'HEART BREAK']),
    # ---------------- Ver.2 --------------------------------------------------
    (18, V2, 'Punimon',               'PUNIMON',       0, 'FREE',    'neutral',  'bal',  []),
    (19, V2, 'Tsunomon',              'TSUNOMON',      1, 'FREE',    'neutral',  'bal',  []),
    (20, V2, 'Gabumon',               'GABUMON',       2, 'DATA',    'fire',     'bal',  ['PETIT FIRE']),
    (21, V2, 'Elecmon',               'ELECMON',       2, 'DATA',    'electric', 'fast', ['SUPER THUNDR']),
    (22, V2, 'Kabuterimon',           'KABUTERIMON',   3, 'VACCINE', 'electric', 'tank', ['HORN CHARGE', 'ELECTRO SHOK']),
    (23, V2, 'Garurumon',             'GARURUMON',     3, 'VACCINE', 'water',    'fast', ['FOX FIRE', 'HOWLING BLST']),
    (24, V2, 'Angemon',               'ANGEMON',       3, 'VACCINE', 'light',    'spec', ['ANGEL ROD', 'HAND OF FATE']),
    (25, V2, 'Yukidarumon',           'FRIGIMON',      3, 'VACCINE', 'water',    'tank', ['SNOW BALL', 'SUBZERO FIST']),
    (26, V2, 'Birdramon',             'BIRDRAMON',     3, 'VACCINE', 'fire',     'fast', ['FIRE WING', 'METEOR WING']),
    (27, V2, 'Whamon',                'WHAMON',        3, 'VACCINE', 'water',    'tank', ['BLASTING SPT', 'TIDAL WAVE']),
    (28, V2, 'Vegimon',               'VEGIEMON',      3, 'VIRUS',   'plant',    'weak', ['SWEET BREATH']),
    (29, V2, 'Skull Greymon',         'SKULLGREYMON',  4, 'VIRUS',   'dark',     'phys', ['BONE CRUSH', 'GROUND ZERO']),
    (30, V2, 'Metal Mamemon',         'METALMAMEMON',  4, 'DATA',    'electric', 'fast', ['METAL PUNCH', 'ENERGY BOMB']),
    (31, V2, 'Vademon',               'VADEMON',       4, 'VIRUS',   'dark',     'spec', ['UF KISS', 'ABDUCT BEAM']),
    (32, V2, 'Skull Mammon',          'SKULLMAMMON',   5, 'VIRUS',   'dark',     'tank', ['TUSK CRUSH', 'SKULL STAMP']),
    (33, V2, 'Cres Garurumon',        'CRESGARURUMON', 5, 'DATA',    'water',    'fast', ['LUNAR CLAW', 'FROST HOWL']),
    (34, V2, 'Ebemon',                'EBEMON',        5, 'VIRUS',   'dark',     'spec', ['BRAIN BEAM', 'BRAIN RUPTR']),
    # ---------------- Ver.3 --------------------------------------------------
    (35, V3, 'Poyomon',               'POYOMON',       0, 'FREE',    'neutral',  'bal',  []),
    (36, V3, 'Tokomon',               'TOKOMON',       1, 'FREE',    'neutral',  'bal',  []),
    (37, V3, 'Patamon',               'PATAMON',       2, 'DATA',    'wind',     'spec', ['BOOM BUBBLE']),
    (38, V3, 'Kunemon',               'KUNEMON',       2, 'VIRUS',   'electric', 'spec', ['ELEC THREAD']),
    (39, V3, 'Unimon',                'UNIMON',        3, 'VACCINE', 'wind',     'fast', ['HORN STRIKE', 'AERIAL ATK']),
    (40, V3, 'Centalmon',             'CENTARUMON',    3, 'DATA',    'earth',    'spec', ['SOLAR RAY', 'HUNT CANNON']),
    (41, V3, 'Ogremon',               'OGREMON',       3, 'VIRUS',   'dark',     'phys', ['BONE CUDGEL', 'PUMMEL WHACK']),
    (42, V3, 'Bakemon',               'BAKEMON',       3, 'VIRUS',   'dark',     'spec', ['HYPNOTISM', 'DARK CLAW']),
    (43, V3, 'Shellmon',              'SHELLMON',      3, 'DATA',    'water',    'tank', ['GOLD RUSH', 'HYDRO PRESS']),
    (44, V3, 'Drimogemon',            'DRIMOGEMON',    3, 'DATA',    'earth',    'phys', ['CRUSH DRILL', 'IRON DRILL']),
    (45, V3, 'Scumon',                'SUKAMON',       3, 'VIRUS',   'dark',     'weak', ['POOP THROW']),
    (46, V3, 'Andromon',              'ANDROMON',      4, 'VACCINE', 'electric', 'bal',  ['LIGHTN BLADE', 'GATLING MSL']),
    (47, V3, 'Giromon',               'GIROMON',       4, 'VACCINE', 'neutral',  'fast', ['SPIRAL SAW', 'DEADLY BOMB']),
    (48, V3, 'Etemon',                'ETEMON',        4, 'VIRUS',   'dark',     'spec', ['LOVE SEREN', 'DARK SPIRITS']),
    (49, V3, 'Chimairamon',           'CHIMAIRAMON',   4, 'VIRUS',   'fire',     'phys', ['DEMON SNAKE', 'HEAT VIPER']),
    (50, V3, 'Hi Andromon',           'HIANDROMON',    5, 'VACCINE', 'electric', 'bal',  ['SPIRAL SWORD', 'ATOMIC RAY']),
    (51, V3, 'Gokumon',               'GOKUMON',       5, 'VIRUS',   'dark',     'phys', ['HELL CHAIN', 'HELL FIRE']),
    (52, V3, 'Bancho Leomon',         'BANCHOLEOMON',  5, 'VACCINE', 'fire',     'phys', ['BANCHO FIST', 'BURN BANCHO']),
    (53, V3, 'Millenniumon',          'MILLENNIUMON',  5, 'VIRUS',   'dark',     'spec', ['TIME BURST', 'TIME DESTROY']),
    # ---------------- Ver.4 --------------------------------------------------
    (54, V4, 'Yuramon',               'YURAMON',       0, 'FREE',    'neutral',  'bal',  []),
    (55, V4, 'Tanemon',               'TANEMON',       1, 'FREE',    'neutral',  'bal',  []),
    (56, V4, 'Piyomon',               'BIYOMON',       2, 'VACCINE', 'wind',     'fast', ['SPIRAL TWIST']),
    (57, V4, 'Palmon',                'PALMON',        2, 'DATA',    'plant',    'spec', ['POISON IVY']),
    (58, V4, 'Monochromon',           'MONOCHROMON',   3, 'DATA',    'earth',    'tank', ['HORN RUSH', 'VOLCANO STRK']),
    (59, V4, 'Cockatrimon',           'COCKATRIMON',   3, 'DATA',    'wind',     'fast', ['PECK DRILL', 'PETRIFY GAZE']),
    (60, V4, 'Leomon',                'LEOMON',        3, 'VACCINE', 'earth',    'phys', ['BEAST SWORD', 'BEAST FIST']),
    (61, V4, 'Kuwagamon',             'KUWAGAMON',     3, 'VIRUS',   'plant',    'phys', ['POWER GUILLO', 'SCISSOR CLAW']),
    (62, V4, 'Coelamon',              'COELAMON',      3, 'DATA',    'water',    'tank', ['SHELL STRIKE', 'TIDE CRASH']),
    (63, V4, 'Mojyamon',              'MOJYAMON',      3, 'VACCINE', 'water',    'bal',  ['BONE BOOMER', 'ICE CLOUD']),
    (64, V4, 'Nanimon',               'NANIMON',       3, 'VIRUS',   'dark',     'weak', ['NANI PUNCH']),
    (65, V4, 'Megadramon',            'MEGADRAMON',    4, 'VIRUS',   'dark',     'phys', ['DARK CLAW S', 'GENOCIDE ATK']),
    (66, V4, 'Piccolomon',            'PICCOLOMON',    4, 'DATA',    'light',    'fast', ['PIT PELTER', 'BIT BOMB']),
    (67, V4, 'Digitamamon',           'DIGITAMAMON',   4, 'DATA',    'dark',     'tank', ['EGG BOMB', 'NIGHTMARE']),
    (68, V4, 'Darkdramon',            'DARKDRAMON',    5, 'VIRUS',   'dark',     'phys', ['DARK LANCE', 'GIGA DEATH']),
    (69, V4, 'Bloom Lordmon',         'BLOOMLORDMON',  5, 'VACCINE', 'plant',    'spec', ['BLOOM THORN', 'PETAL STORM']),
    (70, V4, 'Gankoomon',             'GANKOOMON',     5, 'DATA',    'earth',    'phys', ['IRON FIST', 'GANKOO SMASH']),
    # ---------------- Ver.5 --------------------------------------------------
    (71, V5, 'Zurumon',               'ZURUMON',       0, 'FREE',    'neutral',  'bal',  []),
    (72, V5, 'Pagumon',               'PAGUMON',       1, 'FREE',    'neutral',  'bal',  []),
    (73, V5, 'Gazimon',               'GAZIMON',       2, 'VIRUS',   'electric', 'fast', ['STUN BLAST']),
    (74, V5, 'Gizamon',               'GIZAMON',       2, 'VIRUS',   'water',    'phys', ['SAW SPIN']),
    (75, V5, 'Dark Tyranomon',        'DARKTYRANNOMON',3, 'VIRUS',   'fire',     'tank', ['TAIL SMASH', 'FIRE BLAST']),
    (76, V5, 'Cyclomon',              'CYCLOMON',      3, 'VIRUS',   'fire',     'phys', ['BEAM ARM', 'HYPER HEAT']),
    (77, V5, 'Devidramon',            'DEVIDRAMON',    3, 'VIRUS',   'dark',     'fast', ['RED EYE', 'CRIMSON CLAW']),
    (78, V5, 'Tuskmon',               'TUSKMON',       3, 'VIRUS',   'earth',    'tank', ['HORN ATTACK', 'SLAM TUSK']),
    (79, V5, 'Flymon',                'FLYMON',        3, 'VIRUS',   'wind',     'fast', ['POISON POWDR', 'DEADLY STING']),
    (80, V5, 'Deltamon',              'DELTAMON',      3, 'VIRUS',   'dark',     'spec', ['SNAKE BITE', 'TRIPLE FORCE']),
    (81, V5, 'Raremon',               'RAREMON',       3, 'VIRUS',   'dark',     'weak', ['TOXIC SLUDGE']),
    (82, V5, 'Metal Tyranomon',       'METALTYRANNOMON',4, 'VIRUS',  'fire',     'tank', ['MISSILE RAIN', 'NUKE LASER']),
    (83, V5, 'Nanomon',               'NANOMON',       4, 'VIRUS',   'electric', 'spec', ['NANO TWIST', 'PLUG BOMB']),
    (84, V5, 'Ex-Tyranomon',          'EXTYRANNOMON',  4, 'DATA',    'fire',     'phys', ['DINO KICK', 'PYRO BLASTER']),
    (85, V5, 'Mugendramon',           'MACHINEDRAMON', 5, 'VIRUS',   'electric', 'tank', ['GIGA CANNON', 'INFINITY CAN']),
    (86, V5, 'Raidenmon',             'RAIDENMON',     5, 'VIRUS',   'electric', 'fast', ['THUNDR LANCE', 'RAIDEN STORM']),
    (87, V5, 'Gaioumon',              'GAIOUMON',      5, 'VIRUS',   'fire',     'phys', ['TWIN BLADE', 'METEOR SLASH']),
]

# Element overrides for the background biome (frosty ones live in the snow).
BIOME_OVERRIDE = {23: 5, 25: 5, 33: 5, 63: 5}

# Versions: name, the dex block it owns, and its Baby I (what the egg hatches).
VERSIONS = [
    (V1, 1,  17, [1]),
    (V2, 18, 34, [18]),
    (V3, 35, 53, [35]),
    (V4, 54, 70, [54]),
    (V5, 71, 87, [71]),
]

# ---------------------------------------------------------------------------
# Digivolution branches, in priority order per source. Each rule:
#   (from, to, minCM, maxCM, minTrain, maxTrain, minWins)
# 255 = no upper bound. The first matching (and showable) rule wins.
NB = 255
def _rookie(frm, a, b, c, d, fail):
    """The V-Pet pattern: few mistakes + trained / untrained, some mistakes +
    trained / untrained, and anything worse is the fail form."""
    return [
        (frm, a,    0, 2,  40, NB, 0),
        (frm, b,    0, 2,   0, 39, 0),
        (frm, c,    3, 5,  40, NB, 0),
        (frm, d,    3, 5,   0, 39, 0),
        (frm, fail, 0, NB,  0, NB, 0),
    ]

def _up(frm, to, wins):
    return (frm, to, 0, NB, 0, NB, wins)

ULT_WINS = 5      # battles won as a Champion before it can reach Ultimate
MEGA_WINS = 15    # ... and as an Ultimate before Mega

EVOLUTIONS = (
    # Ver.1
    [_up(1, 2, 0), (2, 3, 0, 2, 0, NB, 0), _up(2, 4, 0)]
    + _rookie(3, 5, 6, 7, 8, 11)          # Agumon
    + _rookie(4, 7, 8, 9, 10, 11)         # Betamon
    + [_up(5, 12, ULT_WINS), _up(7, 12, ULT_WINS), _up(9, 12, ULT_WINS),
       _up(6, 13, ULT_WINS), _up(8, 13, ULT_WINS), _up(10, 13, ULT_WINS),
       _up(11, 14, ULT_WINS),
       _up(12, 15, MEGA_WINS), _up(13, 16, MEGA_WINS), _up(14, 17, MEGA_WINS)]
    # Ver.2
    + [_up(18, 19, 0), (19, 20, 0, 2, 0, NB, 0), _up(19, 21, 0)]
    + _rookie(20, 22, 23, 24, 25, 28)     # Gabumon
    + _rookie(21, 24, 25, 26, 27, 28)     # Elecmon
    + [_up(22, 29, ULT_WINS), _up(24, 29, ULT_WINS), _up(26, 29, ULT_WINS),
       _up(23, 30, ULT_WINS), _up(25, 30, ULT_WINS), _up(27, 30, ULT_WINS),
       _up(28, 31, ULT_WINS),
       _up(29, 32, MEGA_WINS), _up(30, 33, MEGA_WINS), _up(31, 34, MEGA_WINS)]
    # Ver.3
    + [_up(35, 36, 0), (36, 37, 0, 2, 0, NB, 0), _up(36, 38, 0)]
    + _rookie(37, 39, 40, 41, 42, 45)     # Patamon
    + _rookie(38, 41, 42, 43, 44, 45)     # Kunemon
    + [_up(39, 46, ULT_WINS), _up(41, 46, ULT_WINS), _up(43, 46, ULT_WINS),
       _up(40, 47, ULT_WINS), _up(42, 47, ULT_WINS), _up(44, 47, ULT_WINS),
       _up(45, 49, 10), _up(45, 48, ULT_WINS),     # Sukamon: Chimairamon if it fought hard
       _up(46, 50, MEGA_WINS), _up(47, 51, MEGA_WINS), _up(48, 52, MEGA_WINS),
       _up(49, 53, 20), _up(49, 52, MEGA_WINS)]    # Chimairamon: Millenniumon at 20
    # Ver.4
    + [_up(54, 55, 0), (55, 56, 0, 2, 0, NB, 0), _up(55, 57, 0)]
    + _rookie(56, 58, 59, 60, 61, 64)     # Biyomon
    + _rookie(57, 60, 61, 62, 63, 64)     # Palmon
    + [_up(58, 65, ULT_WINS), _up(60, 65, ULT_WINS), _up(62, 65, ULT_WINS),
       _up(59, 66, ULT_WINS), _up(61, 66, ULT_WINS), _up(63, 66, ULT_WINS),
       _up(64, 67, ULT_WINS),
       _up(65, 68, MEGA_WINS), _up(66, 69, MEGA_WINS), _up(67, 70, MEGA_WINS)]
    # Ver.5
    + [_up(71, 72, 0), (72, 73, 0, 2, 0, NB, 0), _up(72, 74, 0)]
    + _rookie(73, 75, 76, 77, 78, 81)     # Gazimon
    + _rookie(74, 77, 78, 79, 80, 81)     # Gizamon
    + [_up(75, 82, ULT_WINS), _up(77, 82, ULT_WINS), _up(79, 82, ULT_WINS),
       _up(76, 83, ULT_WINS), _up(78, 83, ULT_WINS), _up(80, 83, ULT_WINS),
       _up(81, 84, ULT_WINS),
       _up(82, 85, MEGA_WINS), _up(83, 86, MEGA_WINS), _up(84, 87, MEGA_WINS)]
)

# ---------------------------------------------------------------------------
# Attacks.
# (NAME <= 12 chars, element, category, power, accuracy, effect, param,
#  statMask, stages, target, ailment, ailmentChance)
MC_PHYS, MC_SPEC, MC_STATUS = 0, 1, 2
AIL_NONE, AIL_PARA, AIL_BURN, AIL_POISON, AIL_SLEEP, AIL_FREEZE, AIL_CONFUSE = range(7)
EF_NONE, EF_STAGE, EF_RECOIL, EF_DRAIN, EF_FIXED_LVL, EF_FIXED, EF_PRIORITY, \
    EF_NEVER_MISS, EF_MULTI, EF_HEAL, EF_RECHARGE, EF_CHARGE = range(12)
ST_ATK, ST_DEF, ST_SPA, ST_SPD, ST_SPE = 1, 2, 4, 8, 16
TG_SELF, TG_FOE = 0, 1

def A(name, el, cat, pw, acc=100, eff=EF_NONE, param=0, mask=0, stg=0,
      tgt=TG_FOE, ail=AIL_NONE, ach=0, disp=None):
    return (name, el, cat, pw, acc, eff, param, mask, stg, tgt, ail, ach, disp)

MOVES = [
    # --- generic, learned by many ------------------------------------------
    A('BUBBLE BLOW',  'neutral', MC_SPEC, 30),
    A('TACKLE',       'neutral', MC_PHYS, 40),
    A('CLAW ATTACK',  'neutral', MC_PHYS, 50),
    A('QUICK DASH',   'neutral', MC_PHYS, 40, eff=EF_PRIORITY, param=1),
    A('HEADBUTT',     'neutral', MC_PHYS, 70),
    A('BODY PRESS',   'neutral', MC_PHYS, 85, 95),
    A('DATA CRUSH',   'neutral', MC_SPEC, 130, 90, eff=EF_RECHARGE),
    A('GUARD',        'neutral', MC_STATUS, 0, 0, EF_STAGE, 0, ST_DEF | ST_SPD, 1, TG_SELF),
    A('POWER CHARGE', 'neutral', MC_STATUS, 0, 0, EF_STAGE, 0, ST_ATK | ST_SPA, 1, TG_SELF),
    A('FOCUS',        'neutral', MC_STATUS, 0, 0, EF_STAGE, 0, ST_SPE, 2, TG_SELF),
    A('GROWL',        'neutral', MC_STATUS, 0, 100, EF_STAGE, 0, ST_ATK, -1, TG_FOE),
    A('RECOVERY',     'neutral', MC_STATUS, 0, 0, EF_HEAL, 50, tgt=TG_SELF),
    # --- Ver.1 ---------------------------------------------------------------
    A('PEPPER BRTH',  'fire',    MC_SPEC, 45, ail=AIL_BURN, ach=10, disp='PEPPER BREATH'),
    A('ELEC SHOCK',   'electric', MC_SPEC, 50, ail=AIL_PARA, ach=20, disp='ELECTRIC SHOCK'),
    A('GREAT HORNS',  'neutral', MC_PHYS, 80, disp='GREAT HORNS ATTACK'),
    A('MEGA FLAME',   'fire',    MC_SPEC, 90, ail=AIL_BURN, ach=10),
    A('TAIL SMASH',   'neutral', MC_PHYS, 75),
    A('BLAZE BLAST',  'fire',    MC_SPEC, 85, ail=AIL_BURN, ach=10),
    A('TOUCH EVIL',   'dark',    MC_SPEC, 40, ail=AIL_CONFUSE, ach=50, disp='TOUCH OF EVIL'),
    A('DEATH CLAW',   'dark',    MC_PHYS, 90),
    A('ROAR FIRE',    'fire',    MC_SPEC, 60, ail=AIL_BURN, ach=20, disp='ROARING FIRE'),
    A('BURN FIST',    'fire',    MC_PHYS, 85, ail=AIL_BURN, ach=10, disp='BURNING FIST'),
    A('WING SLASH',   'wind',    MC_PHYS, 60),
    A('SPIN NEEDLE',  'wind',    MC_SPEC, 85, disp='SPINNING NEEDLE'),
    A('WATER JET',    'water',   MC_SPEC, 55),
    A('ICE BLAST',    'water',   MC_SPEC, 85, ail=AIL_FREEZE, ach=10),
    A('NUME SLUDGE',  'dark',    MC_SPEC, 50, 95, ail=AIL_POISON, ach=40),
    A('MEGA CLAW',    'neutral', MC_PHYS, 95),
    A('GIGA DESTROY', 'fire',    MC_SPEC, 120, 90, disp='GIGA DESTROYER'),
    A('MAME PUNCH',   'neutral', MC_PHYS, 70, eff=EF_PRIORITY, param=1),
    A('SMILEY BOMB',  'neutral', MC_PHYS, 115, 95, eff=EF_RECOIL, param=4),
    A('LOVELY PUNCH', 'light',   MC_PHYS, 80, disp='LOVELY ATTACK'),
    A('HEARTS ATK',   'light',   MC_SPEC, 105, ail=AIL_CONFUSE, ach=20, disp='HEARTS ATTACK'),
    # --- Ver.2 ---------------------------------------------------------------
    A('PETIT FIRE',   'fire',    MC_SPEC, 45, ail=AIL_BURN, ach=10),
    A('SUPER THUNDR', 'electric', MC_SPEC, 50, ail=AIL_PARA, ach=10, disp='SUPER THUNDER STRIKE'),
    A('HORN CHARGE',  'neutral', MC_PHYS, 70),
    A('ELECTRO SHOK', 'electric', MC_SPEC, 90, ail=AIL_PARA, ach=10, disp='ELECTRO SHOCKER'),
    A('FOX FIRE',     'fire',    MC_SPEC, 70, ail=AIL_BURN, ach=10),
    A('HOWLING BLST', 'water',   MC_SPEC, 90, ail=AIL_FREEZE, ach=10, disp='HOWLING BLASTER'),
    A('ANGEL ROD',    'light',   MC_PHYS, 70),
    A('HAND OF FATE', 'light',   MC_SPEC, 95),
    A('SNOW BALL',    'water',   MC_SPEC, 55),
    A('SUBZERO FIST', 'water',   MC_PHYS, 85, ail=AIL_FREEZE, ach=10, disp='SUBZERO ICE PUNCH'),
    A('FIRE WING',    'fire',    MC_PHYS, 65),
    A('METEOR WING',  'fire',    MC_SPEC, 90, ail=AIL_BURN, ach=10),
    A('BLASTING SPT', 'water',   MC_SPEC, 60, disp='BLASTING SPOUT'),
    A('TIDAL WAVE',   'water',   MC_SPEC, 95, 90),
    A('SWEET BREATH', 'plant',   MC_SPEC, 45, ail=AIL_SLEEP, ach=30),
    A('BONE CRUSH',   'dark',    MC_PHYS, 90),
    A('GROUND ZERO',  'dark',    MC_SPEC, 125, 85),
    A('METAL PUNCH',  'electric', MC_PHYS, 75, eff=EF_PRIORITY, param=1),
    A('ENERGY BOMB',  'electric', MC_SPEC, 115, 95),
    A('UF KISS',      'dark',    MC_SPEC, 60, ail=AIL_CONFUSE, ach=40, disp='U.F. KISS'),
    A('ABDUCT BEAM',  'dark',    MC_SPEC, 105, 95, disp='ABDUCTION BEAM'),
    # --- Ver.3 ---------------------------------------------------------------
    A('BOOM BUBBLE',  'wind',    MC_SPEC, 50),
    A('ELEC THREAD',  'electric', MC_SPEC, 45, ail=AIL_PARA, ach=30, disp='ELECTRIC THREAD'),
    A('HORN STRIKE',  'wind',    MC_PHYS, 70),
    A('AERIAL ATK',   'wind',    MC_SPEC, 90, disp='AERIAL ATTACK'),
    A('SOLAR RAY',    'light',   MC_SPEC, 65),
    A('HUNT CANNON',  'earth',   MC_SPEC, 90, disp='HUNTING CANNON'),
    A('BONE CUDGEL',  'dark',    MC_PHYS, 70),
    A('PUMMEL WHACK', 'dark',    MC_PHYS, 90),
    A('HYPNOTISM',    'dark',    MC_SPEC, 40, 90, ail=AIL_SLEEP, ach=40),
    A('DARK CLAW',    'dark',    MC_PHYS, 85, ail=AIL_CONFUSE, ach=10),
    A('GOLD RUSH',    'water',   MC_PHYS, 65),
    A('HYDRO PRESS',  'water',   MC_SPEC, 90, disp='HYDRO PRESSURE'),
    A('CRUSH DRILL',  'earth',   MC_PHYS, 65, disp='CRUSHING DRILL'),
    A('IRON DRILL',   'earth',   MC_PHYS, 90, disp='IRON DRILL SPIN'),
    A('POOP THROW',   'dark',    MC_SPEC, 50, 95, ail=AIL_POISON, ach=40),
    A('LIGHTN BLADE', 'electric', MC_PHYS, 100, disp='LIGHTNING BLADE'),
    A('GATLING MSL',  'neutral', MC_SPEC, 115, 90, disp='GATLING MISSILE'),
    A('SPIRAL SAW',   'neutral', MC_PHYS, 85),
    A('DEADLY BOMB',  'neutral', MC_PHYS, 120, 95, eff=EF_RECOIL, param=4),
    A('LOVE SEREN',   'dark',    MC_SPEC, 60, 90, ail=AIL_SLEEP, ach=30, disp='LOVE SERENADE'),
    A('DARK SPIRITS', 'dark',    MC_SPEC, 115, 90),
    # --- shared with Ver.4 Champions/Ultimates --------------------------------
    A('POWER GUILLO', 'plant',   MC_PHYS, 70, disp='POWER GUILLOTINE'),
    A('SCISSOR CLAW', 'plant',   MC_PHYS, 90),
    A('HORN RUSH',    'earth',   MC_PHYS, 70),
    A('VOLCANO STRK', 'fire',    MC_SPEC, 90, ail=AIL_BURN, ach=10, disp='VOLCANIC STRIKE'),
    A('PIT PELTER',   'light',   MC_PHYS, 90),
    A('BIT BOMB',     'light',   MC_SPEC, 110, 95),
    # --- Ver.1 Megas ---------------------------------------------------------
    A('TRIDENT ARM',  'fire',    MC_PHYS, 110),
    A('BLITZ CANNON', 'fire',    MC_SPEC, 135, 90, ail=AIL_BURN, ach=10),
    A('BANCHO PUNCH', 'neutral', MC_PHYS, 110, eff=EF_PRIORITY, param=1),
    A('GUTS BOMB',    'neutral', MC_PHYS, 140, 90, eff=EF_RECOIL, param=4),
    A('HUG TACKLE',   'light',   MC_PHYS, 110),
    A('HEART BREAK',  'light',   MC_SPEC, 135, 90, ail=AIL_CONFUSE, ach=20, disp='HEART BREAK ATTACK'),
    # --- Ver.2 Megas ---------------------------------------------------------
    A('TUSK CRUSH',   'dark',    MC_PHYS, 110),
    A('SKULL STAMP',  'dark',    MC_PHYS, 135, 90),
    A('LUNAR CLAW',   'water',   MC_PHYS, 110),
    A('FROST HOWL',   'water',   MC_SPEC, 135, 90, ail=AIL_FREEZE, ach=10),
    A('BRAIN BEAM',   'dark',    MC_SPEC, 105, ail=AIL_CONFUSE, ach=30),
    A('BRAIN RUPTR',  'dark',    MC_SPEC, 135, 90, disp='BRAIN RUPTURE'),
    # --- Ver.3 Ultimate/Megas -----------------------------------------------
    A('DEMON SNAKE',  'dark',    MC_PHYS, 95),
    A('HEAT VIPER',   'fire',    MC_SPEC, 115, 90, ail=AIL_BURN, ach=20),
    A('SPIRAL SWORD', 'electric', MC_PHYS, 115),
    A('ATOMIC RAY',   'electric', MC_SPEC, 135, 90, ail=AIL_PARA, ach=10),
    A('HELL CHAIN',   'dark',    MC_PHYS, 110),
    A('HELL FIRE',    'fire',    MC_SPEC, 135, 90, ail=AIL_BURN, ach=20),
    A('BANCHO FIST',  'fire',    MC_PHYS, 115),
    A('BURN BANCHO',  'fire',    MC_PHYS, 140, 90, eff=EF_RECOIL, param=4, disp='BURNING BANCHO PUNCH'),
    A('TIME BURST',   'dark',    MC_SPEC, 115),
    A('TIME DESTROY', 'dark',    MC_SPEC, 150, 90, eff=EF_RECHARGE, disp='TIME DESTROYER'),
    # --- Ver.4 ---------------------------------------------------------------
    A('SPIRAL TWIST', 'wind',    MC_SPEC, 50, disp='SPIRAL TWISTER'),
    A('POISON IVY',   'plant',   MC_PHYS, 50, ail=AIL_POISON, ach=20),
    A('PECK DRILL',   'wind',    MC_PHYS, 65),
    A('PETRIFY GAZE', 'wind',    MC_SPEC, 85, 90, ail=AIL_PARA, ach=30),
    A('BEAST SWORD',  'earth',   MC_PHYS, 70),
    A('BEAST FIST',   'earth',   MC_SPEC, 90, disp='FIST OF THE BEAST KING'),
    A('SHELL STRIKE', 'water',   MC_PHYS, 65, disp='ANCIENT SHELL'),
    A('TIDE CRASH',   'water',   MC_SPEC, 90),
    A('BONE BOOMER',  'neutral', MC_PHYS, 70, disp='BONE BOOMERANG'),
    A('ICE CLOUD',    'water',   MC_SPEC, 85, ail=AIL_FREEZE, ach=10),
    A('NANI PUNCH',   'dark',    MC_PHYS, 50, 95, ail=AIL_CONFUSE, ach=30),
    A('DARK CLAW S',  'dark',    MC_PHYS, 100, disp='DARK SIDE CLAW'),
    A('GENOCIDE ATK', 'dark',    MC_SPEC, 115, 90, disp='GENOCIDE ATTACK'),
    A('EGG BOMB',     'neutral', MC_PHYS, 100),
    A('NIGHTMARE',    'dark',    MC_SPEC, 110, 90, ail=AIL_SLEEP, ach=20, disp='NIGHTMARE SYNDROME'),
    A('DARK LANCE',   'dark',    MC_PHYS, 115),
    A('GIGA DEATH',   'dark',    MC_SPEC, 140, 90, eff=EF_RECHARGE),
    A('BLOOM THORN',  'plant',   MC_PHYS, 110),
    A('PETAL STORM',  'plant',   MC_SPEC, 135, 90, eff=EF_DRAIN, param=25),
    A('IRON FIST',    'earth',   MC_PHYS, 115),
    A('GANKOO SMASH', 'earth',   MC_PHYS, 140, 90, eff=EF_RECOIL, param=4),
    # --- Ver.5 ---------------------------------------------------------------
    A('STUN BLAST',   'electric', MC_SPEC, 50, ail=AIL_PARA, ach=20, disp='ELECTRIC STUN BLAST'),
    A('SAW SPIN',     'water',   MC_PHYS, 50),
    A('FIRE BLAST',   'fire',    MC_SPEC, 90, 90, ail=AIL_BURN, ach=20),
    A('BEAM ARM',     'neutral', MC_PHYS, 70),
    A('HYPER HEAT',   'fire',    MC_SPEC, 90, ail=AIL_BURN, ach=10),
    A('RED EYE',      'dark',    MC_SPEC, 60, ail=AIL_PARA, ach=30, disp='RED EYE GLARE'),
    A('CRIMSON CLAW', 'dark',    MC_PHYS, 90),
    A('HORN ATTACK',  'earth',   MC_PHYS, 65),
    A('SLAM TUSK',    'earth',   MC_PHYS, 90, disp='SLAMMING TUSK'),
    A('POISON POWDR', 'wind',    MC_SPEC, 55, ail=AIL_POISON, ach=30, disp='POISON POWDER'),
    A('DEADLY STING', 'wind',    MC_PHYS, 90),
    A('SNAKE BITE',   'dark',    MC_PHYS, 65, ail=AIL_POISON, ach=20),
    A('TRIPLE FORCE', 'dark',    MC_SPEC, 90),
    A('TOXIC SLUDGE', 'dark',    MC_SPEC, 50, 95, ail=AIL_POISON, ach=40),
    A('MISSILE RAIN', 'neutral', MC_SPEC, 100),
    A('NUKE LASER',   'fire',    MC_SPEC, 120, 90, disp='NUCLEAR LASER'),
    A('NANO TWIST',   'electric', MC_PHYS, 90),
    A('PLUG BOMB',    'electric', MC_SPEC, 115, 90),
    A('DINO KICK',    'earth',   MC_PHYS, 100),
    A('PYRO BLASTER', 'fire',    MC_SPEC, 120, 90, ail=AIL_BURN, ach=10),
    A('GIGA CANNON',  'electric', MC_SPEC, 120),
    A('INFINITY CAN', 'neutral', MC_SPEC, 150, 90, eff=EF_RECHARGE, disp='INFINITY CANNON'),
    A('THUNDR LANCE', 'electric', MC_PHYS, 115, disp='THUNDER LANCE'),
    A('RAIDEN STORM', 'electric', MC_SPEC, 135, 90, ail=AIL_PARA, ach=20),
    A('TWIN BLADE',   'fire',    MC_PHYS, 115),
    A('METEOR SLASH', 'fire',    MC_SPEC, 135, 90),
    # --- fallback, never learned: used when a slot list comes back empty ------
    A('STRUGGLE',     'neutral', MC_PHYS, 50, 0, EF_RECOIL, 4),
]

# Generic attacks, localised. Signature attacks keep their (English) names in
# every language -- that is how the series itself handles them.
# Order: ES, EN, FR, DE, IT, PT. ASCII only (the bitmap font has no accents).
GENERIC_NAMES = {
    'BUBBLE BLOW':  ['BURBUJAS',        'BUBBLE BLOW',  'BULLES',        'BLUBBERBLASE',  'BOLLE',          'BOLHAS'],
    'TACKLE':       ['PLACAJE',         'TACKLE',       'CHARGE',        'TACKLE',        'AZIONE',         'INVESTIDA'],
    'CLAW ATTACK':  ['GARRAZO',         'CLAW ATTACK',  'GRIFFE',        'KRALLENHIEB',   'ARTIGLIO',       'GARRA'],
    'QUICK DASH':   ['CARRERA',         'QUICK DASH',   'RUEE',          'BLITZSPRINT',   'SCATTO',         'DISPARADA'],
    'HEADBUTT':     ['COCAZO',          'HEADBUTT',     'COUP DE TETE',  'KOPFSTOSS',     'TESTATA',        'CABECADA'],
    'BODY PRESS':   ['PLANCHA',         'BODY PRESS',   'ECRASEMENT',    'BODYSLAM',      'SCHIACCIATA',    'PRENSA'],
    'DATA CRUSH':   ['APLASTADATOS',    'DATA CRUSHER', 'BROIE-DONNEES', 'DATENBRECHER',  'SCHIACCIADATI',  'ESMAGA-DADOS'],
    'GUARD':        ['GUARDIA',         'GUARD',        'GARDE',         'ABWEHR',        'GUARDIA',        'GUARDA'],
    'POWER CHARGE': ['CARGA',           'POWER CHARGE', 'CHARGE FORCE',  'KRAFTLADUNG',   'CARICA',         'CARGA'],
    'FOCUS':        ['CONCENTRACION',   'FOCUS',        'CONCENTRATION', 'FOKUS',         'CONCENTRAZIONE', 'FOCO'],
    'GROWL':        ['GRUNIDO',         'GROWL',        'GROGNEMENT',    'KNURREN',       'RINGHIO',        'ROSNADO'],
    'RECOVERY':     ['RECUPERACION',    'RECOVERY',     'SOIN',          'REPARATUR',     'RECUPERO',       'RECUPERAR'],
    'STRUGGLE':     ['FORCEJEO',        'STRUGGLE',     'LUTTE',         'VERZWEIFLER',   'SCONTRO',        'DEBATER'],
}

# Generic attacks added by stage, as (move, level offset from stage entry).
# Offset None = level 1: a form met in the wild or a boss team always has
# something to fight with.
STAGE_GENERIC = {
    0: [('BUBBLE BLOW', None)],
    1: [('BUBBLE BLOW', None), ('TACKLE', 0)],
    2: [('TACKLE', None), ('GROWL', 3), ('CLAW ATTACK', 6), ('GUARD', 10), ('QUICK DASH', 13)],
    3: [('TACKLE', None), ('HEADBUTT', 8), ('POWER CHARGE', 12), ('BODY PRESS', 16), ('RECOVERY', 20)],
    4: [('HEADBUTT', None), ('FOCUS', 4), ('DATA CRUSH', 14)],
    5: [('BODY PRESS', None), ('RECOVERY', 6), ('DATA CRUSH', 10)],
}
# Where the signature attacks land, as level offsets from stage entry:
# the first the moment it digivolves, the second a few hours later.
SIG_OFFSETS = [0, 5]

# ---------------------------------------------------------------------------
# Boss ladders ("gyms"): 8 areas, 4 elite and a champion per version. Teams
# are (dex, level). Levels follow the stage clock: a Champion team is only
# met once a Champion of your own is possible.
AREAS = {
    V1: ['NATIVE FOREST', 'GEAR SAVANNA', 'DRILL TUNNEL', 'TROPIC JUNGLE',
         'OVERDELL', 'FREEZELAND', 'MT. PANORAMA', 'GREYLORD HALL'],
    V2: ['PRIMARY VLG', 'MISTY TREES', 'GECKO SWAMP', 'BEETLE LAND',
         'GREAT CANYON', 'ICE SANCTUARY', 'DATAMON RUIN', 'FACTORIAL'],
    V3: ['TOY TOWN', 'SPIRAL MTN', 'LOST VILLAGE', 'SERVER DESERT',
         'GEAR CITY', 'DARK FOREST', 'KOROMON VLG', 'INFINITY MTN'],
    V4: ['FLOWER FIELD', 'BIRD CLIFF', 'BEAST PLAINS', 'INSECT WOODS',
         'ANCIENT SEA', 'SNOW VALLEY', 'TOY BOX', 'EGG TEMPLE'],
    V5: ['SEWER', 'JUNK YARD', 'DINO GORGE', 'SWAMP OF FLIES',
         'DEVIL PEAK', 'TRIPLE CAVE', 'METAL CITY', 'MACHINE CORE'],
}


def _ladder(ver):
    """Eight area bosses, four elite and a champion, built from the version's
    own tree so the bosses climb the same stages the player does. Levels never
    sit below a stage's entry level, so nothing appears before it could exist."""
    mons = [r for r in ROSTER if r[1] == ver]
    by = lambda st: [r[0] for r in mons if r[4] == st]
    b1, b2, rk, ch, ul, mg = (by(i) for i in range(6))
    bn = {r[0]: r for r in ROSTER}
    el = lambda d: bn[d][6]
    nm = lambda d: bn[d][3]
    # Ver.2 and Ver.4 open with a two-Digimon boss, the others with three, so
    # the ladders are not all the same shape (touch_test relies on that).
    first = [(b1[0], 3), (b2[0], 4), (b2[0], 5)]
    if [v[0] for v in VERSIONS].index(ver) % 2:
        first = [(b2[0], 4), (b2[0], 5)]
    out = [(nm(b2[0]), el(b2[0]), first),
           (nm(rk[0]), el(rk[0]), [(rk[1], 8), (rk[0], 9)]),
           (nm(rk[1]), el(rk[1]), [(rk[0], 12), (rk[1], 13), (rk[1], 14)])]
    strong = ch[:-1]                       # the fail form is not a boss
    for i in range(5):
        a, b = strong[i % len(strong)], strong[(i + 1) % len(strong)]
        out.append((nm(b), el(b), [(rk[i % 2], 22 + 3 * i), (a, 26 + 3 * i), (b, 28 + 3 * i)]))
    for i in range(4):
        u, u2 = ul[i % len(ul)], ul[(i + 1) % len(ul)]
        out.append((nm(u2), el(u2), [(strong[i], 44 + 2 * i), (u, 49 + 2 * i), (u2, 51 + 2 * i)]))
    out.append((nm(mg[-1]), el(mg[-1]),
                [(ul[0], 56)] + [(m, 62 + 2 * k) for k, m in enumerate(mg)]))
    return out


LADDERS = {v[0]: _ladder(v[0]) for v in VERSIONS}
for _v in AREAS:
    AREAS[_v] = AREAS[_v] + ['ELITE'] * 4 + ['CHAMPION']


# ---------------------------------------------------------------------------
# Derived helpers used by the generators and the tests.
def by_num():
    return {r[0]: r for r in ROSTER}


def evolve_level(num):
    """The level a form is offered its digivolution: the NEXT stage's entry,
    or 0 if nothing leads out of it."""
    stage = by_num()[num][4]
    if not any(e[0] == num for e in EVOLUTIONS):
        return 0
    return STAGE_ENTRY[stage + 1]


def first_target(num):
    for e in EVOLUTIONS:
        if e[0] == num:
            return e[1]
    return 0


def base_stats(num):
    _n, _v, _s, _d, stage, _a, _e, prof, _m = by_num()[num]
    b = STAGE_BASE[stage]
    # a small per-species wobble so two forms of one profile are not twins
    wob = [((num * 37 + i * 11) % 9) - 4 for i in range(6)]
    return tuple(max(5, min(160, round(b * f) + w)) for f, w in zip(PROFILES[prof], wob))


def learnset(num):
    """[(move NAME, level)] in level order."""
    _n, _v, _s, _d, stage, _a, _e, _p, sigs = by_num()[num]
    entry = STAGE_ENTRY[stage]
    out = []
    for mv, off in STAGE_GENERIC[stage]:
        out.append((mv, 1 if off is None else entry + off))
    for i, mv in enumerate(sigs):
        out.append((mv, entry + SIG_OFFSETS[min(i, len(SIG_OFFSETS) - 1)]))
    seen, uniq = set(), []
    for mv, lv in sorted(out, key=lambda x: x[1]):
        if mv in seen:
            continue
        seen.add(mv)
        uniq.append((mv, lv))
    return uniq


def validate():
    nums = [r[0] for r in ROSTER]
    assert nums == list(range(1, len(ROSTER) + 1)), 'dex numbers must be 1..N'
    names = {m[0] for m in MOVES}
    for r in ROSTER:
        assert len(r[3]) <= 15, r[3]
        for mv in r[8]:
            assert mv in names, (r[3], mv)
    for m in MOVES:
        assert len(m[0]) <= 12, m[0]
        assert m[1] in ELEMENTS, m
        disp = m[12] or m[0]
        assert len(disp) <= 23, disp
    lo = 1
    for name, a, b, st in VERSIONS:
        assert a == lo, name
        lo = b + 1
        for s in st:
            assert by_num()[s][4] == 0, 'a version hatches its Baby I'
    assert lo == len(ROSTER) + 1, 'versions must tile the roster'
    bn = by_num()
    for e in EVOLUTIONS:
        f, t = e[0], e[1]
        assert bn[t][4] == bn[f][4] + 1, ('a branch goes up exactly one stage', e)
    for v, ladder in LADDERS.items():
        assert len(ladder) == 13, v
        for name, el, team in ladder:
            assert el in ELEMENTS
            for d, lv in team:
                assert lv >= STAGE_ENTRY[bn[d][4]], ('boss below its stage', v, name, d, lv)
    return True


if __name__ == '__main__':
    validate()
    print('ok: %d Digimon, %d attacks, %d branches' % (len(ROSTER), len(MOVES), len(EVOLUTIONS)))
