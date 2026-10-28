# Time Stranger data format

`tools/gen_ts_data.py` turns `tools/data/time-stranger/` into two generated
headers at the repository root. This page is for the firmware cutover: what is
in each table, what every field means, and the rules the generator applied. The
rules themselves are decided in `docs/HANDOFF.md`'s "Game design decisions"
table. This page records only how they were applied, plus the cases that table
leaves open.

```bash
python3 tools/gen_ts_data.py           # validate, write ts_dex.h and ts_skills.h, print a summary
python3 tools/gen_ts_data.py --check   # validate, and fail if the committed headers are stale
```

The generator asserts every rule before it writes anything (`validate()`), and
it stops on any source row or skill clause it cannot place. This pipeline is
separate from `tools/digimon_data.py`, which still feeds today's `dex.h`,
`moves.h` and the rest of the current build. Nothing includes the new headers
yet. All identifiers are prefixed `TS_` / `Ts` / `TSF_` so they can sit beside
the old ones during the cutover.

## Indices

- **Dex id**: 1-469, the v1 roster in Time Stranger Field Guide order with the
  Aegiomon line (6 forms) removed. `TS_DEX_TBL[0]` is an unused sentinel.
  `tsDex` keeps the game's own number (1-475) for display.
- **Skill id** (16-bit): 0 = none; 1-44 are the learnable skills; 45-652 are the
  608 signature skills, in `skills.json` row order. Skill ids are positional and
  saved in partner data, so existing ids never change and new skills are
  appended at the end. Restoring a dropped signature in its row-order place
  would shift every later id, so it too goes at the end.

## `ts_dex.h`

### `TsDexEntry TS_DEX_TBL[TS_DEX_COUNT + 1]`

| Field | Meaning |
|---|---|
| `name` | Display name: upper-case ASCII, at most 15 characters, unique. Rule: upper case, drop `(`, `)` and `:`, keep spaces. 29 names come out longer and use a hand table (`SHORT_NAMES` in the generator), e.g. `METALGREY BLUE`, `ULFORCEVDRMON X`, `CRANIA+ENBARR` |
| `tsDex` | Time Stranger Field Guide number |
| `base[6]`, `max[6]` | Raw level-1 and level-99 stats, indexed by `TS_STAT_HP, _ATK, _DEF, _SPD, _MATK` (INT), `_MDEF` (SPI). SP is dropped. Grow linearly from level 1 to 99, then apply the one scale constant (~0.1) in firmware |
| `stage` | `TS_ST_IN_TRAINING_1` ... `TS_ST_MEGA_PLUS` (0-6), `TS_ST_ARMOR` (7), `TS_ST_HYBRID` (8) |
| `tier` | Stage used for wild encounters: the same as `stage` for the seven raisable stages. Armor/Hybrid use their stat tier: 14 Champion, 2 Ultimate, 5 Mega |
| `attr` | `TS_AT_VACCINE, _DATA, _VIRUS, _FREE, _VARIABLE, _UNKNOWN, _NODATA`. Only Vaccine > Virus > Data > Vaccine matters; the rest are neutral. `VARIABLE` is the Hybrids' attribute, which HANDOFF does not list |
| `flags` | Exactly one of `TSF_RAISABLE` (419), `TSF_WILD_ONLY` (33: 10 Armor, 11 Hybrid, the 12 listed forms) and `TSF_FUSION_ONLY` (17) |
| `resist[11]` | Damage taken from each element, in tenths (0, 5, 10, 15, 20). Order is `TS_EL_FIRE, _WATER, _PLANT, _ICE, _ELECTRICITY, _EARTH, _STEEL, _WIND, _LIGHT, _DARK, _NULL` |
| `evoFirst`, `evoCount` | This form's routes in `TS_EVO_TBL` (`evoFirst` is 0 when `evoCount` is 0) |
| `learnFirst`, `learnCount` | This form's learnset in `TS_LEARN_TBL` (`ts_skills.h`) |

`TS_STAGE_ENTRY[7]` = `{ 1, 2, 7, 25, 49, 61, 73 }` holds the stage-timer
minimums as levels (level = hours since hatching + 1).

### `TsEvo TS_EVO_TBL[TS_EVO_COUNT]` (1025 routes)

Grouped by source in dex order, and in the game's order within each source. A
de-digivolution is any route read backwards (`to` -> `from`).

| Field | Meaning |
|---|---|
| `from`, `to` | Dex ids, both raisable |
| `min[6]` | Raw stat minimums by `TS_STAT_*`; 0 = none. HP minimums are kept even when they exceed the source's level-99 HP (65 kept routes; the summary prints the count): the de-digivolve HP boost (Q16) is what reaches them |
| `minLevel` | The SP minimum turned into a level: the first level (1-99) at which the source's untrained SP, growing linearly from level 1 to level 99, reaches the minimum. 0 = no SP minimum |
| `rank` | Agent Rank needed. 0 for every route into a stage below Ultimate. The game's value is kept for routes into Ultimate or later: 5 into Ultimate, 7 into Mega, 8 into Mega+ |

Kept routes are those between two raisable forms that need no Jogress, item,
Agent Skills or Talent. `JOGRESS_SHIPPED` in the generator makes the
fusion-only forms raisable and restores the routes among them, such as
Omnimon (X Antibody) -> Omnimon. Setting it is not enough on its own: the
Jogress work must also add the Jogress pairs to `validate()`'s reachability
check and update its expected counts (419 / 33 / 17).

### `TsJogress TS_JOGRESS_TBL[TS_JOGRESS_COUNT]` (17 pairs, data only)

| Field | Meaning |
|---|---|
| `a`, `b`, `to` | The two partners and the result, as dex ids. Each pair is listed once; the game's personality requirement is dropped |
| `min[6]`, `rank` | The game's stat minimums and Agent Rank: 5 for the four Ultimate fusions (Shakkoumon, Silphymon, Dinobeemon, Paildramon, which are also reached by ordinary routes), 7 for Millenniummon and Mastemon, 8 for the rest |
| `partnersRaisable` | 0 when a partner can never be a partner in v1. This holds only for EmperorGreymon + MagnaGarurumon -> Susanomon (both Hybrid) |

### `TsBoss TS_BOSS_TBL[13]`

| Field | Meaning |
|---|---|
| `dex`, `level`, `stage` | The ladder from the "Boss ladder" row, in unlock order: DemiDevimon 15 ... Apocalymon 80 |
| `kind` | `TS_BOSS_AREA` (8, a badge each), `TS_BOSS_ELITE` (4), `TS_BOSS_CHAMPION` (1) |

## `ts_skills.h`

### `TsSkill TS_SKILL_TBL[TS_SKILL_COUNT]`

| Field | Meaning |
|---|---|
| `name` | Upper-case ASCII. Accents are stripped (Grau Lärm -> `GRAU LARM`). Names are not truncated; the longest is 30 characters, against today's 12 |
| `element` | `TS_EL_*`, or `TS_EL_NONE` for support skills |
| `cat` | `TS_SC_PHYS`, `TS_SC_MAGIC` (uses magic attack against magic defence), or `TS_SC_SUPPORT` |
| `power[3]` | Learnable attack skills: power for tier I, II and III, chosen by the partner's level (I below 25, II at 25-60, III from 61). Signature skills: one value, repeated. Heals: the heal power |
| `hits` | Fixed hit count; power is per hit. 0 for support skills |
| `acc` | Accuracy in percent |
| `effect`, `chance`, `param`, `statMask` | The skill's one effect (see below). `chance` (percent) is used by `STATUS` and `STAT` |
| `flags` | `TS_SKF_SELF`: the effect lands on the user. `TS_SKF_WAS_AOE`: an all-enemies skill in Time Stranger, now single-target. 163 signatures have it; the balance pass may give them a power boost (Mega Flame, ~55-60) |

Effects (`TS_EF_*`):

| Effect | `param` |
|---|---|
| `STATUS` | `TS_AIL_PARALYSIS, _INJURY, _POISON, _SLEEP, _CRYSTAL, _CONFUSION` (1-6) |
| `STAT` | Signed percent (-50 ... +30) applied to every stat in `statMask` (`TS_SM_ATK, _DEF, _SPD, _MATK, _MDEF`). Time Stranger's own numbers are kept; mapping them to engine stages is a cutover choice |
| `DRAIN` | Percent of the damage dealt that heals the user |
| `RECOIL` | Percent of the user's max HP lost (100 = the user faints) |
| `HEAL` | Percent of max HP restored; 0 = by the skill's power and the user's magic defence (Time Stranger's SPI heal) |
| `FIRST` | Moves first |
| `NEVER_MISS` | Skips the accuracy roll |
| `RECHARGE` | The user loses its next turn |
| `CHARGE_PHYS`, `CHARGE_MAGIC` | Learnable only: the user's next physical or magic attack is boosted |
| `CURE` | Learnable only: clears the six statuses |
| `REMOVE_BUFFS`, `REMOVE_DEBUFFS` | Learnable only: clears the foe's stat boosts, or the user's stat drops |

### The 44 learnable skills (ids 1-44)

- **Ids 1-22, attack skills.** One per element and physical/magic, in
  `TS_EL_*` order with physical first. Each is the single-target I/II/III
  family; the all-enemies family of the same element and kind folds into it
  (Ignition Flare into Wolkenapalm). Tier powers come from the single-target
  rows, even for tiers that no Digimon learns.
- **Ids 23-44, support skills.**
  - Stat skills: Attack, Guard, Magic, Mental and Speed Charge (+30%; each
    Field folds in), and Attack, Guard, Magic, Spirit and Speed Bind (-20%;
    each Break folds in).
  - Acceleration Boost and Spell Boost.
  - Heal (Aura folds in) and X-Heal (X-Aura folds in).
  - Restore (Meditator and Normalize fold in).
  - Reset Body and Dispel.
  - Idle Bubble (Sleep), Panic Wisp (Confusion), Ripping Net (Paralysis),
    Venom Trap (Poison) and Texture Blow (Crystallization), each 60%.

  The generator classifies attachment rows by their description, not their
  name. Every one of the 181 rows maps to one of the 44 or is dropped for a
  named reason: Reflect, Taunt, Guts, Reverse, Revive, the immunity barriers,
  and crit up/down.

### Signature skills (ids 45-652)

These are all 613 special skills except the five named for dropping: Attract
Echo, Brain Rupture, Breath of the Gods, Table Flip and Purge Shine. Each keeps
its name, element, physical/magic, power, hit count and accuracy, and at most
one effect. The table comment on each row gives its `skills.json` row and any
simplification applied.

- **Clauses dropped**: trait bonuses, crit, "nullifies unfavorable
  compatibility", every conditional clause ("If ...", "The lower/greater ..."),
  SP costs and SP recovery, resistance up or down, evasion, max-HP down,
  Taunt, immunity, cures and buff removal on signatures, Chaos and Instant
  Death.
- **Statuses**: Deep Slumber -> Sleep, Deadly Poison -> Poison.
  Immobilization, and "Immobilized next turn" cast on the foe, -> Paralysis.
  On the user, "Immobilized next turn" is `RECHARGE`.
- **Counters** become plain attacks with the counter's element, kind and power:
  Maul Attack, Dragon's Roar, Graceful Lance, Weltgeist, and Circlet Defense
  (Aegiochusmon only). Monkey Wrench keeps its 50% ATK/magic-attack drop.
- **Percent-of-HP damage** becomes power = 4 x percent per hit (Death Charm,
  15% -> 60; Poop, 1% x 5 -> 4 x 5). It uses element Null, and is physical or
  magic by its user's stronger level-99 attack stat. Peace Fantasia's "fixed
  damage at power 10" (shown as 10%) is treated the same way.
- **One effect**: when several survive, the first in this order is kept:
  recoil, recharge, status, heal, stat, drain, moves first, never misses. 23
  signatures lost a second effect this way; the summary lists them.

### `TsLearn TS_LEARN_TBL[TS_LEARN_COUNT]`

Per form (`learnFirst` / `learnCount`), as `{ skill, hours }`:

- Signature skills come first with `hours` 0, given on becoming the form,
  whether by digivolving or de-digivolving.
- Attachment skills follow, sorted by learn level. Each `hours` is
  ceil(Time Stranger learn level / 5) after becoming the form. Each is mapped
  to its learnable skill. Dropped skills are removed, and so are later entries
  of a skill already in the list (Holy Light I and Shine Burst I are one
  entry).
- Learnsets exist for all 469 forms, because wild Digimon and bosses fight too.

## Resolved where HANDOFF is silent or ambiguous

| Question | Resolution |
|---|---|
| "16 fusion-only" | 17 are flagged: Susanomon also has no other route, and its partners are both Hybrids, so its Jogress pair has `partnersRaisable = 0` |
| Display-name rule behind "29 too long" | Upper case, drop `():`, keep spaces |
| Hybrids' attribute "Variable" | Its own value, neutral like Free |
| SP minimum above the source's level-99 SP (13 kept routes, e.g. Lillymon -> Rosemon) | `minLevel` 99: there is no SP boost to reach it otherwise |
| Rank on Rookie -> Ultimate (Shoutmon -> OmniShoutmon), Ultimate -> Ultimate, Mega -> Mega | The rule follows the target's stage: kept from Ultimate on (5 / 7) |
| Aura, X-Aura | Fold into Heal and X-Heal, as each Field folds into its Charge |
| Signatures left with nothing | Accuracy down (Blinding Ray, Kudamon's only signature) -> ATK and magic attack down by the same percent. Damage-taken down (Bee Cyclone, Elec Guard) -> DEF and magic defence up |
| "Power 30 or 120" (Dead or Alive), "3-6 hits" | The mean, rounded down (75; 4 hits) |
| Random or either-or (Trinity Arm, Miracle Bomb) | The first attack (Steel physical 110; Fire physical 110) |
