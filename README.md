# Extreme Tux Racer for PSP

An unofficial PSP port of [Extreme Tux Racer](https://sourceforge.net/projects/extremetuxracer/), based on version 0.8.4. Race Tux down snowy mountains, collect fish, and beat the clock.

<img width="480" alt="Extreme Tux Racer on a real PSP-1000: races against penguins, snow at night, then a wall of PSPs for the more than 1,000 test runs" src="docs/images/etr-psp-launch.webp" />

[▶ The launch video in full quality](https://github.com/chriopter/psp-tuxracer/releases/download/v1.0.0/etr-psp-launch.mp4)

Tested more than 1,000 times on a real PSP-1000 at 333 MHz, and in PPSSPP.
Performance depends on the course and the weather; see the
[hardware measurements and limitations](docs/psp-hardware-validation.md).

## Play

Download the game ZIP from the [latest release](https://github.com/chriopter/psp-tuxracer/releases/latest). Extract it and copy `PSP/GAME/ExtremeTuxRacer/` to your PSP's memory stick, or open its `EBOOT.PBP` in PPSSPP. Keep `data/` and `config/` alongside the executable.

| Control | Action |
| --- | --- |
| D-pad / analog stick | Steer and navigate |
| Up / R | Paddle |
| Down / L | Brake |
| Cross | Hold and release to jump / confirm |
| Circle | Back in menus / resume from pause (no action while racing) |
| Square + direction | Trick |
| Triangle | Reset to the course |
| Start | Pause / resume |

The first visit to the main menu is preceded by an illustrated PSP control guide.
Press **Start** to continue; **Help** reopens the guide later.
During a race, press **Start**, then select **Controls**. The race stays paused
while viewing the guide; closing it returns to the pause menu.
Start, Square, Triangle and the shoulder buttons do not activate normal menu items.
To leave a race, pause, select **End race**, then confirm with Cross (Circle cancels).
Quitting the game also requires confirmation. The original snowy main menu is retained.

![Illustrated PSP controls introduction](docs/images/psp-controls-intro.png)

![Original snowy main menu](docs/images/psp-main-menu.png)

See the [screenshot and performance checks](docs/psp-ui-validation.md), [PSP illustration and prompt](docs/artwork/psp-guide.md), and [button artwork](docs/artwork/psp-controls.md).

Progress and settings save automatically. The **Saved data** menu opens the PSP Save/Load dialogs.

## Build

Requires Docker, Python 3, FFmpeg, ImageMagick, and PPSSPP (`PPSSPPSDL`).

```sh
./build-extremetuxracer.sh
./start-extremetuxracer.sh
```

The build uses a pinned PSP SDK container. The launcher prepares the complete game data automatically after a rebuild, preserving saves and settings. Set `PPSSPP_BIN` if your emulator is installed elsewhere.

## Development

The history has three steps: import original 0.8.4, move it into `ports/extremetuxracer/`, then port it to PSP. See [source provenance and updates](docs/upstream.md) and [porting notes](docs/porting.md).

[Build and test details](docs/development.md) · [Hardware testing](docs/hardware-validation.md) · [Release packaging](docs/binary-distribution.md)

## Credits and license

Original game by the [Extreme Tux Racer team](ports/extremetuxracer/AUTHORS) and its contributors. This port is not affiliated with the original team or Sony.

Game code and assets: **GPL-2.0-or-later**, with a separate permissive license for the quadtree implementation. See [LICENSE](LICENSE), [license details](docs/licensing.md), and [source provenance](docs/upstream.json).
