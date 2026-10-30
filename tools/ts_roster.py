"""The Time Stranger roster as the tools see it: read back out of ts_dex.h and
ts_skills.h, so nothing here is a second copy of the tables.

    from ts_roster import ROSTER, COUNT, art_map, home_element
"""
import csv
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
_DEX = open(os.path.join(ROOT, 'ts_dex.h')).read()
_SKILLS = open(os.path.join(ROOT, 'ts_skills.h')).read()

COUNT = int(re.search(r'#define TS_DEX_COUNT (\d+)', _DEX).group(1))
STAGES = ['IN_TRAINING_1', 'IN_TRAINING_2', 'ROOKIE', 'CHAMPION', 'ULTIMATE', 'MEGA',
          'MEGA_PLUS', 'ARMOR', 'HYBRID']
ELEMENTS = ['FIRE', 'WATER', 'PLANT', 'ICE', 'ELECTRICITY', 'EARTH', 'STEEL', 'WIND',
            'LIGHT', 'DARK', 'NULL']

_ROW = re.compile(r'^\s*\{ "([^"]+)", \d+, \{ [\d, ]+ \}, \{ [\d, ]+ \}, TS_ST_(\w+), TS_ST_(\w+), '
                  r'TS_AT_\w+, TSF_\w+, \{ ([\d, ]+) \}, \d+, \d+, (\d+), (\d+) \},\s*// (\d+) (.*)$', re.M)
ROSTER = {}
for m in _ROW.finditer(_DEX):
    ROSTER[int(m.group(7))] = {
        'dex': int(m.group(7)), 'name': m.group(1), 'form': m.group(8), 'stage': STAGES.index(m.group(2)),
        'tier': STAGES.index(m.group(3)), 'resist': [int(v) for v in m.group(4).split(', ')],
        'learn_first': int(m.group(5)), 'learn_count': int(m.group(6)),
    }
if sorted(ROSTER) != list(range(1, COUNT + 1)):
    raise SystemExit('ts_roster: could not read every row of ts_dex.h')

_SKILL = re.compile(r'^\s*\{ ".*?", TS_EL_(\w+), TS_SC_(\w+), ', re.M)
_skill_rows = _SKILL.findall(_SKILLS)
_learn = re.findall(r'\{ (\d+), (\d+) \}', _SKILLS[_SKILLS.index('TS_LEARN_TBL[TS_LEARN_COUNT]'):])


def home_element(dex):
    """The same answer as homeElement() in types.h: the element of the form's first
    attacking signature skill, else the element it resists best (ties: the first)."""
    row = ROSTER[dex]
    for i in range(row['learn_first'], row['learn_first'] + row['learn_count']):
        skill, hours = int(_learn[i][0]), int(_learn[i][1])
        if hours == 0 and _skill_rows[skill][1] != 'SUPPORT':
            return ELEMENTS.index(_skill_rows[skill][0])
    return row['resist'].index(min(row['resist']))


def art_map():
    """Time Stranger dex id -> its Digimon Color sheet row (tools/data/art/)."""
    path = os.path.join(HERE, 'data', 'art', 'digimon_color.tsv')
    rows = csv.DictReader((line for line in open(path) if not line.startswith('#')), delimiter='\t')
    return {int(r['dex']): r for r in rows}
