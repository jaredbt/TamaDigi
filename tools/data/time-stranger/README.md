# Time Stranger data

Reference data from *Digimon Story: Time Stranger* for the planned roster rework
(see `docs/HANDOFF.md`). Nothing in the firmware reads these files yet; the data
pipeline will.

| File | What it is | Source |
|---|---|---|
| `stats.tsv` | Level-1 and maximum (level 99) stats of 475 Digimon, with stage and personality | the game's own stat data, transcribed 2026-10-27 |
| `evolutions.json` | Evolution requirements of 475 forms | [MelbearX123/digivolve-target](https://github.com/MelbearX123/digivolve-target) `server/data/digimon_evolutions_clean.json` at commit `e31958f`, MIT (`evolutions.LICENSE`) |
| `bosses.tsv` | The game's boss fights in story order, matched to the roster's names and stages (the ladder itself uses anime villains; these could return as optional fights) | [Game8's boss list](https://game8.co/games/Digimon-Story-Time-Stranger/archives/553558), 2026-10-27; stages from `stats.tsv` |
| `attributes.tsv` | Each Digimon's dex number (the game's Field Guide order, which `stats.tsv` follows row for row) and attribute | the game's data, read from Game8's Digimon list, 2026-10-28 |
| `resistances.tsv` | Each Digimon's damage multiplier against the game's 11 elements (Fire, Water, Plant, Ice, Electricity, Earth, Steel, Wind, Light, Dark, Null): 0, 0.5, 1, 1.5 or 2 | the game's data, read from Grindosaur's resistance table, 2026-10-28 (copied with a checksum; names in the same order as `stats.tsv`) |
| `skills.json` | Every skill (181 attachment, 40 personality, 613 special: element, damage type, SP cost, accuracy, crit rate, power, description) and each Digimon's special skills and attachment skills learned by level | the game's data, read from Grindosaur's skills page and each Digimon's page, 2026-10-28 (checked by hash; names in `stats.tsv` order) |
| `roster_audit.py` | What the data pipeline has to deal with: stages, kept and dropped evolutions, reachability, training and storage limits, bosses, display names, public art | this project |
| `unlock_check.py` | When an untrained Digimon could first take each evolution under Approach B | this project |

## `stats.tsv`

Tab-separated, one row per Digimon:

- `name`, `stage`, `personality`;
- level-1 stats: `HP`, `SP`, `ATK`, `DEF`, `INT`, `SPI`, `SPD`, `TOTAL`;
- maximum stats, the same columns prefixed with `m` (`mHP` … `mTOTAL`).

Both tables were transcribed by hand. Every row's seven stats add up to its
`TOTAL` and `mTOTAL`; that check caught one transcription slip
(MetalGarurumon (Black)'s max HP), which is corrected here.

Stat mapping for TamaDigi (decided, see `docs/HANDOFF.md`): HP, ATK, DEF and SPD
map directly, INT becomes special attack, SPI special defence, and SP is dropped.

## `evolutions.json`

Keyed by Digimon name. Each entry has a `stage` and an `evolves_into` list of
`[target, conditions]`, where the conditions are stat minimums (`HP`, `ATK`, …),
`Agent Rank`, and sometimes `Jogress` partners, `Req. Item`, `Agent Skills` or
`Talent`. Names match `stats.tsv` once case and punctuation are ignored.

## `unlock_check.py`

```bash
python3 -I tools/data/time-stranger/unlock_check.py
python3 -I tools/data/time-stranger/unlock_check.py --json out.json   # every row
```

It grows each Digimon linearly from its level-1 to its maximum stats over
levels 1–99, one level per hour, and reports when each evolution's stat
requirements are first met, against TamaDigi's stage minimums. Evolutions that
need Jogress, items, Agent Skills or Talent are left out.

## `roster_audit.py`

```bash
python3 -I tools/data/time-stranger/roster_audit.py
```

It applies the decided filters (no Armor or Hybrid; no evolution that needs
Jogress, an item, Agent Skills or Talent) and prints one section per check.
Nothing reads its output; it is the evidence for the pipeline's decisions in
`docs/HANDOFF.md`.

## `skills.json`

`skills` holds the three tables as Grindosaur shows them (`attachment`,
`personality`, `special`). `digimon` lists the 475 forms in `stats.tsv` order;
each has `special`, row numbers into `skills.special`, and `learn`, pairs of a
row number into `skills.attachment` and the level it is learned at. Special
skill names repeat with different stats (Acid Bubbles is both a Fire and a
Water attack), so those rows were matched by Grindosaur's skill ID, not by name.
