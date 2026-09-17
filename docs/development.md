# Development

## Build and test

The PSP SDK image is pinned in `ports/extremetuxracer/psp/Dockerfile`. Building requires Docker; the host tests also require Python 3 and a C++ compiler.

```sh
python3 tools/test-etr-numerics.py
python3 tools/test-etr-terrain.py
python3 tools/test-psp-save.py
python3 tools/test-psp-music.py
python3 tools/test-release.py
./build-extremetuxracer.sh
```

Run `./start-extremetuxracer.sh` after building: it stages new builds automatically (FFmpeg and ImageMagick required). To refresh only the data, run `python3 tools/stage-extremetuxracer.py` explicitly. Do not launch the bare build EBOOT from `ports/extremetuxracer/psp/`: it has no adjacent game data. The staged game lives at `state/extremetuxracer/config/ppsspp/PSP/GAME/ExtremeTuxRacer/`. The first staging run creates an isolated PPSSPP profile at 333 MHz, native resolution, and no frameskip; existing settings are preserved.

With Xvfb installed, `./start-extremetuxracer-background.sh` runs on a hidden display. Background audio is muted unless `TUXRACER_BACKGROUND_AUDIO=1` is set. Set `PPSSPP_BIN` to choose an emulator executable. Close other PPSSPP instances if the desktop launcher is silent.

## Saves and controls

The native save is `PSP/SAVEDATA/ETRX00001PROFILE/`. Back up the entire directory. The main menu's **Saved data** entry opens the system dialogs. To edit a player name, select its field and press Cross; Start finishes typing. A damaged or unsupported save disables autosave until it is explicitly replaced or a backup is restored.

Default desktop controls in the supplied PPSSPP profile: arrows / IJKL to steer, Up / W to paddle, Down / Q to brake, Space for Cross, Backspace for Circle, A for Square, R for Triangle, Enter for Start.

## Validation

See [startup validation](startup-validation.md), [raw benchmark results](benchmarks/), and the [hardware test procedure](hardware-validation.md). Recorded emulator frame rates do not establish physical PSP performance or compatibility.

Graphics validation (2026-09-17, PPSSPP 1.20.4): all 22 standard courses passed 600-frame scripted starts with steering and music at 59.837–59.940 FPS in the default detail-1 profile. A 7,200-frame Chinese Wall run averaged 59.791 FPS. Race screenshots were inspected for missing terrain/skybox faces and HUD corruption; mirrored races, snowfall, wind, night lighting, jump, pause and reset were also checked manually. This is coverage of the tested scenes, not a guarantee for every camera position or physical PSP hardware. [Exact build hash and measurements](benchmarks/graphics-2026-09-17.json).

To run a benchmark, put a line such as `7200 frozen_river` in the staged game's `config/benchmark`, then launch. Results appear in `config/benchmark-result.json`. Remove the trigger file to play normally. Music and sound should remain enabled for comparable measurements.

## Releases

Successful builds on `main`, including manual workflow runs, publish the next `v0.x.0` release. Pull requests build test artifacts without publishing. Releases include the game ZIP, corresponding sources, and checksums; incomplete uploads remain drafts and can be resumed. CI artifacts are retained for 30 days.

See [binary distribution](binary-distribution.md) for packaging and dependency sources. The clean three-commit history starts a new release series at `v0.1.0`; its initial notes use the original-source import as their baseline.
