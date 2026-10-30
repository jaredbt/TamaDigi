#!/usr/bin/env python3
"""Audit of the Time Stranger roster before it replaces the 87-Digimon one.

Joins stats.tsv, attributes.tsv and evolutions.json, applies the decided
filters (no Armor or Hybrid; no evolution that needs Jogress, an item, Agent
Skills or Talent) and reports what the data pipeline has to deal with:

  - forms per stage, and evolutions kept and dropped;
  - the fallback rule: every In-Training and Rookie form keeps a way out;
  - reachability: every kept form joins up with an In-Training I through
    digivolution and de-digivolution;
  - training: no kept evolution is out of reach under the 255 training cap,
    and HP (not trainable) is reached by growth alone;
  - storage: maximum stats times the scale factor against a uint8_t;
  - the 13 bosses: present and on the expected stage;
  - display names: over 15 characters or clashing once upper-cased;
  - public art: which of the current 87 Digimon Color sprites map onto a
    Time Stranger form, and whether those forms still join up.

    python3 -I tools/data/time-stranger/roster_audit.py
"""
import csv
import json
import os
import re
import sys
from collections import Counter, defaultdict, deque

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
S = ['HP', 'SP', 'ATK', 'DEF', 'INT', 'SPI', 'SPD']
CHECKED = ['HP', 'ATK', 'DEF', 'INT', 'SPI', 'SPD']        # SP dropped (Q12b)
TRAINABLE = ['ATK', 'DEF', 'INT', 'SPI', 'SPD']            # HP grows with age only (Q12c)
SCALE = 0.1                                                # TS stat -> TamaDigi stat (Q12e)
CAP = 255                                                  # training cap per stat (Q12d)
FAREWELL = 73
HP_BOOST, HP_BOOST_CAP = 0.1, 2.0                          # HP x1.1 per de-digivolve, at most x2.0 (Q16)
LEFT_OUT = {'Armor', 'Hybrid'}
ON_ICE = {'Aegiomon', 'Aegiochusmon', 'Aegiochusmon: Green', 'Aegiochusmon: Dark',
          'Aegiochusmon: Blue', 'Aegiochusmon: Holy'}            # out of v1 entirely (2026-10-28)
STAGES = ['In-Training I', 'In-Training II', 'Rookie', 'Champion', 'Ultimate', 'Mega', 'Mega +']
BOSSES = [('DemiDevimon', 'Rookie'), ('Ogremon', 'Champion'), ('Unimon', 'Champion'),
          ('Devimon', 'Champion'), ('Etemon', 'Ultimate'), ('Myotismon', 'Ultimate'),
          ('Mummymon', 'Ultimate'), ('Arukenimon', 'Ultimate'), ('MetalSeadramon', 'Mega'),
          ('MetalEtemon', 'Mega'), ('MaloMyotismon', 'Mega'), ('Diaboromon', 'Mega'),
          ('Apocalymon', 'Mega +')]
# Digimon Color sprite names whose Time Stranger name differs. The mapping these produced
# is kept in tools/data/art/digimon_color.tsv.
ART_ALIASES = {'Metal Greymon (Virus)': 'MetalGreymon (Blue)', 'Chimairamon': 'Kimeramon',
               'Gaioumon': 'Gaiomon', 'Mugendramon': 'Machinedramon', 'Cyclomon': 'Cyclonemon',
               'Millenniumon': 'Millenniummon', 'Piyomon': 'Biyomon', 'Yukidarumon': 'Frigimon',
               'Scumon': 'Sukamon', 'Vegimon': 'Vegiemon', 'Centalmon': 'Centarumon'}

norm = lambda s: re.sub(r'[^a-z0-9]+', '', s.lower())


def display(name):
    return re.sub(r'[^A-Z0-9+ ]', '', name.upper().replace(':', '')).replace(' ', '')


def load():
    forms = {}
    for i, r in enumerate(csv.DictReader(open(os.path.join(HERE, 'stats.tsv')), delimiter='\t'), 1):
        forms[norm(r['name'])] = {'dex': i, 'name': r['name'], 'stage': r['stage'],
                                  'lv1': {k: int(r[k]) for k in S}, 'max': {k: int(r['m' + k]) for k in S}}
    for r in csv.DictReader(open(os.path.join(HERE, 'attributes.tsv')), delimiter='\t'):
        f = forms[norm(r['name'])]
        assert f['dex'] == int(r['dex']), r
        f['attr'] = r['attribute']
    evo = json.load(open(os.path.join(HERE, 'evolutions.json')))
    return forms, evo


def at(f, s, lv):
    lv = min(lv, 99)
    return f['lv1'][s] + (f['max'][s] - f['lv1'][s]) * (lv - 1) / 98


def component(starts, adj):
    seen, q = set(starts), deque(starts)
    while q:
        for n in adj[q.popleft()]:
            if n not in seen:
                seen.add(n)
                q.append(n)
    return seen


def main():
    forms, evo = load()
    kept = {k: f for k, f in forms.items() if f['stage'] not in LEFT_OUT and f['name'] not in ON_ICE}
    print(f'{len(forms)} forms, {len(kept)} kept (Armor, Hybrid and the Aegiomon line left out)')
    c = Counter(f['stage'] for f in kept.values())
    print('  per stage: ' + ', '.join(f'{s} {c[s]}' for s in STAGES))
    print('  attributes: ' + ', '.join(f'{a} {n}' for a, n in Counter(f['attr'] for f in kept.values()).most_common()))

    edges, why = [], Counter()
    for src, d in evo.items():
        fs = kept.get(norm(src))
        for to, cond in d['evolves_into']:
            ft = kept.get(norm(to))
            if fs is None or ft is None:
                why['source or target is Armor/Hybrid'] += 1
            elif 'Jogress' in ''.join(cond) :
                why['Jogress'] += 1
            elif 'Req. Item' in cond:
                why['item'] += 1
            elif any('Agent Skills' in k for k in cond):
                why['Agent Skills'] += 1
            elif 'Talent' in cond:
                why['Talent'] += 1
            else:
                edges.append((fs, ft, cond))
    print(f'\n{len(edges)} evolutions kept; dropped: ' + ', '.join(f'{k} {n}' for k, n in why.most_common()))
    sp = [(a['name'], b['name']) for a, b, cnd in edges if 'SP' in cnd]
    sponly = [(a['name'], b['name']) for a, b, cnd in edges if 'SP' in cnd and not any(k in cnd for k in CHECKED)]
    print(f'  {len(sp)} need SP, which TamaDigi drops; {len(sponly)} need SP and no other stat '
          f'(only Agent Rank would gate them): {sponly[:8]}{" ..." if len(sponly) > 8 else ""}')
    back = [(a['name'], b['name']) for a, b, _ in edges if STAGES.index(b['stage']) <= STAGES.index(a['stage'])]
    print(f'  {len(back)} go to the same or an earlier stage: {back[:6]}{" ..." if len(back) > 6 else ""}')

    out = defaultdict(list)
    for a, b, cnd in edges:
        out[a['name']].append((b, cnd))
    print('\nfallback rule (every In-Training and Rookie form keeps a way out):')
    for stg in STAGES[:3]:
        bad = [f['name'] for f in kept.values() if f['stage'] == stg and not out[f['name']]]
        print(f'  {stg}: {len(bad)} with no kept evolution {bad}')

    adj, fwd = defaultdict(set), defaultdict(set)
    for a, b, _ in edges:
        adj[a['name']].add(b['name'])
        adj[b['name']].add(a['name'])
        fwd[a['name']].add(b['name'])
    eggs = [f['name'] for f in kept.values() if f['stage'] == 'In-Training I']
    both, onward = component(eggs, adj), component(eggs, fwd)
    print(f'\nreachability from the {len(eggs)} In-Training I forms:')
    print(f'  by digivolution alone: {len(onward)} of {len(kept)}')
    orphans = sorted((f['dex'], f['name'], f['stage']) for f in kept.values() if f['name'] not in both)
    print(f'  with de-digivolution too: {len(both)}; never reachable: {len(orphans)}')
    for o in orphans:
        print(f'    {o[0]:3} {o[1]} ({o[2]})')

    print(f'\ntraining (scale {SCALE}, cap {CAP} per stat; HP from growth alone):')
    hp99, hp73, need = [], [], []
    for a, b, cnd in edges:
        if 'HP' in cnd:
            if at(a, 'HP', 99) < cnd['HP']:
                hp99.append((a['name'], b['name']))
            elif at(a, 'HP', FAREWELL) < cnd['HP']:
                hp73.append((a['name'], b['name']))
        t = max([(cnd[k] - at(a, k, 99)) * SCALE for k in TRAINABLE if k in cnd] + [0])
        need.append((t, a['name'], b['name']))
    need.sort(reverse=True)
    print(f'  HP never reached by level 99 without de-digivolving: {len(hp99)} {hp99[:6]}')
    loops = Counter()
    for a, b, cnd in edges:
        if 'HP' in cnd and at(a, 'HP', 99) < cnd['HP']:
            n = next((k for k in range(1, 11) if at(a, 'HP', 99) * (1 + HP_BOOST * k) >= cnd['HP']), None)
            loops[n if n and 1 + HP_BOOST * n <= HP_BOOST_CAP else 'over the cap'] += 1
    print(f'  de-digivolve loops they need (x{1 + HP_BOOST:.1f} each, cap x{HP_BOOST_CAP:.1f}): {dict(loops)}')
    print(f'  HP reached only after level {FAREWELL}: {len(hp73)} {hp73[:6]}')
    print(f'  over the cap: {sum(1 for t, *_ in need if t > CAP)}; hardest: '
          + ', '.join(f'{x} -> {y} {t:.0f}' for t, x, y in need[:3]))

    big = Counter()
    for f in kept.values():
        for k in CHECKED:
            if f['max'][k] * SCALE > 255:
                big[k] += 1
    top = max(kept.values(), key=lambda f: f['max']['HP'])
    print(f'\nstorage: max stat x {SCALE} over 255 (uint8_t) in {sum(big.values())} cases {dict(big)}; '
          f'largest HP {top["name"]} {top["max"]["HP"]} -> {top["max"]["HP"] * SCALE:.0f}')

    print('\nbosses:')
    for n, stg in BOSSES:
        f = kept.get(norm(n))
        state = 'MISSING' if f is None else ('ok' if f['stage'] == stg else f'stage is {f["stage"]}')
        reach = '' if f is None or f['name'] in both else ', never reachable as a partner'
        print(f'  {n:15} {stg:9} {state}{reach}' + (f' (dex {f["dex"]}, {f["attr"]})' if f else ''))

    names = defaultdict(list)
    for f in kept.values():
        names[display(f['name'])].append(f['name'])
    long_ = sorted(d for d in names if len(d) > 15)
    clash = {d: v for d, v in names.items() if len(v) > 1}
    print(f'\ndisplay names (upper case, no spaces or punctuation): {len(long_)} over 15 characters, '
          f'{len(clash)} clashes')
    for d in long_:
        print(f'    {len(d):2} {d}')
    for d, v in clash.items():
        print(f'    clash {d}: {v}')

    art = {}
    path = os.path.join(ROOT, 'tools', 'data', 'art', 'digimon_color.tsv')
    for row in csv.DictReader((l for l in open(path) if not l.startswith('#')), delimiter='\t'):
        f = kept.get(norm(row['form']))
        assert f, row
        art[f['name']] = row['sheet']
    print(f'\npublic art: {len(art)} Digimon Color sheets map onto a kept Time Stranger form')
    aadj = defaultdict(set)
    for a, b, _ in edges:
        if a['name'] in art and b['name'] in art:
            aadj[a['name']].add(b['name'])
            aadj[b['name']].add(a['name'])
    aeggs = [n for n in eggs if n in art]
    reach = component(aeggs, aadj)
    print(f'  In-Training I forms with art: {aeggs}')
    print(f'  forms with art reachable using only forms with art: {len(reach)} of {len(art)}')
    by = Counter(kept[norm(n)]['stage'] for n in reach)
    print('  per stage: ' + ', '.join(f'{s} {by[s]}' for s in STAGES))


if __name__ == '__main__':
    main()
