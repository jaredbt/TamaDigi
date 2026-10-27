#!/usr/bin/env python3
"""Approach B check: Time Stranger's own stat growth (level 1 -> max at 99,
linear between them, from stats.tsv), TamaDigi level = TS level (one per hour,
level 1 at hatch). For each of the game's evolutions (evolutions.json), when
could an UNTRAINED Digimon first take it?

This is the check behind the "training is the bridge" decision in
docs/HANDOFF.md: untrained growth opens nearly every In-Training and Rookie
evolution on time but few Champion -> Ultimate and Ultimate -> Mega ones.

    python3 -I tools/data/time-stranger/unlock_check.py
    python3 -I tools/data/time-stranger/unlock_check.py --json out.json   # every row
"""
import csv
import json
import os
import re
import statistics as st
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
S = ['HP', 'SP', 'ATK', 'DEF', 'INT', 'SPI', 'SPD']
CHECKED = ['HP', 'ATK', 'DEF', 'INT', 'SPI', 'SPD']       # SP dropped (Q12b)
# TamaDigi stage minimums (STAGE_ENTRY levels), by the stage evolved INTO; Mega+ from 72 h (Q11).
STAGE_MIN = {'In-Training II': 2, 'Rookie': 7, 'Champion': 25, 'Ultimate': 49, 'Mega': 61, 'Mega +': 73}
FAREWELL, MAX_LV = 73, 100
norm = lambda s: re.sub(r'[^a-z0-9]+', '', s.lower())

stats = {}
for r in csv.DictReader(open(os.path.join(HERE, 'stats.tsv')), delimiter='\t'):
    stats[norm(r['name'])] = {'stage': r['stage'], 'lv1': {k: int(r[k]) for k in S},
                              'max': {k: int(r['m' + k]) for k in S}}
evo = json.load(open(os.path.join(HERE, 'evolutions.json')))


def at(p, s, lv):
    lv = min(lv, 99)
    return p['lv1'][s] + (p['max'][s] - p['lv1'][s]) * (lv - 1) / 98


rows, excluded = [], 0
for src, d in evo.items():
    p = stats[norm(src)]
    for to, c in d['evolves_into']:
        if 'Jogress' in c or 'Req. Item' in c or any('Agent Skills' in k for k in c) or 'Talent' in c:
            excluded += 1
            continue
        tgt = stats.get(norm(to), {}).get('stage')
        req = {k: c[k] for k in CHECKED if k in c}
        grow = next((L for L in range(1, MAX_LV + 1) if all(at(p, k, L) >= v for k, v in req.items())), None)
        floor = STAGE_MIN.get(tgt, 1)
        rows.append({'from': src, 'to': to, 'fs': d['stage'], 'ts': tgt, 'req': req, 'rank': c.get('Agent Rank'),
                     'grow': grow, 'floor': floor, 'open': None if grow is None else max(grow, floor)})
print(f'{len(rows)} evolutions checked; {excluded} left out (Jogress, items, Agent Skills, Talent)\n')

order = ['In-Training I', 'In-Training II', 'Rookie', 'Champion', 'Ultimate', 'Mega', 'Armor', 'Hybrid']
print(f"{'from stage':15}{'paths':>6}{'open at stage min':>19}{'median opens':>14}{'by 72 h':>10}"
      f"{'by lv100':>10}{'never':>7}")
for stg in order:
    rs = [r for r in rows if r['fs'] == stg]
    if not rs:
        continue
    op = [r['open'] for r in rs if r['open'] is not None]
    at_min = sum(1 for r in rs if r['open'] == r['floor'])
    print(f"{stg:15}{len(rs):6}{at_min:10} ({100 * at_min // len(rs):3}%)"
          f"{('lv %d' % st.median(op)) if op else '-':>14}"
          f"{sum(1 for o in op if o <= FAREWELL):5} ({100 * sum(1 for o in op if o <= FAREWELL) // len(rs):3}%)"
          f"{len(op):5} ({100 * len(op) // len(rs):3}%){len(rs) - len(op):7}")

late = sorted(r['open'] - r['floor'] for r in rows if r['open'] and r['open'] > r['floor'])
print(f"\nopen AFTER their stage minimum: {len(late)} of {len(rows)}; hours late: "
      f"median {st.median(late):.0f}, 75th pct {late[int(len(late) * .75)]}, max {late[-1]}")

print('\nper source form, does at least one way out open by its next stage minimum + 6 h?')
stuck = []
for src, d in evo.items():
    mine = [r for r in rows if r['from'] == src]
    if not mine:
        continue
    best = min((r['open'] for r in mine if r['open']), default=None)
    floor = min(r['floor'] for r in mine)
    if best is None or best > floor + 6:
        stuck.append((src, d['stage'], best, floor))
print(f'  forms whose EARLIEST evolution opens >6 h after the stage minimum (or never): {len(stuck)}')
for s in stuck[:15]:
    print('   ', s)

print('\nBotamon line (untrained):')
for a, b in [('Botamon', 'Koromon'), ('Koromon', 'Agumon'), ('Koromon', 'Guilmon'), ('Koromon', 'Kotemon'),
             ('Agumon', 'Greymon'), ('Agumon', 'Coredramon (Green)'), ('Greymon', 'MetalGreymon'),
             ('Greymon', 'Kimeramon'), ('MetalGreymon', 'WarGreymon'), ('MetalGreymon', 'Gaiomon'),
             ('MetalGreymon', 'BlitzGreymon')]:
    r = next((r for r in rows if r['from'] == a and r['to'] == b), None)
    if r:
        print(f"  {a:12} -> {b:19} {str(r['req']):34} growth lv {str(r['grow']):>4}  min {r['floor']:3}"
              f"  => opens lv {r['open']} (hour {r['open'] - 1 if r['open'] else '-'})")
if '--json' in sys.argv:
    json.dump(rows, open(sys.argv[sys.argv.index('--json') + 1], 'w'), indent=1)
