# Handoff: custom TamaDigi (Time Stranger roster)

Status as of 2026-10-27. Supersedes the original notes on the `claude/handoff`
branch of `jaredbt/Esp-J`.

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

## Game design decisions

| # | Decision |
|---|---|
| Roster | Time Stranger's (~448 forms), limited to Digimon with art. **One single roster**: no "versions" (joop0810's term is dropped). The first egg hatches a random In-Training I; de-digivolution then makes every Digimon reachable in principle. Open: later eggs, wild-encounter pools and boss ladders without versions |
| Evolution | Time Stranger style: choose among branches whose conditions are met. TamaDigi's stage timers (6/24/48/60/72 h) are **minimums**; evolution is offered, never forced ("keep form" re-offers next level) |
| De-digivolve | Allowed (Q5): into any form that evolves into the current one, if it is in the Digidex. The earlier form then waits out its stage again (Q8) |
| Level | Stays as age (Q4); no XP-based level |
| Bond | Sets how much training carries over on (de-)digivolution (Q6) |
| Battles | Kept (Q3). Wins earn XP; training mini-games cost XP and nothing else; Energy is only for battles. XP balance is player-wide (Q10) |
| Agent Rank | Boss-badge count mapped to ranks 1-10, player-wide (Q7). Revisit for balance: little to do at rank 10 once stats cap |
| Fallback | Every form keeps at least one evolution reachable without battling (Q9) |
| Stages | Mega+ is a stage after Mega, from 72 h (Q11). Armor, Hybrid and Jogress left out of v1 |
| Stat mapping (Q12a/b) | HP, ATK, DEF, SPD direct; INT -> Special Attack; SPI -> Special Defense; **SP dropped** |
| Stat formula (Q12e) | **Approach B**: rebuild TamaDigi's formula on Time Stranger growth. Each species grows linearly from its TS level-1 to its TS max (level 99) stats (Grindosaur data, 475 forms incl. personality), times one scale factor (~0.1), plus training. TamaDigi level = TS level, one per hour. Requirements convert with the same factor; no per-stage scaling. Battle balance must be redone |
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
- Emulator: `EMU_EPOCH=<unix secs>` sets the start time (`tools/emu/host_impl.cpp`).
- The full test suite (`tools/emu/tests/run.sh`) needs `arduino-cli` with the ESP32
  core and the `esp-emu` native emulator; neither is installed on this machine yet.
- All comments, logs, scripts and docs are in English (see CLAUDE.md); only the UI
  string table keeps its six languages.

## Still open

- How evolution is offered, and how the 72-hour farewell is offered (revisit later).
- Without versions: later eggs, wild-encounter pools, boss ladders, badges -> Agent Rank.
- Battle timing (above).
- Public-build art.
