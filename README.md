# TamaDigi

![Firmware](https://img.shields.io/badge/firmware-v1.0.0-8A2BE2) ![Board](https://img.shields.io/badge/board-ESP32--S3%20round%20AMOLED-E7352C) ![Code](https://img.shields.io/badge/code-MIT-blue)

A **Digimon V-Pet** for the **Waveshare ESP32-S3-Touch-AMOLED-1.75**: a round 466×466 AMOLED with touch, battery and speaker.
You raise a Digimon from a **Digi-Egg**. How well you care for and train it, and how many battles it wins, decides **which Digimon it digivolves into**.

> **Origin:** The project is built on the TamaPoke engine (original by **Quique Tortosa** – [socquique/TamaPoke](https://github.com/socquique/TamaPoke), forked by [DylanPDao](https://github.com/DylanPDao/TamaPoke) and [eperdeme](https://github.com/eperdeme/TamaPoke)). The display, touch, sound, save files, battle system, emulator and web installer come from there. TamaDigi replaces the Pokémon content entirely with Digimon content and Digimon rules.
>
> **Non-commercial fan project.** The code is MIT licensed. Digimon, all names and the pixel art (from *Digimon Color*) are © Bandai / Toei Animation. See [CREDITS.md](CREDITS.md).

## Screens

Every image comes straight from the emulator (`bash tools/make_screens.sh`) and shows exactly what the display draws.

| Your Digimon | Digi-Egg | Choose a version |
|---|---|---|
| <img src="docs/screens/main.png" width="220"> | <img src="docs/screens/egg.png" width="220"> | <img src="docs/screens/region.png" width="220"> |

| Battle | Choose an attack | Wild Digimon | Explore |
|---|---|---|---|
| <img src="docs/screens/btlmenu.png" width="200"> | <img src="docs/screens/btlmoves.png" width="200"> | <img src="docs/screens/wild.png" width="200"> | <img src="docs/screens/explore.png" width="200"> |

| Boss ladders | Ver.2 bosses | Victory | Choose a party |
|---|---|---|---|
| <img src="docs/screens/gympick.png" width="200"> | <img src="docs/screens/gymsj.png" width="200"> | <img src="docs/screens/win.png" width="200"> | <img src="docs/screens/pick.png" width="200"> |

| Digidex | Ver.1 | Ver.2 | Box |
|---|---|---|---|
| <img src="docs/screens/dexpick.png" width="200"> | <img src="docs/screens/gallery.png" width="200"> | <img src="docs/screens/gallery2.png" width="200"> | <img src="docs/screens/box.png" width="200"> |

| Tamer card | Badges | Attacks | LAN battle |
|---|---|---|---|
| <img src="docs/screens/player.png" width="200"> | <img src="docs/screens/player2.png" width="200"> | <img src="docs/screens/moves.png" width="200"> | <img src="docs/screens/lanready.png" width="200"> |

## How it differs from Pokémon

| | TamaPoke | **TamaDigi** |
|---|---|---|
| Growth | Evolution by level, all stats ≥ 40 | **Digivolution** by age; *care* picks the branch |
| Stages | Basic → Stage 1 → Stage 2 | **Digi-Egg → Baby I → Baby II → Rookie → Champion → Ultimate → Mega** |
| Types | 18 Pokémon types | **9 elements** + the **attribute triangle** Vaccine > Virus > Data > Vaccine |
| Regions | Kanto, Johto … | **V-Pet versions**: Ver.1 to Ver.5 |
| Catching | Poké Balls | **Scan chips** (Scan, Super, Hyper, Master) |
| Healing/training | Potions, Protein … | **Recovery disks** (S/M/L), Full Recovery disk, Attack/Defence/Speed chip |
| Food | Berries, candy | **Meat, fish, mushroom** (every Digimon has a favourite food), **protein** |
| Gyms | Gym leaders + badges | **Boss ladders** per version + their own badges |
| Shiny | Shiny Pokémon | a rare **colour variant** |
| Art/music | Nintendo material | coloured Digimon Color sprites; the music, badges, avatars and backgrounds are **original work** |

## Digivolution – how it works

One level equals one hour of real time. When a stage's time is up, the **digivolution button** appears. Tap it to watch the digivolution; "Keep form" postpones it. **Which** Digimon comes out is decided by the first matching branch:

| Stage | Digivolves from | What decides the branch |
|---|---|---|
| Baby I → Baby II | Lv. 2 (1 h) | – |
| Baby II → Rookie | Lv. 7 (6 h) | **Care mistakes:** 0–2 → the first Rookie, otherwise the second |
| Rookie → Champion | Lv. 25 (24 h) | **Care mistakes** (0–2 / 3–5 / more) × **training** (≥ 40 / less) |
| Champion → Ultimate | Lv. 49 (48 h) | **5 battles won** in this stage |
| Ultimate → Mega | Lv. 61 (60 h) | **15 battles won** in this stage |

- **Care mistake**: a need (food, joy, energy, hygiene) drops to 10 or below (at most one per hour).
- **Training**: the sum of strength, defence and speed (punching bag, reaction test, ball game, victories, chips).
- Care mistakes and victories **start counting again after every digivolution**, as on the real V-Pet.
- A Champion with no victories stays a Champion. It still reaches the end of its life: after 3 days any Digimon from Champion up can **say goodbye**, and it then joins the party.
- The **PROGRESS** card (swipe up on the status card) shows care mistakes, training, the hours until digivolution and the victories still missing.

Example, Ver.1 – **Agumon** becomes:

| | Training ≥ 40 | Training < 40 |
|---|---|---|
| **0–2 care mistakes** | Greymon | Tyrannomon |
| **3–5 care mistakes** | Devimon | Meramon |
| **more** | Numemon | Numemon |

With 5 victories, Greymon, Devimon and Airdramon become **MetalGreymon**; Tyrannomon, Meramon and Seadramon become **Mamemon**; Numemon becomes **Monzaemon**. With 15 more victories these become the Megas **BlitzGreymon**, **BanchoMamemon** and **ShinMonzaemon**.

## The versions

| Version | Digi-Egg | Digimon | Megas |
|---|---|---|---|
| **Ver.1** | Botamon | 17 | BlitzGreymon, BanchoMamemon, ShinMonzaemon |
| **Ver.2** | Punimon | 17 | SkullMammon, CresGarurumon, Ebemon |
| **Ver.3** | Poyomon | 19 | HiAndromon, Gokumon, BanchoLeomon, **Millenniumon** |
| **Ver.4** | Yuramon | 17 | Darkdramon, BloomLordmon, Gankoomon |
| **Ver.5** | Zurumon | 17 | Machinedramon, Raidenmon, Gaioumon |
| **ALL** | one of the five | mixed | |

That makes **87 Digimon** in all, each with its own coloured art. The trees follow the V-Pets and *Digimon Color*. The branch conditions are adapted to this game's one-level-per-hour pace. It is all in one file: [`tools/digimon_data.py`](tools/digimon_data.py).

**Ver.3 special route:** with 10 victories Sukamon becomes **Chimairamon**, otherwise with 5 it becomes Etemon. With 20 victories Chimairamon becomes **Millenniumon**, with 15 BanchoLeomon.

## Battle

- Turn-based with 4 attacks. Signature attacks come with digivolution (Pepper Breath, Mega Flame, Giga Destroyer …).
- **Element table** (Fire, Water, Plant, Electric, Wind, Earth, Light, Dark, Neutral) with 2×/½× and no immunities.
- **Attribute triangle:** Vaccine beats Virus, Virus beats Data, Data beats Vaccine (×1.3 or ×0.8). Babies are *Free* and neutral.
- **Explore:** wild Digimon matched to your level (level 10 → Rookies, from level 25 Champions too). Weaken them and use a **scan chip** to take their data into your party or box.
- **Boss ladders:** per version, 8 areas, 4 elite bosses and a champion, on Easy and Hard. Victories train your Digimon and count toward digivolution.
- **LAN battle** between two devices over ESP-NOW.

## Hardware

- [Waveshare ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75): the **standard** or **-G** version, **not "-B"**
- 466×466 AMOLED (CO5300, QSPI), CST9217 touch, AXP2101 (battery/PWR), PCF85063 (RTC), microSD, ES8311 (sound)
- A microSD card in the slot (the sprites live there)

## Installation

### Option A: web installer (recommended, nothing to install)

1. Open **https://joop0810.github.io/TamaDigi/web/** in **Chrome or Edge** (Firefox and Safari cannot do Web Serial).
2. Plug the board in over USB-C, click **Install firmware** and choose the port.
3. Further down, send the **version packs** (sprites) to the SD card: Ver.1–Ver.5, one at a time or all of them.
4. Done. To change the language on the device: tap the name → SETTINGS → tap the language row (until it shows the one you want).

**Your own fork:** fork the repo, replace `joop0810` with your GitHub name in `web/editions.json`, `web/index.html` and `web/installer.js`, choose "GitHub Actions" under **Settings → Pages → Source**, and push to `main`. The workflow compiles the firmware in the cloud and publishes your own installer.

### Option A2: `start.bat` (Windows, local)

Double-click **`start.bat`** in the TamaDigi folder. The script

- downloads the firmware GitHub built most recently from the Pages site, or
- takes it from **`tamadigi-firmware.zip`** if there is no Pages site (for example a private fork). You get that file under **GitHub → Actions → latest run → Artifacts → tamadigi-firmware**. Leave it in your Downloads folder or put it next to `start.bat`.
- Then it starts a small local server and opens the installer at `http://localhost:8765/web/` in Chrome or Edge.

Everything else (flashing the firmware, sprites to the SD, backing up the save) works there as in option A. You need neither Python nor Arduino for this, only the PowerShell that comes with Windows. Close the black window to stop it.

### Option B: Arduino, locally

- Board: **ESP32S3 Dev Module** · Flash **16MB** · PSRAM **OPI PSRAM** · Partition Scheme **16M Flash (3MB APP/9MB FATFS)** · USB CDC On Boot **Enabled**
- Libraries: GFX Library for Arduino 1.6.7, SensorLib 0.4.1, XPowersLib 0.3.3, ESP32 core 3.3.11

```bash
FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB"
arduino-cli compile --fqbn "$FQBN" .
arduino-cli upload -p COM5 --fqbn "$FQBN" .
python3 tools/unpack_bundle.py   # unpack the sprites from web/sprites-*.pak
python3 tools/send_sd.py         # and send them to the SD over USB
```

## Sprites

The Digimon are the **coloured pixel sprites from *Digimon Color*** (Bandai), as collected by the [furudbat/wayland-vpets](https://github.com/furudbat/wayland-vpets) project (`assets/dmc/`). Each Digimon there has a sheet of 9–13 images: standing, happy, angry, eating, sleeping, refusing, sad, defeated and attacking. The animations (walking, eating, sleeping, hit, attack, nodding) are built from those. A light rim keeps the figure visible against the dark sky at night too. The rare **colour variant** is the same art with a shifted hue.

```bash
python3 tools/pack_sprites.py   # fetches the source images (once), produces tools/sdcard/mons/*.bin + noart.h
python3 tools/make_thumbs.py    # Digidex thumbnails
python3 tools/pack_bundle.py    # web/sprites-<version>.pak for the installer
```

The finished packs are already in `web/sprites-ver1.pak` … `ver5.pak`. You only need to run these commands if you add Digimon or change the art. A Digimon without art goes into `noart.h`; it then never hatches and is never a digivolution target.

## How to play

- **Bottom**: 🍖 Feed (meat/fish/mushroom or protein) · 🌙 Light/sleep · 🫧 Bath · 🏋 Training (punching bag → strength, reaction → speed, ball → defence, memory game → joy)
- **Swipe**: horizontally through `TAMER · PARTY · [DIGIMON] · EXPLORE · BOSSES · DIGIDEX`, up = status card, down = back, drag at the edge = turn pages
- **Tap the name** = menu (bag, settings, say goodbye); **tap the Digimon** = pet it; **hold for 3 s** = release it
- **At night** (0–6 h, screen off) your Digimon falls asleep on its own so it does not run away overnight. This needs the time to be set in the settings.
- **PWR short press** = screen on/off, **long press (4 s)** = power off (the clock keeps running, so your Digimon ages while switched off too)
- Language: English (default), German, Spanish, French, Italian, Portuguese – in the settings

## Try it on a PC: the emulator

`tools/emu/` compiles the **real firmware** into a desktop program; only the hardware is simulated.

```bash
sudo apt install libsdl2-dev          # macOS: brew install sdl2
python3 tools/unpack_bundle.py
bash tools/emu/build.sh
tools/emu/tamadigi-emu --scale 2 --fast 60   # 60x time-lapse: a whole life in ~1 h
```

## Tests

```bash
TAMA_TEST_BACKEND=host bash tools/emu/tests/run.sh           # every suite in the emulator
TAMA_TEST_BACKEND=host bash tools/emu/tests/run.sh digivolve # just one
```

48 suites, including `digivolve_test` (every Ver.1 branch, victories, the stage clock, Megas in every version, no branch to a Digimon without a sprite), battle, saving through a power cut, the web installer and touch. GitHub Actions runs them on every push and also compiles the real ESP32 firmware.

## Serial console (115200)

`STATS` · `SPEC <dex>` · `LVL <n>` · `MISS <n>` (care mistakes) · `TR <atk> <def> <spe>` · `HATCH` · `EGG <dex>` · `VARIANT` · `WILD` · `BATTLE <dex> <lvl>` · `BYE` · `RUN` · `WIPE` · `EXPORT` / `IMPORT` (back up the save) · `LS` / `PUT` (SD files)

## Licence

- **Code** (firmware + tools): MIT, © 2026 Quique Tortosa and contributors, Digimon conversion © 2026 Dennis.
- **Digimon, names, sprites**: © Bandai / Toei Animation. Private, non-commercial use only.
- **Music, badges, avatars, battle backgrounds, UI icons**: made in this project (see `tools/gen_music.py`, `gen_ladders.py`, `gen_avatars.py`, `draw_backs.py`, `sprites.py`), MIT.

Unofficial fan project. Not affiliated with or endorsed by Bandai or Toei.
