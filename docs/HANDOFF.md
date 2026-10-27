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

- Remotes: `joop0810`, `eperdeme`, `dylan`, `socquique` (no `origin` yet).
- `joop0810-original` = the untouched single-commit version.
- socquique made 64 commits after the June 14 fork point that no fork merged
  (48 never copied either). Worth porting: **v1.18 battery work** (adaptive frame
  rate, static screens, amp gating, light sleep, LiPo curve, safe power-off) and
  **run-merged sprite drawing**; then check the Sep 7 fixes (v1.6-v1.14) one by one.

## Game design decisions

| # | Decision |
|---|---|
| Roster | Time Stranger's (~448 forms), limited to Digimon with art |
| Evolution | Time Stranger style: when a stage's timer is up, choose among branches whose conditions are met. Conditions rescaled to TamaDigi stats |
| De-digivolve | Allowed (Q5): into any form that evolves into the current one, if it is in the Digidex. The earlier form then waits out its stage again (Q8) |
| Level | Stays as age (Q4); no XP-based level |
| Bond | Sets how much training carries over on (de-)digivolution (Q6) |
| Battles | Kept (Q3). Wins earn XP; training mini-games cost XP and nothing else; Energy is only for battles. XP balance is player-wide (Q10) |
| Agent Rank | Boss-badge count mapped to ranks 1-10, player-wide (Q7). Revisit for balance: little to do at rank 10 once stats cap |
| Fallback | Every form keeps at least one evolution reachable without battling (Q9) |
| Stages | Mega+ is a stage after Mega, from 72 h (Q11). Armor, Hybrid and Jogress left out of v1 |
| Stats | **Open (Q12)**: proposed HP/ATK/DEF/SPD direct, INT->Sp.Atk, SPI->Sp.Def, SP dropped |

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
- Emulator: `EMU_EPOCH=<unix secs>` sets the start time (uncommitted change to
  `tools/emu/host_impl.cpp`).

## Still open

- Q12 stat mapping.
- How eggs/versions divide the roster.
- Battle timing (above).
- Public-build art.
- Repo visibility and first push; archiving `jaredbt/Esp-J`.
