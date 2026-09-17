The release in five lines:

- 🎮 The PSP port now starts and races on the tested physical PSP, not just PPSSPP.
- ⚡ Hardware-driven renderer optimizations substantially improve performance; see the linked validation report.
- ⏸️ Start opens Pause; Controls stays paused, and Circle no longer interrupts a race.
- ❄️ Original snowy menus, a photo-referenced PSP-1004 guide and a genuine in-game screen capture.
- 📦 Ready-to-copy game, corresponding sources, licenses and SHA-256 checksums included.

✨ New

- FPS counter at bottom left, with a readable shadow and half-second averaging.
- Pause menu with Resume, Controls and a confirmed End race action.
- Classic snow tracks are enabled in the default PSP profile, with batched,
  view-clipped rendering that retains the original trench textures.
- Control-guide artwork generated using real original-generation PSP photographs. The screen is an unchanged capture from the physical PSP; labels and leader lines are drawn by the game.

🐛 Fixed

- Two physical-PSP FPU exceptions: a zero-scale character-model part and stationary-camera interpolation. This supersedes the hardware-incompatible v0.3.0 build.
- Redundant object draws through batching and frustum rejection, and repeated character-geometry conversion through persistent vertex buffers. Terrain mipmaps that reduced visible snow detail were rejected; original terrain detail and view distance are retained.
- Repeated terrain traversals at default detail, individual HUD digit draws and unnecessary opaque blending.
- Release notes now describe this release, not the entire import history; incomplete uploads remain private.

🧪 Validation

Physical PSP tests use 333 MHz, default detail and music enabled. Performance is course-dependent; this is not a stable-60-FPS guarantee for every course or PSP model. Exact measurements, settings and limitations are recorded in [hardware validation](https://github.com/chriopter/psp-tuxracer/blob/v0.4.0/docs/psp-hardware-validation.md).

With the restored snow trench, the short Bunny Hill hardware run measured
53.103 FPS; p95 frame time was 33.374 ms. Longer manual play showed lower
instantaneous rates. Longer benchmarks, growing track-history costs and
additional hardware validation remain [open work](https://github.com/chriopter/psp-tuxracer/blob/v0.4.0/TODO.md).

Build and regression tests cover numerics, terrain clipping/material selection, saves, music lifetime, contextual controls, paused help, GUI/HUD/object geometry, texture mipmaps and release automation. CI does not itself run physical hardware tests.

📦 Downloads

- `extremetuxracer-psp.zip` — copy `PSP/GAME/ExtremeTuxRacer` to your homebrew-enabled PSP, or open its EBOOT.PBP in PPSSPP.
- `sources.tar.gz` — corresponding game/dependency sources and build records.
- `SHA256SUMS` — checksums for both archives.
