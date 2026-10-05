# Where the art comes from

Nothing in this folder is art: the sprites are fetched and packed by scripts.

| Format | Script | Source | What |
|---|---|---|---|
| **TPK2** `pNNN.bin` / `psNNN.bin` | `pack_sprites.py` | [furudbat/wayland-vpets](https://github.com/furudbat/wayland-vpets) Digimon Color sprite sheets (© Bandai) | 9–13 colour frames per Digimon, cut from the sheet and mapped to idle, walk, sleep, eat, hurt, attack, nod and breathe. `ps` = the rare colour variant. |
| **TPTH** `thumbs.bin` | `make_thumbs.py` | derived from the TPK2 files | Digidex thumbnails |
| **TPAK** `web/sprites-<version>.pak` | `pack_bundle.py` | the files above | One bundle per V-Pet version for the web installer |

```bash
python3 tools/pack_sprites.py   # -> tools/sdcard/mons/p[s]NNN.bin + noart.h
python3 tools/make_thumbs.py    # -> tools/sdcard/mons/thumbs.bin
python3 tools/pack_bundle.py    # -> web/sprites-*.pak + web/paks.json
python3 tools/send_sd.py        # or send everything to the board's SD over USB
```

Your own art overrides the download: `tools/sprites_src/extra/<name>.png`
(black dots on transparent/white, on a 16×16 or 18×18 grid at any integer scale).
