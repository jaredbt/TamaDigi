#!/usr/bin/env python3
"""Generates the Time Stranger data for the reworked firmware.

Reads tools/data/time-stranger/ (stats.tsv, attributes.tsv, resistances.tsv,
evolutions.json, skills.json) and applies the rules in docs/HANDOFF.md's
"Game design decisions" table. Writes two headers at the repository root:

  ts_dex.h     the 469 v1 forms, their evolution routes, Jogress pairs and the
               13-boss ladder
  ts_skills.h  the 44 learnable skills, the signature skills and each form's
               learnset

Nothing includes them yet: the firmware cutover will. This pipeline is
separate from tools/digimon_data.py (which still feeds dex.h, moves.h and the
rest of today's build). Every rule is asserted before anything is written
(see validate()), and the run ends with a short summary. The struct layouts
and field meanings are documented in docs/ts-data-format.md.

    python3 tools/gen_ts_data.py           # validate, write both headers
    python3 tools/gen_ts_data.py --check   # validate, fail if the headers are stale
"""
import csv
import json
import math
import os
import re
import sys
import unicodedata
from collections import Counter, defaultdict, deque

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DATA = os.path.join(HERE, 'data', 'time-stranger')

# ---------------------------------------------------------------- the rules

STAGES = ['In-Training I', 'In-Training II', 'Rookie', 'Champion', 'Ultimate', 'Mega', 'Mega +',
          'Armor', 'Hybrid']
RAISABLE_STAGES = STAGES[:7]
STAGE_IDS = ['IN_TRAINING_1', 'IN_TRAINING_2', 'ROOKIE', 'CHAMPION', 'ULTIMATE', 'MEGA', 'MEGA_PLUS',
             'ARMOR', 'HYBRID']
# Level at which each raisable stage may first be reached (the 6/24/48/60/72 h
# stage timers; level = hours since hatching + 1; Mega+ from 72 h, Q11).
STAGE_ENTRY = [1, 2, 7, 25, 49, 61, 73]
ATTRS = ['Vaccine', 'Data', 'Virus', 'Free', 'Variable', 'Unknown', 'No Data']
ELEMENTS = ['Fire', 'Water', 'Plant', 'Ice', 'Electricity', 'Earth', 'Steel', 'Wind', 'Light', 'Dark',
            'Null']
# Stored stat order: HP, ATK, DEF, speed, magic attack (INT), magic defence (SPI). SP is dropped.
STATS = ['HP', 'ATK', 'DEF', 'SPD', 'INT', 'SPI']
STAT_IDS = ['HP', 'ATK', 'DEF', 'SPD', 'MATK', 'MDEF']

# Out of v1 entirely (2026-10-28).
ON_ICE = {'Aegiomon', 'Aegiochusmon', 'Aegiochusmon: Green', 'Aegiochusmon: Dark',
          'Aegiochusmon: Blue', 'Aegiochusmon: Holy'}
# Need a Time Stranger mechanic TamaDigi lacks (Agent Skills, Talent, items): wild-only.
WILD_ONLY = {'Jupitermon', 'Jupitermon WM', 'Chronomon HM', 'Chronomon DM', 'Agumon (Bond of Bravery)',
             'Gabumon (Bond of Friendship)', 'Lucemon', 'Lucemon CM', 'Lucemon SM', 'Parallelmon',
             'Imperialdramon PM', 'Magnamon (X Antibody)'}
# Reached only by Jogress (a v1 stretch goal): wild-only until it ships. The
# 16 HANDOFF names, plus Susanomon, whose partners are both Hybrids.
FUSION_ONLY = {'Omnimon', 'Omnimon Zwart', 'Omnimon Alter-B', 'Omnimon Alter-S', 'Omnimon Zwart Defeat',
               'Omnimon MM', 'Omnimon (X Antibody)', 'Millenniummon', 'Mastemon', 'GraceNovamon',
               'Alphamon: Ouryuken', 'Chaosmon', 'Chaosmon: Valdur Arm', 'Examon', 'Craniamon + Enbarrmon',
               'Titamon + SkullBaluchimon', 'Susanomon'}
# When Jogress ships, this makes the fusion-only forms raisable and brings back
# the routes among them (Omnimon (X Antibody) -> Omnimon and the like). It is
# not a one-line flip: validate() must then also walk the Jogress pairs in its
# reachability check, and its expected counts (419 / 33 / 17) change.
JOGRESS_SHIPPED = False
# Armor and Hybrid are wild-only, met in the wild at this stat tier (Stages row).
WILD_TIER = {
    'Champion': ['Submarimon', 'Shurimon', 'Digmon', 'Nefertimon', 'Flamedramon', 'Pegasusmon', 'Halsemon',
                 'Lighdramon', 'Agunimon', 'Lobomon', 'Lanamon', 'BurningGreymon', 'Calmaramon',
                 'KendoGarurumon'],
    'Ultimate': ['Aldamon', 'Beowolfmon'],
    'Mega': ['Rapidmon (Armor)', 'Magnamon', 'EmperorGreymon', 'MagnaGarurumon', 'MagnaGarurumon (Detached)'],
}
# Agent Rank counts only from Champion -> Ultimate on (Q1): by the target's stage.
RANK_FROM_STAGE = 'Ultimate'

# Display names: upper case, "()" and ":" dropped, at most 15 characters.
# These are the 29 that come out longer.
SHORT_NAMES = {
    'Coredramon (Green)': 'COREDRAMON GRN',
    'DarkSuperStarmon': 'DKSUPERSTARMON',
    'BlackKingNumemon': 'BLKKINGNUMEMON',
    'UlforceVeedramon': 'ULFORCEVDRAMON',
    'Cherubimon (Black)': 'CHERUBIMON BLK',
    'HerculesKabuterimon': 'HERCKABUTERIMON',
    'UltimateBrachiomon': 'ULTBRACHIOMON',
    'MedievalGallantmon': 'MEDGALLANTMON',
    'TyrantKabuterimon': 'TYRKABUTERIMON',
    'Imperialdramon DM': 'IMPERDRAMON DM',
    'Imperialdramon FM': 'IMPERDRAMON FM',
    'Imperialdramon PM': 'IMPERDRAMON PM',
    'MagnaGarurumon (Detached)': 'MAGNAGARURU DET',
    'Alphamon: Ouryuken': 'ALPHA OURYUKEN',
    'Craniamon + Enbarrmon': 'CRANIA+ENBARR',
    'Titamon + SkullBaluchimon': 'TITA+SKULLBALU',
    'Chaosmon: Valdur Arm': 'CHAOSMON VALDUR',
    'MirageGaogamon BM': 'MIRAGEGAOGA BM',
    'Agumon (Bond of Bravery)': 'AGUMON BRAVERY',
    'Gabumon (Bond of Friendship)': 'GABUMON FRIEND',
    'Omnimon Zwart Defeat': 'OMNIMON ZWART D',
    'UlforceVeedramon (X Antibody)': 'ULFORCEVDRMON X',
    'Magnamon (X Antibody)': 'MAGNAMON X',
    'Jesmon (X Antibody)': 'JESMON X',
    'Gallantmon (X Antibody)': 'GALLANTMON X',
    'Omnimon (X Antibody)': 'OMNIMON X',
    'MetalGreymon (Blue)': 'METALGREY BLUE',
    'WereGarurumon (Black)': 'WEREGARURU BLK',
    'MetalGarurumon (Black)': 'METALGARURU BLK',
}
NAME_MAX = 15

# The boss ladder (Q10e): name, level, stage, kind.
BOSSES = [('DemiDevimon', 15, 'Rookie', 'AREA'), ('Ogremon', 22, 'Champion', 'AREA'),
          ('Unimon', 32, 'Champion', 'AREA'), ('Devimon', 40, 'Champion', 'AREA'),
          ('Etemon', 47, 'Ultimate', 'AREA'), ('Myotismon', 54, 'Ultimate', 'AREA'),
          ('Mummymon', 58, 'Ultimate', 'AREA'), ('Arukenimon', 60, 'Ultimate', 'AREA'),
          ('MetalSeadramon', 64, 'Mega', 'ELITE'), ('MetalEtemon', 66, 'Mega', 'ELITE'),
          ('MaloMyotismon', 68, 'Mega', 'ELITE'), ('Diaboromon', 70, 'Mega', 'ELITE'),
          ('Apocalymon', 80, 'Mega +', 'CHAMPION')]

# Learnable skills (Q14, "Learnable skills"): attachment rows fold into one
# skill per I/II/III family, all-enemies families into their single-target
# twin. Support skills are matched on the description, never the name.
SUPPORT = [  # (skill name, description pattern of every row folded into it)
    ('Attack Charge', r'boost to ATK'), ('Attack Bind', r'reduction to ATK'),
    ('Guard Charge', r'boost to DEF'), ('Guard Bind', r'reduction to DEF'),
    ('Magic Charge', r'boost to INT'), ('Magic Bind', r'reduction to INT'),
    ('Mental Charge', r'boost to SPI'), ('Spirit Bind', r'reduction to SPI'),
    ('Speed Charge', r'boost to SPD'), ('Speed Bind', r'reduction to SPD'),
    ('Acceleration Boost', r'apply Charge Physical'), ('Spell Boost', r'apply Charge Magic'),
    ('Heal', r'^\[Target: (1 ally|All allies)\] (Slightly|Moderately) recovers HP\.$'),
    ('X-Heal', r'^\[Target: (1 ally|All allies)\] (Greatly|Moderately) recovers HP\.$'),
    ('Restore', r'Removes (status abnormalities|Injury)'),
    ('Reset Body', r'Removes stat buffs'), ('Dispel', r'Removes stat debuffs'),
    ('Idle Bubble', r'apply Sleep'), ('Panic Wisp', r'apply Confusion'),
    ('Ripping Net', r'apply Paralysis'), ('Venom Trap', r'apply Poison'),
    ('Texture Blow', r'apply Crystallization'),
]
# Aura ("slightly", all allies) folds into Heal and X-Aura ("moderately", all
# allies) into X-Heal, the way a Field folds into its Charge.
HEAL_FOLD = {'Heal': 'Heal', 'Aura': 'Heal', 'X-Heal': 'X-Heal', 'X-Aura': 'X-Heal'}
# Party-only, crit, or unused: dropped, each for a named reason.
DROP_SUPPORT = [
    (r'Reflect (Physical|Magic)', 'party-only (Reflect)'), (r'apply Taunting', 'party-only (Taunt)'),
    (r'apply Guts', 'party-only (Guts)'), (r'apply Reverse', 'party-only (Reverse)'),
    (r'Revives from KO', 'party-only (Revive)'), (r'Immunity', 'party-only (immunity barrier)'),
    (r'CRT Rate', 'crit up/down'),
]

# Signature skills (Q14, "Signature skills").
SIG_DROPPED = ['Attract Echo', 'Brain Rupture', 'Breath of the Gods', 'Table Flip', 'Purge Shine']
SIG_COUNTERS = ['Maul Attack', "Dragon's Roar", 'Graceful Lance', 'Weltgeist']
PCT_HP_POWER = 4          # percent-of-HP damage -> ordinary power: power = 4 x percent
STATUSES = ['Paralysis', 'Injury', 'Poison', 'Sleep', 'Crystallization', 'Confusion']
AIL_IDS = {'Paralysis': 'PARALYSIS', 'Injury': 'INJURY', 'Poison': 'POISON', 'Sleep': 'SLEEP',
           'Crystallization': 'CRYSTAL', 'Confusion': 'CONFUSION'}
ail = lambda s: STATUSES.index(s) + 1      # TS_AIL_* value; 0 = none
STATUS_MAP = {'paralysis': 'Paralysis', 'injury': 'Injury', 'poison': 'Poison', 'deadlypoison': 'Poison',
              'sleep': 'Sleep', 'deepslumber': 'Sleep', 'crystallization': 'Crystallization',
              'confusion': 'Confusion',
              # Not one of the six: Immobilization (and "Immobilized next turn"
              # cast on the foe) costs the target its turn; Paralysis is closest.
              'immobilization': 'Paralysis', 'immobilizednextturn': 'Paralysis'}
STATUS_DROPPED = {'chaos', 'instantdeath', 'reverse'}
# One effect per signature. When several survive, the first in this order is
# kept: drawbacks first (the skill's power assumes them), then a status, then
# a heal (a healing signature's main action), then the rest.
EFFECT_ORDER = ['RECOIL', 'RECHARGE', 'STATUS', 'HEAL', 'STAT', 'DRAIN', 'FIRST', 'NEVER_MISS']
EFFECTS = ['NONE', 'STATUS', 'STAT', 'DRAIN', 'RECOIL', 'HEAL', 'FIRST', 'NEVER_MISS', 'RECHARGE',
           'CHARGE_PHYS', 'CHARGE_MAGIC', 'CURE', 'REMOVE_BUFFS', 'REMOVE_DEBUFFS']
STAT_BITS = {'ATK': 1, 'DEF': 2, 'SPD': 4, 'INT': 8, 'SPI': 16}

norm = lambda s: re.sub(r'[^a-z0-9]+', '', s.lower())


class DataError(Exception):
    pass


def need(cond, msg):
    if not cond:
        raise DataError(msg)


def ascii_upper(s):
    s = unicodedata.normalize('NFKD', s).encode('ascii', 'ignore').decode()
    return s.upper()


def display_name(name):
    if name in SHORT_NAMES:
        return SHORT_NAMES[name]
    return re.sub(r' +', ' ', re.sub(r'[():]', '', ascii_upper(name))).strip()


def tsv(name):
    with open(os.path.join(DATA, name), newline='') as f:
        return list(csv.DictReader(f, delimiter='\t'))


def jload(name):
    with open(os.path.join(DATA, name)) as f:
        return json.load(f)


# ---------------------------------------------------------------- roster

def load_roster():
    rows = tsv('stats.tsv')
    attrs, res = tsv('attributes.tsv'), tsv('resistances.tsv')
    need(len(rows) == len(attrs) == len(res) == 475, 'expected 475 rows in each roster table')
    forms, by_name = [], {}
    for i, (r, a, x) in enumerate(zip(rows, attrs, res), 1):
        need(a['name'] == r['name'] == x['name'] and int(a['dex']) == int(x['dex']) == i,
             f'row {i}: roster tables out of step ({r["name"]}, {a["name"]}, {x["name"]})')
        need(r['stage'] in STAGES, f'{r["name"]}: unknown stage {r["stage"]}')
        need(a['attribute'] in ATTRS, f'{r["name"]}: unknown attribute {a["attribute"]}')
        f = {'ts_dex': i, 'name': r['name'], 'stage': r['stage'], 'personality': r['personality'],
             'attr': a['attribute'],
             'lv1': {k: int(r[k]) for k in STATS + ['SP']}, 'max': {k: int(r['m' + k]) for k in STATS + ['SP']},
             'resist': []}
        for e in ELEMENTS:
            v = float(x[e])
            need(v in (0, 0.5, 1, 1.5, 2), f'{r["name"]}: resistance {e} = {x[e]}')
            f['resist'].append(int(v * 10))
        need(r['name'] not in by_name, f'duplicate name {r["name"]}')
        by_name[r['name']] = f
        forms.append(f)
    for n in ON_ICE | WILD_ONLY | FUSION_ONLY | set(SHORT_NAMES) | {n for v in WILD_TIER.values() for n in v}:
        need(n in by_name, f'rule names a Digimon not in the roster: {n}')
    v1 = [f for f in forms if f['name'] not in ON_ICE]
    for i, f in enumerate(v1, 1):
        f['id'] = i
        f['display'] = display_name(f['name'])
    tier_of = {n: t for t, ns in WILD_TIER.items() for n in ns}
    for f in v1:
        armor_hybrid = f['stage'] in ('Armor', 'Hybrid')
        need(armor_hybrid == (f['name'] in tier_of), f'{f["name"]}: Armor/Hybrid without a wild tier, or the reverse')
        f['tier'] = tier_of.get(f['name'], f['stage'])
        f['wild_only'] = armor_hybrid or f['name'] in WILD_ONLY
        f['fusion_only'] = f['name'] in FUSION_ONLY and not JOGRESS_SHIPPED
        f['raisable'] = not f['wild_only'] and not f['fusion_only']
    return v1, {f['name']: f for f in v1}, by_name


# ---------------------------------------------------------------- evolution

def lvl_for(f, stat, value):
    """First level (1..99) at which f's untrained stat reaches value, linear lv1 -> lv99; None if never."""
    a, b = f['lv1'][stat], f['max'][stat]
    for lv in range(1, 100):
        if a * 98 + (b - a) * (lv - 1) >= value * 98:
            return lv
    return None


PERSONALITIES = None


def strip_personality(name, by_name):
    m = re.fullmatch(r'(.*) \((\w+)\)', name)
    need(m and m.group(2) in PERSONALITIES, f'Jogress partner without a personality: {name}')
    need(m.group(1) in by_name, f'Jogress partner not in the roster: {name}')
    return m.group(1)


def build_evolutions(v1, by_v1, by_name):
    global PERSONALITIES
    PERSONALITIES = {f['personality'] for f in by_name.values()}
    evo = jload('evolutions.json')
    need(set(evo) == set(by_name), 'evolutions.json and stats.tsv list different Digimon')
    routes, jog, dropped = [], {}, Counter()
    sp_over = []
    for f in v1:
        need(evo[f['name']]['stage'] == f['stage'], f'{f["name"]}: stage differs in evolutions.json')
    for src in [f['name'] for f in by_name.values()]:
        for to, cond in evo[src]['evolves_into']:
            need(to in by_name, f'route target not in the roster: {src} -> {to}')
            unknown = [k for k in cond if k not in STATS + ['SP', 'Agent Rank', 'Jogress', 'Req. Item', 'Talent']
                       and not k.endswith('(Agent Skills)')]
            need(not unknown, f'{src} -> {to}: unknown condition {unknown}')
            if 'Jogress' in cond:
                a, b = (strip_personality(n, by_name) for n in cond['Jogress'])
                need(src in (a, b), f'{src} -> {to}: source is not one of its Jogress partners')
                need(set(cond) <= set(STATS) | {'Agent Rank', 'Jogress'}, f'{src} -> {to}: odd Jogress conditions')
                key = (tuple(sorted((a, b), key=lambda n: by_name[n]['ts_dex'])), to)
                req = {k: v for k, v in cond.items() if k != 'Jogress'}
                need(jog.setdefault(key, req) == req, f'Jogress {key}: the two listings disagree')
                dropped['Jogress'] += 1
                continue
            if 'Req. Item' in cond:
                dropped['item'] += 1
                continue
            if any(k.endswith('(Agent Skills)') for k in cond):
                dropped['Agent Skills'] += 1
                continue
            if 'Talent' in cond:
                dropped['Talent'] += 1
                continue
            if src in ON_ICE or to in ON_ICE:
                dropped['Aegiomon line'] += 1
                continue
            fs, ft = by_v1[src], by_v1[to]
            if not (fs['raisable'] and ft['raisable']):
                dropped['touches a non-raisable form'] += 1
                continue
            r = {'from': fs, 'to': ft, 'min': {k: cond.get(k, 0) for k in STATS}, 'ts_rank': cond.get('Agent Rank', 0),
                 'min_level': 0, 'sp': cond.get('SP')}
            if STAGES.index(ft['stage']) >= STAGES.index(RANK_FROM_STAGE):
                r['rank'] = r['ts_rank']
            else:
                r['rank'] = 0
            if 'SP' in cond:
                lv = lvl_for(fs, 'SP', cond['SP'])
                if lv is None:
                    # A minimum the source can never reach is dropped: Agent Rank
                    # and the trainable stats still gate these routes.
                    sp_over.append((src, to, cond['SP'], fs['max']['SP']))
                    lv = 0
                r['min_level'] = lv
            routes.append(r)
    jogress = []
    for ((a, b), to), req in sorted(jog.items(), key=lambda kv: (by_name[kv[0][1]]['ts_dex'], kv[0][0])):
        if to in ON_ICE:
            continue
        fa, fb, ft = by_v1.get(a), by_v1.get(b), by_v1[to]
        need(fa and fb, f'Jogress partner out of v1: {a} + {b}')
        jogress.append({'a': fa, 'b': fb, 'to': ft, 'min': {k: req.get(k, 0) for k in STATS},
                        'rank': req.get('Agent Rank', 0), 'partners_raisable': fa['raisable'] and fb['raisable']})
    return routes, jogress, dropped, sp_over


# ---------------------------------------------------------------- learnable skills

def build_learnables(att):
    """The 44 learnable skills, and each attachment row's fate: (skill index, tier) or a drop reason."""
    fam = defaultdict(list)          # (family name) -> [(tier, row)]
    fate = {}
    for i, a in enumerate(att):
        m = re.fullmatch(r'(.+) (I{1,3})', a['name'])
        if a['damage'] in ('Physical', 'Magic'):
            need(m and a['icon'] in ELEMENTS, f'attack skill without a tier or element: {a["name"]}')
            fam[m.group(1)].append((len(m.group(2)), i))
        else:
            need(not m, f'support skill with a tier: {a["name"]}')
    attacks, by_pair = [], defaultdict(dict)
    for name, tiers in fam.items():
        rows = [att[i] for _, i in sorted(tiers)]
        need(sorted(t for t, _ in tiers) == [1, 2, 3], f'{name}: not three tiers')
        need(len({(r['icon'], r['damage'], r['acc']) for r in rows}) == 1, f'{name}: tiers differ in kind')
        tgt = re.match(r'\[Target: (1 enemy|All enemies)\]', rows[0]['desc'])
        need(tgt, f'{name}: unknown target')
        hits = re.search(r'x (\d+) hits', rows[0]['desc'])
        powers = []
        for r in rows:
            p = re.search(r'at power (\d+)', r['desc'])
            need(p and int(p.group(1)) == int(r['power']), f'{r["name"]}: power column and text disagree')
            powers.append(int(r['power']))
        single = tgt.group(1) == '1 enemy'
        pair = (rows[0]['icon'], rows[0]['damage'])
        need('single' if single else 'all' not in by_pair[pair], f'{pair}: two families of one kind')
        by_pair[pair]['single' if single else 'all'] = {
            'name': name, 'element': rows[0]['icon'], 'cat': rows[0]['damage'], 'power': powers,
            'hits': int(hits.group(1)) if hits else 1, 'acc': int(rows[0]['acc'].rstrip('%')),
            'rows': [i for _, i in sorted(tiers)]}
    need(len(by_pair) == 22, f'{len(by_pair)} element x physical/magic pairs, expected 22')
    skills = []
    for e in ELEMENTS:
        for cat in ('Physical', 'Magic'):
            p = by_pair[(e, cat)]
            need(set(p) == {'single', 'all'}, f'{e} {cat}: needs one single-target and one all-enemies family')
            s = dict(p['single'], kind='attack', effect='NONE', fold=p['all']['name'])
            idx = len(skills)
            for t, row in enumerate(p['single']['rows'] + p['all']['rows']):
                fate[row] = (idx, t % 3)
            skills.append(s)
    for name, pat in SUPPORT:
        rows = [i for i, a in enumerate(att) if a['damage'] not in ('Physical', 'Magic') and i not in fate
                and re.search(pat, a['desc'])]
        if name in ('Heal', 'X-Heal'):
            rows = [i for i in rows if HEAL_FOLD.get(att[i]['name']) == name]
        need(rows, f'support skill {name}: no attachment row')
        main_ = [i for i in rows if att[i]['name'] == name]
        need(len(main_) == 1, f'support skill {name}: its own row is missing')
        a = att[main_[0]]
        s = {'name': name, 'element': None, 'cat': 'Support', 'power': [int(a['power'])] * 3, 'hits': 0,
             'acc': int(a['acc'].rstrip('%')), 'kind': 'support', 'fold': [att[i]['name'] for i in rows if i != main_[0]]}
        d = a['desc']
        m = re.search(r'(\d+)% chance of (\d+)% (boost|reduction) to (\w+)', d)
        if m:
            s.update(effect='STAT', chance=int(m.group(1)), param=int(m.group(2)) * (1 if m.group(3) == 'boost' else -1),
                     mask=STAT_BITS[m.group(4)], self_=m.group(3) == 'boost')
        elif re.search(r'(\d+)% chance to apply (\w+)\.', d) and 'Charge' not in d:
            m = re.search(r'(\d+)% chance to apply (\w+)\.', d)
            s.update(effect='STATUS', chance=int(m.group(1)), param=ail(STATUS_MAP[norm(m.group(2))]))
        elif 'Charge Physical' in d:
            s.update(effect='CHARGE_PHYS', self_=True)
        elif 'Charge Magic' in d:
            s.update(effect='CHARGE_MAGIC', self_=True)
        elif 'recovers HP' in d:
            s.update(effect='HEAL', self_=True)
        elif 'Removes stat buffs' in d:
            s.update(effect='REMOVE_BUFFS')
        elif 'Removes stat debuffs' in d:
            s.update(effect='REMOVE_DEBUFFS', self_=True)
        elif 'Removes' in d:
            s.update(effect='CURE', self_=True)
        else:
            raise DataError(f'support skill {name}: no effect recognised in "{d}"')
        idx = len(skills)
        for i in rows:
            fate[i] = (idx, 0)
        skills.append(s)
    for i, a in enumerate(att):
        if i in fate:
            continue
        reason = next((r for pat, r in DROP_SUPPORT if re.search(pat, a['desc'])), None)
        need(reason, f'attachment skill {a["name"]} is neither learnable nor dropped for a named reason')
        fate[i] = reason
    return skills, fate


# ---------------------------------------------------------------- signature skills

# Clauses are matched with all whitespace removed and in lower case, so the
# source's run-together words ("atpower", "Increaseddamage") need no fixing.
ELS = '|'.join(e.lower() for e in ELEMENTS)
DROP_CLAUSES = [
    r'\d+%crtrate', r'increaseddamagetotargetswith\w+trait', r'(nullifies|negates)unfavorablecompatibility',
    r'if.*', r'the(lower|greater|more).*', r'consumesallsp', r'appliestaunting(touser)?',
    r'appliesstatusabnormalityimmunity', r'removes(stat|allstat)(buffs|debuffs|changes)',
    r'removesstatusabnormalities.*', r'stealsallstatchanges', r'invertsallstatchangesofthetarget',
    r'\d+%chancetonullifyphysical/magicdamage', r'(\d+%chanceof)?(\d+%)?reductionto\w+resistance',
    r'\d+%chancetoapply\w+resistancedown', r'\d+%chanceof\w+resistanceboostfor\d+tu?r?ns?',
    r'\d+%chanceof\d+%(reduction|boost)toevafor\d+turns?(\(s\))?',
    r'\d+%chanceof\d+%reductiontouser\'smaxhpfor\d+turns?', r'recoversspequalto\d+%ofdamagedealt',
    r'activatescounterphysical',
]


def parse_signature(sk, users):
    """One special skill -> dict, or None for the five dropped. Raises on any clause it cannot place."""
    name, desc = sk['name'], sk['desc']
    if name in SIG_DROPPED:
        return None
    m = re.match(r'\s*\[Target: (1 enemy|All enemies|User|All allies|1 ally)\]\s*', desc)
    need(m, f'{name}: no target')
    target, body = m.group(1), desc[m.end():]
    s = {'name': name, 'element': None, 'cat': 'Support', 'power': 0, 'hits': 0,
         'acc': int(sk['acc'].rstrip('%')), 'aoe': target in ('All enemies',), 'notes': [], 'effects': []}
    if name == 'Trinity Arm':
        # A random pick of three attacks; the first one is kept.
        s.update(element='Steel', cat='Physical', power=110, hits=1)
        s['notes'].append('random attack: first option kept')
        return s
    flat = re.sub(r'\s+', '', body).lower()
    flat = flat.replace('counterphysicalupon', 'counterphysical.upon')
    self_target = target in ('User', 'All allies', '1 ally')
    for c in [c for c in flat.split('.') if c]:
        m = re.fullmatch(r'(?:either)?performs(' + ELS + r')(physical|magic)attackatpower(\d+)(?:or(\d+))?'
                         r'(?:x(\d+)(?:～(\d+))?hits)?(,or.*)?', c)
        if m:
            el = m.group(1).capitalize()
            need(el in ELEMENTS, f'{name}: element {el}')
            p = int(m.group(3))
            if m.group(4):
                p = (p + int(m.group(4))) // 2
                s['notes'].append('"power A or B": the mean')
            hits = int(m.group(5) or 1)
            if m.group(6):
                hits = (hits + int(m.group(6))) // 2
                s['notes'].append('variable hits: the mean, rounded down')
            if m.group(7):
                s['notes'].append('"either attack or debuff": the attack kept')
            s.update(element=el, cat=m.group(2).capitalize(), power=p, hits=hits)
            continue
        m = re.fullmatch(r'uponreceivinga(?:physical)attack,counters(againstallenemies)?withan?(' + ELS + r')(physical|magic)'
                         r'attackatpower(\d+)', c)
        if m:
            el = m.group(2).capitalize()
            need(el in ELEMENTS, f'{name}: counter element {el}')
            s.update(element=el, cat=m.group(3).capitalize(), power=int(m.group(4)), hits=1,
                     aoe=bool(m.group(1)))
            self_target = False
            s['notes'].append('counter: a plain attack')
            continue
        m = re.fullmatch(r'deals(fixeddamageatpower|)(\d+)%?(hpdamage)?(?:x(\d+)hits)?', c)
        if m:
            # Percent-of-HP (and Peace Fantasia's "fixed damage", shown as 10%)
            # becomes ordinary power; physical or magic by the first user's
            # stronger attack stat.
            u = users[0]
            s.update(element='Null', cat='Physical' if u['max']['ATK'] >= u['max']['INT'] else 'Magic',
                     power=int(m.group(2)) * PCT_HP_POWER, hits=int(m.group(4) or 1))
            s['notes'].append(f'percent-HP damage: power {PCT_HP_POWER} x percent')
            continue
        m = re.fullmatch(r'(\d+)%chancetoapply([\w/]+?)(touser)?', c)
        if m:
            kept = [STATUS_MAP[x] for x in m.group(2).split('/') if x in STATUS_MAP]
            need(kept or all(x in STATUS_DROPPED or x.endswith('resistancedown') for x in m.group(2).split('/')),
                 f'{name}: status {m.group(2)}')
            if kept:
                s['effects'].append(('STATUS', {'chance': int(m.group(1)), 'param': ail(kept[0]),
                                                'self_': bool(m.group(3))}))
            continue
        m = re.fullmatch(r'(\d+)%chance(?:of|toapply)(\d+)%(reduction|boost)to(user\'s|alluser\'s)?([a-z/]+?)(stats)?'
                         r'(&applyingparalysis|&\d+%reductiontoacu)?(?:for\d+tu?rns?(\(s\))?)?', c)
        if m and m.group(5) not in ('eva', 'maxhp', 'physical/magicdamagetaken'):
            st = ['ATK', 'DEF', 'INT', 'SPI', 'SPD'] if m.group(4) == "alluser's" else m.group(5).upper().split('/')
            if st == ['ACU']:
                # TamaDigi has no accuracy stat: a foe that hits N% less often
                # deals about N% less, so it becomes ATK and magic attack down.
                st = ['ATK', 'INT']
                s['notes'].append('accuracy down: ATK and magic attack down')
            need(all(x in STAT_BITS for x in st), f'{name}: stats {st}')
            boost = m.group(3) == 'boost'
            s['effects'].append(('STAT', {'chance': int(m.group(1)), 'param': int(m.group(2)) * (1 if boost else -1),
                                          'mask': sum(STAT_BITS[x] for x in st),
                                          'self_': boost or bool(m.group(4)) or self_target}))
            if m.group(7) == '&applyingparalysis':
                s['effects'].append(('STATUS', {'chance': int(m.group(1)), 'param': ail('Paralysis')}))
            continue
        if m and m.group(5) == 'physical/magicdamagetaken':   # Bee Cyclone, Elec Guard: the same cut to DEF and SPI
            s['effects'].append(('STAT', {'chance': int(m.group(1)), 'param': int(m.group(2)),
                                          'mask': STAT_BITS['DEF'] | STAT_BITS['SPI'], 'self_': True}))
            s['notes'].append('damage taken down: DEF and magic defence up')
            continue
        m = re.fullmatch(r'\d+%chanceofreductiontoelectricityresistance/(\d+)%reductiontodeffor\d+turns?(\(s\))?', c)
        if m:
            s['effects'].append(('STAT', {'chance': 100, 'param': -int(m.group(1)), 'mask': STAT_BITS['DEF']}))
            continue
        m = re.fullmatch(r'recovershp(?:/sp)?equalto(\d+)%ofdamage(dealt)?', c)
        if m:
            s['effects'].append(('DRAIN', {'param': int(m.group(1))}))
            continue
        m = re.fullmatch(r'receives(\d+)%hpdamagefromattackrecoil|causes(ko)fromattackrecoil', c)
        if m:
            s['effects'].append(('RECOIL', {'param': 100 if m.group(2) else int(m.group(1))}))
            continue
        if c == 'recovershpbasedonuser\'sspi':
            s['power'] = int(re.sub(r'\D', '', sk['power']))
            s['effects'].append(('HEAL', {'param': 0, 'self_': True}))
            continue
        m = re.fullmatch(r'recoversuser\'shpby(\d+)%', c)
        if m:
            s['effects'].append(('HEAL', {'param': int(m.group(1)), 'self_': True}))
            continue
        m = re.fullmatch(r'(\d+)%acu', c)
        if m:
            need(int(m.group(1)) == s['acc'], f'{name}: accuracy column and text disagree')
            continue
        if c == 'alwayshits':
            s['effects'].append(('NEVER_MISS', {}))
            continue
        if c == 'actsfirstnextround':
            s['effects'].append(('FIRST', {}))
            continue
        if c == 'immobilizednextturn':
            s['effects'].append(('RECHARGE', {}))
            continue
        if any(re.fullmatch(p, c) for p in DROP_CLAUSES):
            continue
        raise DataError(f'signature {name}: clause not understood: "{c}"')
    if s['cat'] == 'Support':
        need(s['power'] == 0 or any(k == 'HEAL' for k, _ in s['effects']), f'{name}: support skill with power')
    else:
        need(s['power'] > 0, f'{name}: attack without power')
    return s


def pick_effect(s):
    """Keep at most one effect (EFFECT_ORDER), and fill the stored fields."""
    effs = sorted(s.pop('effects'), key=lambda e: EFFECT_ORDER.index(e[0]))
    s['dropped_effects'] = [k for k, _ in effs[1:]]
    if effs:
        k, a = effs[0]
        s.update(effect=k, chance=a.get('chance', 100), param=a.get('param', 0), mask=a.get('mask', 0),
                 self_=a.get('self_', k in ('RECOIL', 'RECHARGE', 'DRAIN', 'HEAL', 'FIRST')))
    else:
        s['effect'] = 'NONE'
    if s['cat'] == 'Support' and s['effect'] == 'NONE':
        s['notes'].append('no effect left')
    return s


def build_skills(by_name, v1):
    sj = jload('skills.json')
    att, spec = sj['skills']['attachment'], sj['skills']['special']
    need(len(att) == 181 and len(spec) == 613, 'skills.json: expected 181 attachment and 613 special rows')
    need([d['name'] for d in sj['digimon']] == list(by_name), 'skills.json: Digimon not in stats.tsv order')
    learn, fate = build_learnables(att)
    users = defaultdict(list)
    for d in sj['digimon']:
        for i in d['special']:
            users[i].append(by_name[d['name']])
    for n in SIG_DROPPED + SIG_COUNTERS:
        need(sum(1 for x in spec if x['name'] == n) == 1, f'signature {n}: not exactly one row')
    sigs, sig_id = [], {}
    for i, sk in enumerate(spec):
        need(users[i], f'signature {sk["name"]} (row {i}) has no Digimon')
        s = parse_signature(sk, users[i])
        if s is None:
            continue
        s = pick_effect(s)
        s['row'] = i
        sig_id[i] = len(learn) + 1 + len(sigs)
        sigs.append(s)
    for k, s in enumerate(learn):
        s['id'] = k + 1
    for s in sigs:
        s['id'] = sig_id[s['row']]
    # Learnsets: specials at once, then attachment skills ceil(level / 5) hours in.
    learnsets = {}
    for d in sj['digimon']:
        f = by_name[d['name']]
        if f['name'] in ON_ICE:
            continue
        out, seen = [], set()
        for i in d['special']:
            if i in sig_id:
                out.append((sig_id[i], 0))
        for row, lv in sorted(d['learn'], key=lambda p: p[1]):
            fa = fate[row]
            if isinstance(fa, str):
                continue
            sid = fa[0] + 1
            if sid in seen:
                continue
            seen.add(sid)
            out.append((sid, math.ceil(lv / 5)))
        learnsets[f['name']] = out
    return learn, sigs, learnsets, fate, att


# ---------------------------------------------------------------- validation

def effective(s):
    return s['power'] > 0 or s['effect'] != 'NONE'


def validate(D):
    v1, routes, learn, sigs, learnsets = D['v1'], D['routes'], D['learn'], D['sigs'], D['learnsets']
    need(len(v1) == 469, f'{len(v1)} forms, expected 469')
    raisable = {f['name'] for f in v1 if f['raisable']}
    wild = {f['name'] for f in v1 if f['wild_only']}
    fusion = {f['name'] for f in v1 if f['fusion_only']}
    need(len(raisable) + len(wild) + len(fusion) == 469 and not (raisable & wild) and not (wild & fusion)
         and not (raisable & fusion), 'raisable / wild-only / fusion-only do not partition the roster')
    need((len(raisable), len(wild), len(fusion)) == (419, 33, 17),
         f'raisable {len(raisable)}, wild-only {len(wild)}, fusion-only {len(fusion)}; expected 419, 33, 17')
    tiers = Counter(f['tier'] for f in v1 if f['stage'] in ('Armor', 'Hybrid'))
    need(tiers == Counter({'Champion': 14, 'Ultimate': 2, 'Mega': 5}), f'Armor/Hybrid tiers {dict(tiers)}')
    for r in routes:
        need(r['from']['raisable'] and r['to']['raisable'], 'a kept route touches a non-raisable form')
    # Reachability: from every In-Training I, by digivolving or de-digivolving.
    adj = defaultdict(set)
    for r in routes:
        adj[r['from']['name']].add(r['to']['name'])
        adj[r['to']['name']].add(r['from']['name'])
    eggs = [f['name'] for f in v1 if f['stage'] == 'In-Training I' and f['raisable']]
    need(len(eggs) == 7, f'{len(eggs)} In-Training I forms, expected 7')
    seen, q = set(eggs), deque(eggs)
    while q:
        for n in adj[q.popleft()]:
            if n not in seen:
                seen.add(n)
                q.append(n)
    need(seen == raisable, f'reachable set differs from the raisable set: unreachable {sorted(raisable - seen)[:8]}, '
                           f'reachable but not raisable {sorted(seen - raisable)[:8]}')
    # Fallback (Q9/Q1b): every In-Training and Rookie form keeps a route with no rank gate.
    out = defaultdict(list)
    for r in routes:
        out[r['from']['name']].append(r)
    for f in v1:
        if f['raisable'] and f['stage'] in STAGES[:3]:
            need(any(r['rank'] == 0 for r in out[f['name']]), f'{f["name"]}: no route without an Agent Rank gate')
    for r in routes:
        if STAGES.index(r['to']['stage']) < STAGES.index(RANK_FROM_STAGE):
            need(r['rank'] == 0, f'{r["from"]["name"]} -> {r["to"]["name"]}: rank before Ultimate')
        need(0 <= r['min_level'] <= 99, 'level minimum out of range')
    # Skills.
    attacks = [s for s in learn if s['kind'] == 'attack']
    support = [s for s in learn if s['kind'] == 'support']
    need(len(learn) == 44 and len(attacks) == 22 and len(support) == 22,
         f'{len(learn)} learnable skills ({len(attacks)} attack, {len(support)} support), expected 44 (22 + 22)')
    need(len({(s['element'], s['cat']) for s in attacks}) == 22, 'attack skills: not one per element and physical/magic')
    need(len(sigs) == 613 - len(SIG_DROPPED), f'{len(sigs)} signatures, expected {613 - len(SIG_DROPPED)}')
    for s in sigs:
        need(s['name'] not in SIG_DROPPED, f'dropped signature {s["name"]} kept')
    for f in v1:
        mine = [s for s in learnsets[f['name']] if s[1] == 0]
        need(mine or not f['raisable'], f'{f["name"]}: no signature skill')
    by_id = {s['id']: s for s in learn + sigs}
    need(sorted(by_id) == list(range(1, len(learn) + len(sigs) + 1)), 'skill IDs are not 1..N')
    need(max(by_id) < 0xFFFF, 'skill IDs overflow 16 bits')
    no_sig = []
    for f in v1:
        ls = learnsets[f['name']]
        ids = [i for i, _ in ls]
        need(len(ids) == len(set(ids)), f'{f["name"]}: a skill twice in its learnset')
        need(all(i in by_id for i in ids), f'{f["name"]}: unknown skill ID')
        if not any(effective(by_id[i]) for i, h in ls if h == 0 and i > len(learn)):
            no_sig.append(f)
    need(not [f for f in no_sig if f['raisable']],
         f'raisable forms without a signature that does anything: {[f["name"] for f in no_sig if f["raisable"]]}')
    D['no_sig'] = no_sig
    for s in learn + sigs:
        if s['effect'] == 'STATUS':
            need(1 <= s['param'] <= len(STATUSES), f'{s["name"]}: status {s["param"]}')
        if s['effect'] == 'STAT':
            need(s['mask'] and s['param'], f'{s["name"]}: stat effect without stats or amount')
        need(s['cat'] != 'Support' or max(s['power'] if isinstance(s['power'], list) else [s['power']]) == 0
             or s['effect'] == 'HEAL', f'{s["name"]}: a support skill with power that does not heal')
        need(len(ascii_upper(s['name'])) == len(s['name']), f'skill name loses a character in ASCII: {s["name"]}')
        need(0 <= min(s['power'] if isinstance(s['power'], list) else [s['power']]) and
             max(s['power'] if isinstance(s['power'], list) else [s['power']]) <= 255, f'{s["name"]}: power over 255')
    # Names.
    names = [f['display'] for f in v1]
    need(len(set(names)) == len(names), f'display names clash: {[n for n, c in Counter(names).items() if c > 1]}')
    for n in names:
        need(len(n) <= NAME_MAX and re.fullmatch(r"[A-Z0-9 +\-'.]+", n), f'display name {n!r}')
    long_ = [f['name'] for f in v1 if len(re.sub(r' +', ' ', re.sub(r'[():]', '', ascii_upper(f['name'])))) > NAME_MAX]
    need(sorted(long_) == sorted(SHORT_NAMES), 'the short-name table does not match the names over 15 characters')
    # Stats fit 16 bits.
    for f in v1:
        for k in STATS:
            need(0 < f['lv1'][k] <= f['max'][k] < 65536, f'{f["name"]} {k}: {f["lv1"][k]} .. {f["max"][k]}')
    # Bosses.
    need(len(D['bosses']) == 13, 'expected 13 bosses')
    for (n, lv, stg, kind), b in zip(BOSSES, D['bosses']):
        need(b['form']['name'] == n and b['form']['stage'] == stg,
             f'boss {n} ({stg}): table has {b["form"]["name"]} ({b["form"]["stage"]})')
    levels = [b['level'] for b in D['bosses']]
    need(levels == sorted(levels), 'boss levels are not in ladder order')
    # Jogress.
    for j in D['jogress']:
        need(j['to']['fusion_only'] or j['to']['raisable'] or j['to']['wild_only'], 'Jogress result')


# ---------------------------------------------------------------- emit

def cstr(s):
    s = ascii_upper(s)
    need(re.fullmatch(r"[ -~]*", s) and '"' not in s and '\\' not in s, f'string {s!r}')
    return '"%s"' % s


def emit_dex(D):
    v1, routes = D['v1'], D['routes']
    out = []
    w = out.append
    w('#pragma once\n#include <stdint.h>\n\n')
    w('// GENERATED by tools/gen_ts_data.py from tools/data/time-stranger/ - do not edit.\n')
    w('// Field meanings: docs/ts-data-format.md.\n\n')
    w('#define TS_DEX_COUNT %d\n\n' % len(v1))
    w('// Stages: the seven raisable ones in order, then Armor and Hybrid (wild-only).\n')
    w('enum : uint8_t { %s, TS_STAGE_COUNT };\n' % ', '.join('TS_ST_' + s for s in STAGE_IDS))
    w('#define TS_RAISABLE_STAGES %d\n' % len(RAISABLE_STAGES))
    w('// Level (hours since hatching + 1) at which each raisable stage may first be reached.\n')
    w('static const uint8_t TS_STAGE_ENTRY[TS_RAISABLE_STAGES] = { %s };\n\n' % ', '.join(map(str, STAGE_ENTRY)))
    w('// Attributes: Vaccine > Virus > Data > Vaccine; Free, Variable (the Hybrids), Unknown\n// and No Data are neutral.\n')
    w('enum : uint8_t { %s, TS_ATTR_COUNT };\n' % ', '.join('TS_AT_' + norm(a).upper() for a in ATTRS))
    w('static const char *const TS_ATTR_NAME[TS_ATTR_COUNT] = { %s };\n\n' % ', '.join(cstr(a) for a in ATTRS))
    w('// Elements, in the resistance-row order.\n')
    w('enum : uint8_t { %s, TS_ELEMENT_COUNT, TS_EL_NONE = 255 };\n'
      % ', '.join('TS_EL_' + e.upper() for e in ELEMENTS))
    w('static const char *const TS_ELEMENT_NAME[TS_ELEMENT_COUNT] = { %s };\n\n'
      % ', '.join(cstr(e) for e in ELEMENTS))
    w('// Stat slots of TsDexEntry::base / max and of the evolution minimums.\n')
    w('enum : uint8_t { %s, TS_STAT_COUNT };\n\n' % ', '.join('TS_STAT_' + s for s in STAT_IDS))
    w('enum : uint8_t {\n  TSF_RAISABLE = 1,     // can be a partner (hatched or digivolved into)\n'
      '  TSF_WILD_ONLY = 2,    // Armor, Hybrid and the 12 Digimon that need a missing mechanic\n'
      '  TSF_FUSION_ONLY = 4,  // reached only by Jogress (wild-only until Jogress ships)\n};\n\n')
    w('struct TsDexEntry {\n'
      '  const char *name;       // display name, upper case, at most 15 characters\n'
      '  uint16_t tsDex;         // Time Stranger Field Guide number (1-475)\n'
      '  uint16_t base[TS_STAT_COUNT];  // raw level-1 stats (TS_STAT_*)\n'
      '  uint16_t max[TS_STAT_COUNT];   // raw level-99 stats\n'
      '  uint8_t stage;          // TS_ST_*\n'
      '  uint8_t tier;           // stage used for wild encounters (Armor/Hybrid: their stat tier)\n'
      '  uint8_t attr;           // TS_AT_*\n'
      '  uint8_t flags;          // TSF_*\n'
      '  uint8_t resist[TS_ELEMENT_COUNT];  // damage taken per element, in tenths (0, 5, 10, 15, 20)\n'
      '  uint16_t evoFirst;      // first route in TS_EVO_TBL; evoCount routes follow\n'
      '  uint8_t evoCount;\n'
      '  uint16_t learnFirst;    // first entry in TS_LEARN_TBL (ts_skills.h); learnCount follow\n'
      '  uint8_t learnCount;\n'
      '};\n\n')
    out_routes = defaultdict(list)
    for r in routes:
        out_routes[r['from']['id']].append(r)
    flat, first = [], {}
    for f in v1:
        first[f['id']] = len(flat)
        flat.extend(out_routes[f['id']])
    lfirst, n = {}, 0
    for f in v1:
        lfirst[f['id']] = n
        n += len(D['learnsets'][f['name']])
    w('static const TsDexEntry TS_DEX_TBL[TS_DEX_COUNT + 1] = {\n')
    w('  { "?", 0, { 0 }, { 0 }, 0, 0, 0, 0, { 0 }, 0, 0, 0, 0 },  // 0: unused\n')
    for f in v1:
        fl = '|'.join(n for n, on in (('TSF_RAISABLE', f['raisable']), ('TSF_WILD_ONLY', f['wild_only']),
                                       ('TSF_FUSION_ONLY', f['fusion_only'])) if on)
        w('  { %s, %d, { %s }, { %s }, TS_ST_%s, TS_ST_%s, TS_AT_%s, %s, { %s }, %d, %d, %d, %d },  // %d %s\n' % (
            cstr(f['display']), f['ts_dex'], ', '.join(str(f['lv1'][k]) for k in STATS),
            ', '.join(str(f['max'][k]) for k in STATS), STAGE_IDS[STAGES.index(f['stage'])],
            STAGE_IDS[STAGES.index(f['tier'])], norm(f['attr']).upper(), fl, ', '.join(map(str, f['resist'])),
            first[f['id']] if out_routes[f['id']] else 0, len(out_routes[f['id']]),
            lfirst[f['id']], len(D['learnsets'][f['name']]), f['id'], f['name']))
    w('};\n\n')
    w('// Evolution routes, grouped by source in dex order (TsDexEntry::evoFirst), in the\n'
      '// game\'s order within a source. Only routes between raisable forms are kept.\n'
      '// De-digivolution is any route read backwards.\n')
    w('struct TsEvo {\n'
      '  uint16_t from, to;      // dex ids\n'
      '  uint16_t min[TS_STAT_COUNT];  // raw stat minimums (TS_STAT_*; 0 = none)\n'
      '  uint8_t minLevel;       // from the SP minimum (SP is dropped); 0 = none\n'
      '  uint8_t rank;           // Agent Rank needed; 0 = none\n'
      '};\n')
    w('#define TS_EVO_COUNT %d\n' % len(flat))
    w('static const TsEvo TS_EVO_TBL[TS_EVO_COUNT] = {\n')
    for r in flat:
        w('  { %d, %d, { %s }, %d, %d },  // %s -> %s\n' % (
            r['from']['id'], r['to']['id'], ', '.join(str(r['min'][k]) for k in STATS), r['min_level'], r['rank'],
            r['from']['name'], r['to']['name']))
    w('};\n\n')
    w('// Jogress pairs (data only; a v1 stretch goal). Partner personalities are not\n'
      '// required. partnersRaisable = 0 when a partner can never be a partner in v1.\n')
    w('struct TsJogress {\n'
      '  uint16_t a, b, to;      // dex ids\n'
      '  uint16_t min[TS_STAT_COUNT];  // raw stat minimums (0 = none)\n'
      '  uint8_t rank;           // Agent Rank needed\n'
      '  uint8_t partnersRaisable;\n'
      '};\n')
    w('#define TS_JOGRESS_COUNT %d\n' % len(D['jogress']))
    w('static const TsJogress TS_JOGRESS_TBL[TS_JOGRESS_COUNT] = {\n')
    for j in D['jogress']:
        w('  { %d, %d, %d, { %s }, %d, %d },  // %s + %s -> %s\n' % (
            j['a']['id'], j['b']['id'], j['to']['id'], ', '.join(str(j['min'][k]) for k in STATS), j['rank'],
            int(j['partners_raisable']), j['a']['name'], j['b']['name'], j['to']['name']))
    w('};\n\n')
    w('// The boss ladder, unlocked in order: 8 area bosses (a badge each), 4 elites, the champion.\n')
    w('enum : uint8_t { TS_BOSS_AREA, TS_BOSS_ELITE, TS_BOSS_CHAMPION };\n')
    w('struct TsBoss {\n  uint16_t dex;          // dex id\n  uint8_t level;\n'
      '  uint8_t stage;          // TS_ST_*\n  uint8_t kind;           // TS_BOSS_*\n};\n')
    w('#define TS_BOSS_COUNT %d\n' % len(D['bosses']))
    w('static const TsBoss TS_BOSS_TBL[TS_BOSS_COUNT] = {\n')
    for b in D['bosses']:
        w('  { %d, %d, TS_ST_%s, TS_BOSS_%s },  // %s\n' % (
            b['form']['id'], b['level'], STAGE_IDS[STAGES.index(b['form']['stage'])], b['kind'], b['form']['name']))
    w('};\n')
    return ''.join(out)


def emit_skills(D):
    learn, sigs = D['learn'], D['sigs']
    out = []
    w = out.append
    w('#pragma once\n#include <stdint.h>\n#include "ts_dex.h"\n\n')
    w('// GENERATED by tools/gen_ts_data.py from tools/data/time-stranger/ - do not edit.\n')
    w('// Field meanings: docs/ts-data-format.md.\n\n')
    w('// Skill IDs are positional and saved: 0 = none, 1-%d the learnable skills, then the\n'
      '// signature skills in skills.json row order. Append only: a skill added later\n'
      '// goes at the end, never into its row-order place.\n' % len(learn))
    w('#define TS_SKILL_COUNT %d   // including 0\n' % (len(learn) + len(sigs) + 1))
    w('#define TS_LEARNABLE_COUNT %d\n' % len(learn))
    w('#define TS_SKILL_FIRST_SIGNATURE %d\n\n' % (len(learn) + 1))
    w('enum : uint8_t { TS_SC_PHYS, TS_SC_MAGIC, TS_SC_SUPPORT };\n')
    w('enum : uint8_t {\n')
    docs = {'NONE': '', 'STATUS': 'param = TS_AIL_*', 'STAT': 'param = signed percent, statMask = TS_SM_*',
            'DRAIN': 'param = percent of damage dealt healed', 'RECOIL': 'param = percent of the user\'s max HP lost',
            'HEAL': 'param = percent of max HP, or 0 = by power and magic defence',
            'FIRST': 'moves first', 'NEVER_MISS': 'skips the accuracy roll', 'RECHARGE': 'the user loses its next turn',
            'CHARGE_PHYS': 'next physical attack boosted', 'CHARGE_MAGIC': 'next magic attack boosted',
            'CURE': 'clears the six statuses', 'REMOVE_BUFFS': 'clears the target\'s stat boosts',
            'REMOVE_DEBUFFS': 'clears the user\'s stat drops'}
    for e in EFFECTS:
        w('  TS_EF_%s,%s\n' % (e, ('  // ' + docs[e]) if docs[e] else ''))
    w('};\n')
    w('enum : uint8_t { %s };\n' % ', '.join('TS_SM_%s = %d' % (k if k in ('ATK', 'DEF', 'SPD') else
                                                                  {'INT': 'MATK', 'SPI': 'MDEF'}[k], v)
                                             for k, v in STAT_BITS.items()))
    w('enum : uint8_t { TS_AIL_NONE = 0, %s };\n' % ', '.join('TS_AIL_%s = %d' % (AIL_IDS[s], ail(s)) for s in STATUSES))
    w('enum : uint8_t {\n  TS_SKF_SELF = 1,       // the effect lands on the user\n'
      '  TS_SKF_WAS_AOE = 2,    // all-enemies in Time Stranger (balance pass: power boost)\n};\n\n')
    w('struct TsSkill {\n'
      '  const char *name;\n'
      '  uint8_t element;        // TS_EL_*; TS_EL_NONE for support skills\n'
      '  uint8_t cat;            // TS_SC_*\n'
      '  uint8_t power[3];       // tier I / II / III by the partner\'s level (signatures: one value)\n'
      '  uint8_t hits;           // fixed hit count (0 for support skills)\n'
      '  uint8_t acc;            // percent\n'
      '  uint8_t effect;         // TS_EF_*\n'
      '  uint8_t chance;         // percent, for TS_EF_STATUS and TS_EF_STAT\n'
      '  int8_t param;           // see TS_EF_*\n'
      '  uint8_t statMask;       // TS_SM_*, for TS_EF_STAT\n'
      '  uint8_t flags;          // TS_SKF_*\n'
      '};\n\n')
    w('static const TsSkill TS_SKILL_TBL[TS_SKILL_COUNT] = {\n')
    w('  { "-", TS_EL_NONE, TS_SC_SUPPORT, { 0, 0, 0 }, 0, 0, TS_EF_NONE, 0, 0, 0, 0 },  // 0\n')
    cat = {'Physical': 'TS_SC_PHYS', 'Magic': 'TS_SC_MAGIC', 'Support': 'TS_SC_SUPPORT'}
    for s in learn + sigs:
        p = s['power'] if isinstance(s['power'], list) else [s['power']] * 3
        fl = '|'.join(n for n, on in (('TS_SKF_SELF', s.get('self_') and s['effect'] != 'NONE'),
                                       ('TS_SKF_WAS_AOE', s.get('aoe'))) if on) or '0'
        mask = '|'.join('TS_SM_' + {'INT': 'MATK', 'SPI': 'MDEF'}.get(k, k) for k, v in STAT_BITS.items()
                        if s.get('mask', 0) & v) or '0'
        param = s.get('param', 0)
        need(-128 <= param <= 127, f'{s["name"]}: param {param}')
        if s['effect'] == 'STATUS':
            param = 'TS_AIL_' + AIL_IDS[STATUSES[param - 1]]
        tag = ('fold ' + (s['fold'] if isinstance(s['fold'], str) else ', '.join(s['fold']))) if s.get('fold') else ''
        if 'row' in s:
            tag = 'row %d' % s['row'] + ('; ' + '; '.join(s['notes']) if s['notes'] else '')
        w('  { %s, %s, %s, { %s }, %d, %d, TS_EF_%s, %d, %s, %s, %s },  // %d%s\n' % (
            cstr(s['name']), 'TS_EL_' + s['element'].upper() if s['element'] else 'TS_EL_NONE', cat[s['cat']],
            ', '.join(map(str, p)), s['hits'], s['acc'], s['effect'], s.get('chance', 0) if s['effect'] in
            ('STATUS', 'STAT') else 0, param, mask, fl, s['id'], (' ' + tag) if tag else ''))
    w('};\n\n')
    w('// Learnsets, per form (TsDexEntry::learnFirst / learnCount): signature skills at\n'
      '// hour 0, then attachment skills ceil(Time Stranger learn level / 5) hours after\n'
      '// becoming the form, one entry per learnable skill.\n')
    w('struct TsLearn {\n  uint16_t skill;         // TS_SKILL_TBL index\n'
      '  uint8_t hours;          // hours after becoming the form\n};\n')
    total = sum(len(D['learnsets'][f['name']]) for f in D['v1'])
    w('#define TS_LEARN_COUNT %d\n' % total)
    w('static const TsLearn TS_LEARN_TBL[TS_LEARN_COUNT] = {\n')
    for f in D['v1']:
        ls = D['learnsets'][f['name']]
        w('  // %d %s\n' % (f['id'], f['name']))
        if ls:
            w('  %s\n' % ' '.join('{ %d, %d },' % p for p in ls))
    w('};\n')
    return ''.join(out)


# ---------------------------------------------------------------- main

def build():
    v1, by_v1, by_name = load_roster()
    routes, jogress, dropped, sp_over = build_evolutions(v1, by_v1, by_name)
    learn, sigs, learnsets, fate, att = build_skills(by_name, v1)
    bosses = []
    for n, lv, stg, kind in BOSSES:
        need(n in by_v1, f'boss {n} is not in the v1 roster')
        bosses.append({'form': by_v1[n], 'level': lv, 'kind': kind})
    return {'v1': v1, 'routes': routes, 'jogress': jogress, 'dropped': dropped, 'sp_over': sp_over,
            'learn': learn, 'sigs': sigs, 'learnsets': learnsets, 'fate': fate, 'att': att, 'bosses': bosses}


def summary(D):
    v1 = D['v1']
    c = Counter(f['stage'] for f in v1)
    print(f'{len(v1)} forms: ' + ', '.join(f'{s} {c[s]}' for s in STAGES))
    print(f'  raisable {sum(f["raisable"] for f in v1)}, wild-only {sum(f["wild_only"] for f in v1)}, '
          f'fusion-only {sum(f["fusion_only"] for f in v1)}; {len(SHORT_NAMES)} short display names')
    rk = Counter((r['from']['stage'], r['to']['stage'], r['rank']) for r in D['routes'] if r['rank'])
    print(f'{len(D["routes"])} routes kept; dropped: ' + ', '.join(f'{k} {n}' for k, n in D['dropped'].most_common()))
    print('  Agent Rank kept: ' + ', '.join(f'{a}->{b} rank {r}: {n}' for (a, b, r), n in sorted(rk.items(),
          key=lambda kv: (STAGES.index(kv[0][0]), STAGES.index(kv[0][1]), kv[0][2]))))
    hp = [r for r in D['routes'] if r['min']['HP'] > r['from']['max']['HP']]
    print(f'  {len(hp)} HP minimums exceed the source\'s level-99 HP (reached with the de-digivolve HP boost)')
    sp = [r for r in D['routes'] if r['sp']]
    print(f'  {len(sp)} routes had an SP minimum and it became a level minimum, except {len(D["sp_over"])} above the source\'s level-99 SP '
          f'and were dropped: ' + ', '.join(f'{a} -> {b}' for a, b, *_ in D['sp_over']
                                              if a not in ON_ICE and b not in ON_ICE))
    print(f'{len(D["jogress"])} Jogress pairs (data only), {sum(not j["partners_raisable"] for j in D["jogress"])} '
          f'with a partner that cannot be raised')
    att_drop = Counter(v for v in D['fate'].values() if isinstance(v, str))
    print(f'{len(D["learn"])} learnable skills from {len(D["att"])} attachment rows; dropped: '
          + ', '.join(f'{k} {n}' for k, n in att_drop.most_common()))
    sigs = D['sigs']
    ef = Counter(s['effect'] for s in sigs)
    print(f'{len(sigs)} signature skills ({len(SIG_DROPPED)} dropped); effects: '
          + ', '.join(f'{k} {n}' for k, n in ef.most_common()))
    print(f'  a second effect dropped from {sum(1 for s in sigs if s["dropped_effects"])}; '
          f'{sum(1 for s in sigs if s["aoe"])} were all-enemies; '
          f'{sum(1 for s in sigs if not effective(s))} do nothing: '
          + ', '.join(s['name'] for s in sigs if not effective(s)))
    print('  forms with no signature that does anything (none raisable): '
          + (', '.join(f['name'] for f in D['no_sig']) or 'none'))
    longest = max(D['learn'] + sigs, key=lambda s: len(s['name']))
    print(f'  longest skill name: {len(longest["name"])} characters ({ascii_upper(longest["name"])})')
    print(f'{sum(len(v) for v in D["learnsets"].values())} learnset entries; 13 bosses: '
          + ', '.join(f'{b["form"]["name"]} {b["level"]}' for b in D['bosses']))


def main():
    try:
        D = build()
        validate(D)
    except DataError as e:
        sys.exit(f'gen_ts_data: {e}')
    files = {'ts_dex.h': emit_dex(D), 'ts_skills.h': emit_skills(D)}
    if '--check' in sys.argv:
        stale = [n for n, t in files.items() if not os.path.exists(os.path.join(ROOT, n))
                 or open(os.path.join(ROOT, n)).read() != t]
        if stale:
            sys.exit(f'gen_ts_data: stale, regenerate: {stale}')
    else:
        for n, t in files.items():
            with open(os.path.join(ROOT, n), 'w') as f:
                f.write(t)
    summary(D)


if __name__ == '__main__':
    main()
