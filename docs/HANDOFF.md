# Handoff: custom TamaDigi (Time Stranger roster)

Status as of 2026-10-27. Supersedes the original notes on the `claude/handoff`
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
  evolution requirements, and the `unlock_check.py` behind the training decision).

### Next, in order

1. The versionless design is decided (2026-10-28; the design table above).
   Left for later: how evolution and the farewell are offered (see "Still open").
2. Build the data pipeline: replace the 87-Digimon roster in `tools/digimon_data.py`
   with the Time Stranger roster and Approach B stats, and regenerate.
3. Before the board arrives: pull the hardware bring-up notes back from upstream's
   CLAUDE.md (`git show f7f02f8:CLAUDE.md`: "Hardware bring-up order", "First
   bring-up, actually done", "Found by hand on the board, not by any test").
4. After the rework: rebuild the README's "game manual" of exact numbers
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
| Roster | Time Stranger's (~448 forms), limited to Digimon with art. **One single roster**: no "versions" (joop0810's term is dropped). The first egg hatches a random In-Training I; de-digivolution then makes every Digimon reachable in principle. Open: later eggs, wild-encounter pools and boss ladders without versions |
| Evolution | Time Stranger style: choose among branches whose conditions are met. TamaDigi's stage timers (6/24/48/60/72 h) are **minimums**; evolution is offered, never forced ("keep form" re-offers next level) |
| De-digivolve | Allowed (Q5): into any form that evolves into the current one, if it is in the Digidex. The earlier form then waits out its stage again (Q8) |
| Level | Stays as age (Q4); no XP-based level |
| Bond | Sets how much training carries over on (de-)digivolution (Q6) |
| Battles | Kept (Q3). Wins earn XP; training mini-games cost XP and nothing else; Energy is only for battles. XP balance is player-wide (Q10) |
| Agent Rank | Player-wide, from the boss ladder: rank = 1 + area badges held (ranks 2-9 from the 8 area bosses), rank 10 for beating the champion (Q7, Q4). Evolution checks ignore Time Stranger's Agent Rank requirement up to Champion and apply it from Champion->Ultimate (rank 5), Ultimate->Mega (7) and Mega->Mega+ (8) (Q1). Revisit for balance: little to do at rank 10 once stats cap |
| Fallback | **No battling needed up to Champion** (Q9, amended by Q1b): every In-Training and Rookie form keeps an evolution that needs no battles. From Ultimate on, the rank gates make boss battles part of the way up |
| One partner | **No teams, anywhere** (Q4a): no player party and no boss teams. One partner Digimon at a time, V-Pet style; every battle is 1-on-1 |
| Eggs | After the first, each egg hatches a random In-Training I (7 in the roster), leaning toward lines with Digimon you have not raised yet; the colour-variant roll stays; the empty rare/legendary egg tiers go (Q2) |
| Wild Digimon | From the whole roster (no Armor/Hybrid), by the stage your partner's level has reached: 65% that stage, 25% one behind, 10% one ahead at your level. Mega+ appears only for a Mega+ partner; for a Mega the "ahead" roll stays Mega (Q3, Q3b, Q3c) |
| Boss ladder | One ladder of 13 single Digimon, unlocked in order: 8 area bosses (each gives a badge), 4 elites (Mega), a champion (Mega+). A mashup of the villains from the anime's first two seasons (Adventure, Adventure 02) (Q10, Q10b). Lineup (Q10e, 2026-10-28), level and stage: area 1 DemiDevimon 15 Rookie; 2 Ogremon 22, 3 Unimon 32, 4 Devimon 40 (Champions); 5 Etemon 47, 6 Myotismon 54, 7 Mummymon 58, 8 Arukenimon 60 (Ultimates); elites Machinedramon 64, Piedmon 66, BlackWarGreymon 68, MaloMyotismon 70; champion Apocalymon 80. The Ultimate tier opens right after the 4th badge gives rank 5, as Mega opens after the 6th gives rank 7. Time Stranger's own story bosses (`tools/data/time-stranger/bosses.tsv`) are not on the ladder; they could return as optional post-champion fights. To do: check the model viewer has a model for each (all 13 are in `stats.tsv`) |
| Hard mode | Easy: your partner fights at its real level. Hard: if your partner is above the boss's level it is scaled down to it, never up, so waiting cannot win a Hard badge. Hard badges are separate and optional; Agent Rank counts Easy badges only; Hard gives about double the training reward (Q6, Q6b) |
| Partner leaves | After a farewell, runaway or release the partner leaves for good: marked "raised" in the Digidex and added to a short partner history on the tamer card (name, final form, age, how it ended); then the next egg (Q8) |
| v1.0.0 saves | **Not carried forward** (Q9). The new firmware recognises a v1.0.0 save, says so, and starts a new game (the installer backs the save up first). `upgrade_test`'s frozen v1.0.0 save then checks it is detected and replaced cleanly, never misread |
| Digidex | A stage picker (In-Training I ... Mega+, with counts) replaces the version picker; inside, the existing 16-per-page grid in Time Stranger dex order (Q5, Q5b). Smooth scrolling: revisit later |
| Digidex states | Unseen (silhouette, "???"), seen (met in the wild or a boss fight: name and sprite), scanned (stats and its evolution requirements: which forms digivolve into it and the stats/rank each route needs), raised (was your partner: everything plus your record). When choosing an evolution, a branch into a scanned or raised Digimon shows its requirements, any other shows "???" (Q7, Q7b). Scanning (Q7c): no scan chips; each wild win against a Digimon adds a third of a scan, so three wild wins mark it scanned; beating a boss scans it fully. The Digidex shows partial progress ("Scan 1/3", "2/3") (Q7d). The bag keeps only recovery disks and stat chips |
| Stages | Mega+ is a stage after Mega, from 72 h (Q11). Armor, Hybrid and Jogress left out of v1 |
| Stat mapping (Q12a/b) | HP, ATK, DEF, SPD direct; INT -> Special Attack; SPI -> Special Defense; **SP dropped** |
| Stat formula (Q12e) | **Approach B**: rebuild TamaDigi's formula on Time Stranger growth. Each species grows linearly from its TS level-1 to its TS max (level 99) stats (`tools/data/time-stranger/stats.tsv`, 475 forms incl. personality), times one scale factor (~0.1), plus training. TamaDigi level = TS level, one per hour. Requirements convert with the same factor; no per-stage scaling. Battle balance must be redone |
| HP (Q12c) | **Not trainable**: grows with age only, so HP requirements act as timing gates (e.g. Agumon -> Greymon at ~hour 37, MetalGreymon -> Gaiomon ~hour 84) |
| Training (Q12d) | **Training is the bridge to late stages**: untrained growth reaches nearly all In-Training/Rookie evolutions, ~31% of Champion->Ultimate and ~6% of Ultimate->Mega. Per-session payout unchanged (up to +18). ATK training also raises Special Attack, DEF training also raises Special Defense (separate INT/SPI mini-games: next version). Examples: MetalGreymon needs ~41 ATK points (3-5 sessions), WarGreymon ~91 (6-10). **No evolution may be unreachable because of IV** (bad IV = train more): today's 70-100 IV-based cap blocked 74 of 308 Mega paths and gated WarGreymon on an ATK IV >= ~22, so the cap is now **255 for every Digimon** (the save field is a byte; the hardest Mega path needs ~192) |
| IV and training | IV scales each session's gain **70% (IV 0) to 130% (IV 31)**, so an average IV (~15) trains exactly as today. WarGreymon (~91 ATK points): ~13 typical sessions at the worst IV, 10 average, 7 best; MetalGreymon (~41): 6 / 5 / 4. Low IV also means more XP, so more battles. Idea: show it to the player ("trains fast" / "slow learner") on the status card |

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
- Open: light PARTY/EXPLORE labels at night (dark ones are hard to read).

## Firmware change list (none made yet)

- **Detect and replace v1.0.0 saves** (decided: not carried forward). The Digidex
  and the move IDs are positional in saved data, so the new firmware must
  recognise a v1.0.0 save and start fresh rather than misread it. Rewrite
  `upgrade_test`'s v1.0.0 section to check exactly that, with the decision in its
  comment; never regenerate its frozen save to make it pass.
- Remove the party and box (one partner, no teams): battles become 1-on-1.

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
- Battle timing (above).
- Public-build art.
- Model check for the 13 bosses (needs the game files in `tools/prerender/models/`).
