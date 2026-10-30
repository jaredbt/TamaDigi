# Handoff: custom TamaDigi (Time Stranger roster)

Status as of 2026-10-30. Supersedes the original notes on the `claude/handoff`
branch of `jaredbt/Esp-J` (archived).

## What this is

A custom build of TamaDigi, the Digimon V-Pet firmware for the **Waveshare
ESP32-S3-Touch-AMOLED-1.75-G** (ordered, not yet arrived), reworked to use the
roster and evolution tree of *Digimon Story: Time Stranger*.

## Repository and history

TamaDigi (joop0810/TamaDigi) was published as a single commit with no history.
It is eperdeme/TamaPoke v4.1.1 plus joop0810's Digimon conversion, and the code
descends socquique/TamaPoke -> DylanPDao/TamaPoke -> eperdeme/TamaPoke. This repo
restores that history: `main` = eperdeme's full history (267 commits back to
socquique's first) plus joop0810's commit re-parented onto `f7f02f8` (tree
identical to joop0810's `4c2a48d`; author and date kept). `git blame -C -M`
credits all four authors.

- Remotes: `origin` (jaredbt/TamaDigi, public), `joop0810`, `eperdeme`, `dylan`, `socquique`.
- `joop0810-original` = the untouched single-commit version.
- socquique made 64 commits after the June 14 fork point that no fork merged
  (48 never copied either). Worth porting: **v1.18 battery work** (adaptive frame
  rate, static screens, amp gating, light sleep, LiPo curve, safe power-off) and
  **run-merged sprite drawing**; then check the Sep 7 fixes (v1.6-v1.14) one by one.

## Where things stand

- **v1.0.0 is released** ([release](https://github.com/jaredbt/TamaDigi/releases/tag/v1.0.0)):
  joop0810's game unchanged except English serial logs, as the baseline to flash
  first when the board arrives and the rollback for later releases. The installer
  is this fork's own, at <https://jaredbt.github.io/TamaDigi/web/>, rebuilt by
  GitHub Pages on every push to `main`.
- **CI is green.** The firmware tests had failed since joop0810's commit because
  `tools/emu/tests/native/native.ino` used Pokémon dex numbers (PR #1). The
  old-save upgrade test he deleted is back (PR #2), with a **frozen v1.0.0 save**.
- **The name stays TamaDigi** (TamaMon, DigiGotchi and TamaDigiv2 were considered;
  the first two clash with existing projects, the last reads as joop0810's next
  version). The README calls it "TamaDigi: Time Stranger fork".
- **Time Stranger data is in the repo**: `tools/data/time-stranger/` (stats,
  evolution requirements, attributes, elemental resistances, every skill and each
  Digimon's skills, the story bosses, `unlock_check.py` behind the training
  decision and `roster_audit.py` behind the pipeline decisions). A review copy of
  the skills ("TamaDigi: Time Stranger skills") is in the owner's Google Drive.
- **Every design and data decision for v1 is made** (2026-10-28): the design
  table below. What is left is presentation and balance tuning (see "Still open").
- **The cutover is half done** (2026-10-30). Phase 1 put the firmware on the Time
  Stranger tables: roster, stats, routes, skills, battle, items and saves. Phase 2 is
  listed under "Firmware change list", and "Stubs phase 2 replaces" says what is
  standing in for it.

### Next, in order

1. Phase 2 of the firmware cutover, **on a branch with a PR** (CI is the only test run,
   and `main` deploys Pages): Agent Rank, the 13-boss ladder and Hard mode, wild
   encounter rules, eggs, the Digidex stage picker and states, partner history, and
   Jogress as a stretch goal.
2. Before the board arrives: pull the hardware bring-up notes back from upstream's
   CLAUDE.md (`git show f7f02f8:CLAUDE.md`: "Hardware bring-up order", "First
   bring-up, actually done", "Found by hand on the board, not by any test").
3. After the rework: rebuild the README's "game manual" of exact numbers
   (upstream's is at `git show f7f02f8:README.md`).

### What joop0810's conversion left out

Checked against eperdeme's v4.1.1 (`f7f02f8`): the CI and release tooling were only
renamed, not loosened, and no Pokémon leftovers remain in firmware code. Gaps:
the upgrade test (restored), the committed release binaries (restored), the
README's game manual and about 1,100 lines of CLAUDE.md notes (still in history),
per-Digimon base stats (stage x one of six profiles; Approach B replaces it), and
stale test comments saying Ver.3 has no Baby I art (it has, now).

## Game design decisions

| # | Decision |
|---|---|
| Roster | Time Stranger's, **one single roster**: no "versions" (joop0810's term is dropped). v1 has **469 forms** (all 475 except the Aegiomon line); 448 sit on the evolution tree, and Armor, Hybrid, the 12 wild-only forms and (until Jogress) the fusion-only forms can be met but not raised (see Stages, Wild-only Digimon, Jogress). A form with no art never hatches, evolves or appears (the existing `NO_ART` mechanism). The first egg hatches a random In-Training I; eggs, wild Digimon and the boss ladder are their own rows |
| Evolution | Time Stranger style: choose among branches whose conditions are met. TamaDigi's stage timers (6/24/48/60/72 h) are **minimums**; evolution is offered, never forced ("keep form" re-offers next level) |
| De-digivolve | Allowed (Q5): into any form that evolves into the current one, if it is in the Digidex. The earlier form then waits out its stage again (Q8) |
| Level | Stays as age (Q4); no XP-based level |
| Bond | Sets how much training carries over on (de-)digivolution (Q6) |
| Battles | Kept (Q3). Wins earn XP; training mini-games cost XP and nothing else; Energy is only for battles. XP balance is player-wide (Q10) |
| Agent Rank | Player-wide, from the boss ladder: rank = 1 + area badges held (ranks 2-9 from the 8 area bosses), rank 10 for beating the champion (Q7, Q4). Evolution checks ignore Time Stranger's Agent Rank requirement up to Champion and apply it from Champion->Ultimate (rank 5), Ultimate->Mega (7) and Mega->Mega+ (8) (Q1). Revisit for balance: little to do at rank 10 once stats cap |
| Fallback | **No battling needed up to Champion** (Q9, amended by Q1b): every In-Training and Rookie form keeps an evolution that needs no battles. From Ultimate on, the rank gates make boss battles part of the way up |
| One partner | **No teams, anywhere** (Q4a): no player party and no boss teams. One partner Digimon at a time, V-Pet style; every battle is 1-on-1 |
| Eggs | After the first, each egg hatches a random In-Training I (7 in the roster), leaning toward lines with Digimon you have not raised yet; the colour-variant roll stays; the empty rare/legendary egg tiers go (Q2) |
| Wild Digimon | From the whole roster (Armor and Hybrid by their stat tier, see Stages), by the stage your partner's level has reached: 65% that stage, 25% one behind, 10% one ahead at your level. Mega+ appears only for a Mega+ partner; for a Mega the "ahead" roll stays Mega (Q3, Q3b, Q3c) |
| Boss ladder | One ladder of 13 single Digimon, unlocked in order: 8 area bosses (each gives a badge), 4 elites (Mega), a champion (Mega+). A mashup of the villains from the anime's first two seasons (Adventure, Adventure 02) (Q10, Q10b). Lineup (Q10e, 2026-10-28), level and stage: area 1 DemiDevimon 15 Rookie; 2 Ogremon 22, 3 Unimon 32, 4 Devimon 40 (Champions); 5 Etemon 47, 6 Myotismon 54, 7 Mummymon 58, 8 Arukenimon 60 (Ultimates); elites MetalSeadramon 64, MetalEtemon 66, MaloMyotismon 68, Diaboromon 70 (the *Our War Game!* movie; highest HP, ATK and SPD of the elites); champion Apocalymon 80. The Ultimate tier opens right after the 4th badge gives rank 5, as Mega opens after the 6th gives rank 7. Time Stranger's own story bosses (`tools/data/time-stranger/bosses.tsv`) are not on the ladder; they could return as optional post-champion fights. To do: check the model viewer has a model for each (all 13 are in `stats.tsv`) |
| Hard mode | Easy: your partner fights at its real level. Hard: if your partner is above the boss's level it is scaled down to it, never up, so waiting cannot win a Hard badge. Hard badges are separate and optional; Agent Rank counts Easy badges only; Hard gives about double the training reward (Q6, Q6b) |
| Partner leaves | After a farewell, runaway or release the partner leaves for good: marked "raised" in the Digidex and added to a short partner history on the tamer card (name, final form, age, how it ended); then the next egg (Q8) |
| v1.0.0 saves | **Not carried forward** (Q9). The new firmware recognises a v1.0.0 save, says so, and starts a new game (the installer backs the save up first). `upgrade_test`'s frozen v1.0.0 save then checks it is detected and replaced cleanly, never misread |
| Digidex | A stage picker (In-Training I ... Mega+, then Armor and Hybrid, with counts) replaces the version picker; inside, the existing 16-per-page grid in Time Stranger dex order (Q5, Q5b). Smooth scrolling: revisit later |
| Digidex states | Unseen (silhouette, "???"), seen (met in the wild or a boss fight: name and sprite), scanned (stats and its evolution requirements: which forms digivolve into it and the stats/rank each route needs), raised (was your partner: everything plus your record). When choosing an evolution, a branch into a scanned or raised Digimon shows its requirements, any other shows "???" (Q7, Q7b). Scanning (Q7c): no scan chips; each wild win against a Digimon adds a third of a scan, so three wild wins mark it scanned; beating a boss scans it fully. The Digidex shows partial progress ("Scan 1/3", "2/3") (Q7d). The bag keeps recovery disks, stat chips and the ALL CURE item (it also clears Crystallization) |
| Stages | Mega+ is a stage after Mega, from 72 h (Q11). Armor and Hybrid cannot be raised in v1 but are **wild-only** (2026-10-28), placed in wild encounters by stat tier: Champion tier for the 8 Digi-Egg Armor forms and Agunimon, Lobomon, Lanamon, BurningGreymon, Calmaramon, KendoGarurumon; Ultimate for Aldamon, Beowolfmon; Mega for Rapidmon (Armor), Magnamon, EmperorGreymon, MagnaGarurumon (and Detached). The Digidex stage picker gains Armor and Hybrid pages. **Aegiomon and its five Aegiochusmon forms are left out of v1 entirely** (2026-10-28): neither partners nor wild Digimon, so the v1 roster is 469 forms, 448 of them raisable in principle |
| Elements (Q13, 2026-10-28) | **Time Stranger's own system**: 11 elements (Fire, Water, Plant, Ice, Electricity, Earth, Steel, Wind, Light, Dark, Null), and each Digimon takes 0, 0.5, 1, 1.5 or 2x damage from each (`tools/data/time-stranger/resistances.tsv`), on top of the Vaccine > Virus > Data > Vaccine triangle (Free, Unknown, No Data neutral; `attributes.tsv`). Replaces the one-element-per-Digimon chart |
| Skills (Q14, 2026-10-28) | **Time Stranger's own skills**, and the game calls them skills, not moves. Each Digimon gets its special (signature) skills and the attachment skills it learns by level (`tools/data/time-stranger/skills.json`), converted to TamaDigi's battle format. SP costs go with the SP stat; personality skills (passives) are for later |
| Skill learning (Q14b, 2026-10-28) | Four skill slots, as today. Becoming a form (by digivolving **or de-digivolving**) gives its special skills at once; each of its attachment skills unlocks **ceil(Time Stranger learn level / 5) hours later** (level 2 -> +1 h, 25 -> +5 h, 45 -> +9 h), because Time Stranger counts learn levels per form and TamaDigi's level is age (taken literally, MetalGreymon would get 5 skills the moment it evolves). Skills already in a slot are skipped. Learned skills carry over on evolution and de-digivolution, **special skills included**: any form can use any skill in its slots (an Agumon that was a Greymon keeps Mega Flame; it earned it). A free slot fills silently; a full set makes an offer (replace a slot or skip). **Dropped or skipped is gone**: no record of past skills; to get one back, de-digivolve into the form that has it (e.g. Greymon -> Agumon for Pepper Breath). Botamon -> WarGreymon at stage minimums gave 14 offers over three days before the 44-skill merge (fewer now: other tiers of a known family are skipped). Targets: see Learnable and Signature skills |
| Status effects (2026-10-28) | Keep it simple. **Injury** takes over the engine's Burn (Pokemon-style chip damage each turn and halved physical attack), caused by any element, so Burn's "Fire can't be burned" immunity goes. **Crystallization** takes over the engine's Freeze: no actions until it wears off (Freeze's per-turn thaw chance) or the ALL CURE item clears it; Freeze's "Water can't be frozen" immunity goes. **Deep Slumber** folds into Sleep and **Deadly Poison** into Poison. **Chaos** (11 skills) and **Instant Death** (7) are dropped as effects: those skills keep their damage, and Death Lure keeps its SPI drop. Paralysis, Confusion and Sleep map directly |
| Learnable skills (2026-10-28) | Time Stranger's 181 attachment skills become **44**. (1) **One skill per I/II/III family**; its power comes from the partner's **level**, in the stage-timer bands: tier I below level 25, II at 25-60, III from 61. Level is age and never drops, so a de-digivolved partner keeps its power (Greymon -> Agumon: Wolkenapalm stays at 60). Learnset entries for another tier of a family already known are skipped. (2) **All-enemies families fold into their single-target twin** (Time Stranger has exactly one of each per element and physical/magic, e.g. Ignition Flare into Wolkenapalm), using the single-target powers: **22 attack skills**, one per element and physical/magic. (3) **22 support skills**: one up and one down skill each for ATK, DEF, magic attack, magic defence and speed (Field = Charge, Break = Bind in 1-on-1), physical and magic charge, Heal and X-Heal, one status cure, remove buffs, remove debuffs, and one skill each for Sleep, Confusion, Paralysis, Poison and Crystallization. Dropped: party-only skills (Reflect, Taunt, Guts, Reverse, Revive, immunity barriers), crit up/down, and the 13 skills no Digimon has. |
| Signature skills (2026-10-28) | All 613 special skills stay (cutting to one per Digimon was considered and rejected: it removes ~150 entries, no rules, and most Megas' second finisher). Each keeps its **name, element, physical/magic, power, hit count and accuracy, plus at most one effect** from: one of the six statuses (with a chance), a stat up or down, drain, recoil, heal, moves first, never misses, or a lost next turn. **Dropped**: trait bonuses, all-enemies targeting (single target in 1-on-1; all-enemies signatures such as Mega Flame need a power boost, ~55-60), conditional clauses ("if the target has already acted", "the lower the user's HP"), SP costs; percent-of-HP damage becomes ordinary power. Counter skills (Maul Attack, Dragon's Roar, Graceful Lance, Weltgeist) become plain attacks with the counter's element and power; Monkey Wrench keeps its ATK/magic-attack drop. Attract Echo, Brain Rupture, Breath of the Gods, Table Flip and Purge Shine are dropped; every Digimon keeps at least one signature (dropping all ten would have left Grizzlymon and KingEtemon with none). After this the 613 signatures have about 325 distinct behaviours |
| SP requirements (Q15, 2026-10-28) | SP is dropped as a stat, so each evolution's SP minimum becomes an age minimum: the level at which the source form's untrained SP would reach it (as HP already works). An SP minimum the source can never reach is **dropped** (13 routes, among them every way into MetalGarurumon, Rosemon and Apocalymon); Agent Rank and the trainable stats still gate them |
| Wild-only Digimon (2026-10-28) | Twelve forms need a Time Stranger mechanic TamaDigi lacks and are **wild-only in v1** (in the Digidex, seen and scanned, never raised): Agent Skills (Jupitermon, Jupitermon WM, Chronomon HM, Chronomon DM, Agumon (Bond of Bravery), Gabumon (Bond of Friendship)), Talent (Lucemon, Lucemon CM, Lucemon SM, Parallelmon), items (Imperialdramon PM, Magnamon (X Antibody)). Later stand-ins: Bond for Agent Skills and Talent, boss rewards for items. The fusion-only forms are wild-only too until Jogress ships |
| Jogress (2026-10-28) | One partner, so fusion uses a second Digimon from outside the party: your partner fuses with **either a Digimon in your partner history** (it must have left as that exact form; the history entry stays) **or a wild Digimon you just defeated** (a prompt after the win; declining is a normal win). Time Stranger's Agent Rank requirement stays (rank 8, Millenniummon and Mastemon 7); its personality requirement goes. Once your partner is eligible for a fusion (right form and rank), its fusion partner appears far more often in the wild (about 1 battle in 5 instead of ~1 in 200). De-digivolving a fused Digimon returns to your own half (Omnimon -> WarGreymon); the HP boost applies as usual. Makes the 16 fusion-only Digimon raisable (Omnimon and its forms, Millenniummon, Mastemon, GraceNovamon, Alphamon: Ouryuken, both Chaosmon, Examon, Craniamon + Enbarrmon, Titamon + SkullBaluchimon); Susanomon needs two Hybrids, which are not in v1. **A stretch goal for v1**: built if time allows; until it ships, the fusion-only Digimon are wild-only |
| Stats storage (2026-10-28) | The built-in species table stores Time Stranger's **raw** level-1 and level-99 values for HP, ATK, DEF, speed, magic attack (INT) and magic defence (SPI) as **16-bit numbers** (24 bytes a Digimon, about 11 KB for the 469 forms in flash); the scale factor (~0.1) is one constant applied in firmware, so balance can be tuned without regenerating data. Saves are unaffected (IVs, training and the de-digivolve count stay bytes); calculated stats are already 16-bit (worst case 487 HP, x2.0 boost = 974) |
| Stat mapping (Q12a/b) | HP, ATK, DEF, SPD direct; INT -> Special Attack; SPI -> Special Defense; **SP dropped** |
| Stat formula (Q12e) | **Approach B**: rebuild TamaDigi's formula on Time Stranger growth. Each species grows linearly from its TS level-1 to its TS max (level 99) stats (`tools/data/time-stranger/stats.tsv`, 475 forms incl. personality), times one scale factor (~0.1), plus training. TamaDigi level = TS level, one per hour. Requirements convert with the same factor; no per-stage scaling. Battle balance must be redone |
| HP (Q12c, Q16) | **Not trainable**: grows with age only (stats still reach Time Stranger's maximum at level 99), so HP requirements act as timing gates (e.g. Agumon -> Greymon at ~hour 37). **De-digivolve HP boost (Q16, 2026-10-28)**: each de-digivolution multiplies the partner's calculated HP by a further 10% (x1.1 after one, x1.2 after two ...), **capped at x2.0**, kept through later evolutions and reset with a new partner; the save stores the count. This is TamaDigi's stand-in for Time Stranger's level-reset loops (which carry stats over and which its requirements assume); training already covers the other stats. It makes all 66 HP requirements that exceed the source form's level-99 HP reachable: 45 need one loop, 11 two, 7 three, 2 four, Belphemon SM -> RM eight (x1.8). Example: Greymon de-digivolves at 40, is Greymon again at 58 with x1.1, and Greymon -> Kimeramon (2030 HP) opens at level 68 instead of 81. Known pacing: without loops, HP gates make typical Mega evolutions open around level 88 |
| Training (Q12d) | **Training is the bridge to late stages**: untrained growth reaches nearly all In-Training/Rookie evolutions, ~31% of Champion->Ultimate and ~6% of Ultimate->Mega. Per-session payout unchanged (up to +18). ATK training also raises Special Attack, DEF training also raises Special Defense (separate INT/SPI mini-games: next version). Examples: MetalGreymon needs ~41 ATK points (3-5 sessions), WarGreymon ~91 (6-10). **No evolution may be unreachable because of IV** (bad IV = train more): today's 70-100 IV-based cap blocked 74 of 308 Mega paths and gated WarGreymon on an ATK IV >= ~22, so the cap is now **255 for every Digimon** (the save field is a byte; the hardest Mega path needs ~192) |
| IV and training | IV scales each session's gain **70% (IV 0) to 130% (IV 31)**, so an average IV (~15) trains exactly as today. WarGreymon (~91 ATK points): ~13 typical sessions at the worst IV, 10 average, 7 best; MetalGreymon (~41): 6 / 5 / 4. Low IV also means more XP, so more battles. Idea: show it to the player ("trains fast" / "slow learner") on the status card |

### Resolved while building phase 1

| Question | Resolution |
|---|---|
| Same-element bonus | **x1.5 when the skill's element is the form's home element**: the element of its first attacking signature skill, or for the 18 forms with none, the element it resists best (ties to the first). The resistance rule alone was the other candidate: only 272 of the 451 forms with an attacking signature resist that signature's element best (Tentomon's Super Shocker would get no bonus), and 402 forms tie for their best resistance, which would make Devimon a Fire form. The UI needs one element per form anyway, so the card, the accent colour, the biome and the bonus share this one answer (`homeElement()`) |
| Stat changes in battle | Time Stranger's percentages, stacked and lasting the fight, capped at -60%..+90%. It uses 3-turn durations; TamaDigi's fights are shorter |
| Attribute triangle multipliers | The existing x1.3 / x0.8 kept: the design table gives no numbers |
| A resistance of 0 | A real immunity: 0 damage, narrated as such (74 cells). The old chart had none |
| Heal by power | power x magic defence / 20, about half of max HP at power 30 across the roster. Time Stranger's formula is not in the data |
| Charge Physical / Magic | The next attack of that kind hits twice as hard, once. The data gives no number |
| A support-only partner | **The basic attack**: Time Stranger's Attack command. Null physical, power 30, in no slot, on the battle menu's free cell. Punimon and Botamon hatch with only a support skill; without it a fight could not end |
| Skill names | English in every language, like Digimon names |
| IVs | No longer add to a stat. They scale mini-game sessions, and name the personality. Boss rewards, chips and the passive DEF tick are not scaled: they are not sessions |
| Replacing a v1.0.0 save | The settings (language, sound, volume, brightness) are kept |
| Learn offers | Saved with the creature. They used to live in RAM, which would have made a reboot a silent "skip" and dropped is gone |
| Care mistakes, wins | Mistakes no longer pick a branch; they stay on the card and in a medal. `wins` is gone |
| `TS_STAT_SCALE_PCT` on HP minimums | The same one constant as every other minimum |

## Art: pre-rendered Time Stranger frames (private builds only)

The Digimon models come from Nuffle's DSTS Model Viewer
(<https://nuffle.me/dsts-viewer>, source <https://tangled.org/nuffle.me/dsts-viewer>,
MIT); frames are captured through that viewer's dev-only recording API, run
locally (`tools/prerender/`). **Never commit or publish models,
renders or packed sprites**: they are Bandai Namco's assets (`tools/prerender/.gitignore`).
A public build would need other art (the Vital Bracelet sprites + mood symbols
were the tested fallback).

- Camera: side-on facing left, azimuth 70, polar 82, zoom-out 1.5; per-Digimon
  overrides (WarGreymon read better at 35).
- Capture at 24 fps (the game's rate); thin for the device later.
- Size: by real in-game height, compressed: stored idle px = 100 * (H / 3.25)^0.5
  (Greymon 3.25 units -> ~200 px on screen, Koromon ~81, Agumon ~104), drawn at 2x.
  Small sprites' idle canvas is padded so the firmware keeps them at 2x.
- No pale rim.

### Action mapping (TamaDigi slot <- Time Stranger clip, eyes)

| Slot | Clip | Eyes |
|---|---|---|
| Idle | bn01 | open |
| Walk L/R | br01 (mirrored for R); Agumon e011; Koromon br01 at 0.6x | open |
| Sleep | bn02 (low-HP idle) | closed |
| Eat | fe01 | open |
| Hurt | bd01 | closed |
| Sad (**new slot**) | fq02 ("No") | closed |
| Attack, physical | ba01 | angry |
| Attack, special (**new slot**) | ba02 | angry |
| Pose / cheer | fe02 | smile |
| Hop (exists, unplayed) | bv01 | smile |
| Nod | fq01 | open |
| Deep breath | fe03 yawn | open > closed (30-76%) > open |
| No slot yet | refuse fq02 (open), knockdown bd02 (closed), getup bd03 | |

ba01 tends to be the physical attack and ba02 the special one (a trend, not a
rule): play by move category (MC_PHYS / MC_SPEC), swap per Digimon where it runs
the other way, fall back to ba01. Status moves have no animation yet (bg01 Guard
is a candidate).

## Screen

- Backdrop: original V-pet-style landscape, full screen, day 35% white / 66%
  saturation, night 45% / 66% plus a blue tint (`art/backdrops/`). Original art,
  safe to commit.
- No cream panel behind the buttons; they float on the backdrop.
- Open: light tile labels (EXPLORE and the rest) at night; dark ones are hard to read. The PARTY tile goes with the party.

## Firmware change list

From the design table (each row has the detail):

**Done in phase 1 (2026-10-30):**

- **Species table**: `ts_dex.h` and `ts_skills.h` replace `dex.h`, `moves.h` and
  `move_names.h`; `roster.h` is the layer the firmware reads them through. Seven
  raisable stages. "Versions" are gone: the version pickers, the egg's version pill,
  Explore's version selector, the per-version ladders and packs.
- **Art stand-in**: the 73 Digimon Color sheets that match a form by name
  (`tools/data/art/digimon_color.tsv`), one pack, `noart.h` as a bitmap.
- **Stat formula** (Approach B), training to 255, IV scaling each session 70-130%, and
  the de-digivolve HP multiplier with its count in the save.
- **Evolution**: `routeMet()`, branches offered and never forced, de-digivolution with
  the form waiting out its stage.
- **Skills**: four 16-bit slots, power by level band, specials on becoming a form,
  attachment skills at their hour offset, offers kept in the save, fixed hit counts.
- **Battle**: Time Stranger's element rows and the triangle, the same-element bonus,
  six statuses, every skill effect, no element immunity to a status.
- **Items**: scan chips and catching are gone; the bag keeps the recovery disks, the
  stat chips and ALL CURE.
- **Saves**: new record formats, and v1.0.0 saves recognised and replaced
  (`saveReplaceOld()`), with `upgrade_test` rewritten around the frozen save.

**Done earlier (PR #4, 2026-10-29):** the party and box are removed; battles are 1-on-1,
LAN sends only the partner.

**Still to do, phase 2:**

- **Progress**: Agent Rank from badges, the 13-boss single-Digimon ladder with Hard
  mode, wild encounters by stage (65/25/10), eggs, Digidex states with scan progress
  (no scan chips), partner history, Jogress (stretch goal).

**Stubs phase 2 replaces:**

| Stub | Where | What it does now |
|---|---|---|
| Agent Rank | `AGENT_RANK_STUB`, `Pet::agentRank()` | 255: every rank check passes. `digivolve_test` pins this and has to flip |
| Boss ladder | `tools/gen_ladders.py`, `TRAINERS` in `trainers.h` | `TS_BOSS_TBL`'s 13 slots and levels, each boss swapped for a form with art of its stage where it has none. Only 4 of the 13 real bosses have art |
| Hard mode | `startTrainerBattle()` | A smarter AI and a double reward. IVs no longer touch a stat, so `HARD_IV`/`EASY_IV` do nothing. Both difficulties cap your level |
| Wild encounters | `startWildBattle()`, `wildPickSpecies()` | The old band (level ±5) and 75/25 stage roll, on `tier`, stepping down when a stage has no art. Mega+ has none |
| Eggs | `Pet::pickEggSpecies()`, the starter screen | The first egg is a choice of the In-Training I forms with art, not a random one. Later eggs lean toward unraised lines |
| Digidex | `galleryPass()`, `renderGallery()` | One flat grid of the forms with art, "registered" meaning raised. No stage picker, no seen/scanned states |
| Evolution offer | `drawChoiceDialog()`, `declineEvolve()` | The old two-button dialog names the branch on offer; "keep form" moves to the next one next level. Choosing properly is "Still open" |
| Partner history | nothing yet | `Pet::frozen` also survives from the removed box, and nothing sets it |

**Findings from phase 1, for the balance pass:**

- The stat scale of 0.1 works as a first pass: fights between same-level Digimon run
  about 10 turns at level 10, 5 at level 30, and 3 from level 70 on, shortest 1 turn.
  They are short, as `battle_test` prints. Level-1 fights take ~27 turns on the basic
  attack.
- Training dwarfs growth: the cap adds up to 255 to a stat that is about 200 at the
  top of the roster, and ATK training feeds two stats. A fully trained partner hits
  about twice as hard as the same form untrained.
- The ladder is rough because a boss's stage outruns the stage clock. `gym_test` prints
  a GREYMON's win rate against each boss: untrained it falls to 0% from DIGITAMAMON
  (level 54) on, trained it recovers. The real ladder will need the same look.
- `battleDamage()`'s +2 floor is per hit, so below about level 10 a 7-hit skill does
  several times what a 1-hit skill of the same family does. From level 25 the families
  are within about 25% of each other. Multi-hit skills are not learned that early in
  practice, so it is left alone.
- `TS_SKF_WAS_AOE` signatures have no boost yet (Mega Flame is power 40). That is
  still the balance pass's.
- On public art only 37 forms can be reached from the three hatchlings using forms with
  art alone: In-Training II 3, Rookie 5, Champion 13, Ultimate 10, Mega 3.

**Later:**

- Raise the 24-frames-per-action cap (`PmdAct.ms[24]`) and the 3 MB sprite file
  cap (Greymon is already 2.8 MB).
- New sprite slots: Sad (sad mood currently plays Hurt) and special attack; play Hop.
- Pick the attack animation by move category.
- Battle timing: attack lunge 260 ms and hit 420 ms cut the 1-2.3 s animations;
  stretch the windows or trim to the impact.
- Pet size: keep or replace the padding trick; cap Megas (Greymon's horns already
  reach the status text).
- `drawScene` -> draw the backdrop image; remove the button-panel fill; night label colour.
- Regenerate Digidex thumbnails for new sprites.
- Port socquique's battery and run-merged drawing work.

## Tooling

- `tools/prerender/actions.mjs`: capture action sets (`--action`, `--speed`,
  `--measure`, per-Digimon overrides, eye timelines).
- `tools/prerender/pack_tpk2.py`: pack captures into the TPK2 sprite format.
- `tools/prerender/preview/`: TDAG and comparison GIF builders.
- Needs: the dsts-viewer source (`tangled.org/nuffle.me/dsts-viewer`, MIT) running
  `vite` with `DSTS_MODELS=tools/prerender/models`, `playwright-core` + Google
  Chrome, Blender only for the abandoned render.py path.
- `tools/data/time-stranger/`: stats and evolution requirements (see its README).
- `tools/gen_ts_data.py`: the Time Stranger data pipeline. Applies the design table to
  `tools/data/time-stranger/`, asserts its counts and rules, and writes `ts_dex.h` and
  `ts_skills.h` (`--check` fails if they are stale). Format, and how the open cases were
  resolved: `docs/ts-data-format.md`.
- Emulator: `EMU_EPOCH=<unix secs>` sets the start time (`tools/emu/host_impl.cpp`).
- The full test suite (`tools/emu/tests/run.sh`) needs `arduino-cli` with the ESP32
  core and the `esp-emu` native emulator; neither is installed on this machine yet,
  so CI is the test run. A single host suite can be built by hand with the same
  `g++` line `run.sh` uses (firmware `.cpp` files plus the test).
- Releasing: see CLAUDE.md § Releasing. The firmware binaries are committed with
  each release; without `arduino-cli`, take them from the Pages build's
  `tamadigi-firmware` artifact.
- All comments, logs, scripts and docs are in English (see CLAUDE.md); only the UI
  string table keeps its six languages.

## Still open

- How evolution is offered, and how the 72-hour farewell is offered (revisit later).
- Battle timing (above), and the balance pass: the stat scale constant and boosts
  for all-enemies signatures such as Mega Flame (~55-60).
- Jogress (a v1 stretch goal); later stand-ins for the wild-only forms (Bond for
  Agent Skills and Talent, boss rewards for items); personality skills (passives);
  separate INT/SPI training mini-games.
- Public-build art: only 73 of the 87 Digimon Color sprites map onto Time Stranger
  forms, and they are what the game runs on today; the Vital Bracelet sprites (kongtaehun/digimon-assets-VB, no licence,
  Bandai's art) cover at least 188 forms by name.
- Model check for the 13 bosses (needs the game files in `tools/prerender/models/`).
