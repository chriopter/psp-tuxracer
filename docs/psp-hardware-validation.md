# Physical PSP validation — 2026-09-17

Tests run on the user's USB-connected physical PSP via PSPLink 3.2.1,
firmware reported as `0x06060010`, CPU 333 MHz. The game runs from an isolated
`host0:/etr-psp-test` directory, with staged PSP assets, detail level 1,
music enabled and scripted steering. This is not an assertion that every PSP
model, custom firmware or course has been certified.

## Reproduce the measurements

Build with `./build-extremetuxracer.sh`, stage the data with
`python3 tools/stage-extremetuxracer.py`, and place the PRX beside that data in
an isolated PSPLink host directory. Its `config/benchmark` contains a frame
count and course directory, for example `1800 bunny_hill`. Start the PRX with
that directory as its working directory. The benchmark automatically enters
the race, applies repeatable steering and pauses when complete.

Results are written to `config/benchmark-result.json`, chronological intervals
to `config/frame-times-us.json`, and CPU section means to `config/profile.json`.
Do not capture screenshots or send input during timing. `config/profile-sync`
enables deliberately serial GPU diagnostics, **not** representative FPS; it
must be absent for performance runs. Remove `config/benchmark` for normal play.
Normal gameplay performs no benchmark writes. The final measurement code also
excludes the partial interval immediately after its race-start marker write.

## Hardware-only failures found and fixed

- Loading Samuel: FPU exception at `CCharShape::ScaleNode`, original model's
  whisker scale `-0.00` caused division by zero. Retain signed minimal thickness
  and a finite, matching inverse transform. Regression tests cover both signed
  zeros, tiny and ordinary positive/negative scales.
- Starting a course: FPU exception at `update_view`, stationary camera
  interpolation divided by zero. Bound its denominator away from zero.
  Regression tests cover stationary, threshold and high speeds.

The pre-fix `v0.3.0` release is marked as unsuitable for physical hardware.

## Optimizations measured on hardware

**Development results, not release acceptance:** the first mipmapped builds
showed dark terrain bands on the physical display, independently confirmed by
the user. Disabling terrain mipmaps restored correct terrain at roughly
30–37 FPS. High FPS from those earlier builds is not a graphics-correctness
claim. An aligned linear upload removed the observed bands, but automatic
terrain mip selection still visibly flattened snow detail. The user rejected
that fidelity loss. Terrain mipmaps are therefore disabled; the reference
settings remain forward distance 60 and course detail 20. The experimental
50/10 settings are not shipping defaults. Further performance work must
preserve this accepted terrain appearance, not just reach a target FPS.

- Persistent native-layout vertex buffers reuse character sphere geometry.
- Cached terrain outcodes and compact clipped vertex streams retain all six
  clipping planes and interpolated attributes.
- Opaque default-detail terrain uses one quadtree traversal with material
  buckets. All 512 three-corner terrain combinations are compared against the
  old per-material selection in the regression test.
- Object batches retain original tree/item positions, normals, UVs and order.
- Native-layout HUD batches retain the original numerical atlas.
- Object mipmaps retain the full-resolution base image. Terrain mipmaps are
  disabled to retain the accepted snow detail. Conservative object bounds
  reject off-screen trees/items.
- Opaque sky/character/default terrain avoids unnecessary alpha blending.
- Early command submission overlaps CPU geometry preparation with GE rendering.
- Bounded texture decoding selects exactly the same source pixels as the old
  full-image conversion followed by upload resampling, while avoiding the
  second full-resolution RGBA allocation. Full-size non-texture image loads
  and logical sprite dimensions are unchanged and covered by host tests.

Quality-preserving VRAM experiment (Bunny Hill, 598 measured intervals,
333 MHz, forward distance 60, course detail 20, music active throughout):
the RAM-texture baseline measured **32.176 FPS**, while reserving VRAM for
world textures measured **55.486 FPS**. The latter had a 16.684 ms median,
33.371 ms p95 and 33.814 ms maximum interval. Neither used terrain mipmaps.
These short frame-count-based runs are not a fixed-time trajectory comparison
or a stable-60 claim. Raw development captures remain outside release assets.

The user subsequently requested the classic snow trench as part of the visual
baseline. It had previously been disabled by the original detail-level gate.
Restoring the original immediate-mode track renderer measured **33.281 FPS**
on the same 598-interval Bunny Hill test. Current work batches the original
track geometry and clips it against the view, rather than removing the trench.
The earlier 55.486 FPS result does **not** include snow tracks.

With batched tracks the same short hardware test measured **45.831 FPS**.
Early whole-quad rejection and the three original 64×64 trench textures in
VRAM raised that to **53.103 FPS** (598 intervals; median 16.684 ms,
p95 33.374 ms, maximum 35.343 ms; music active throughout). Original terrain
detail, view distance and texture sampling remain unchanged. This is still
not locked 60 FPS; longer manual play also showed lower instantaneous rates.
The [physical-PSP snow-trench capture](images/psp-hardware-snow-trench.png)
shows the effect during ordinary play, not an emulator-only result.

Initial Frozen River measurement: **29.970 FPS**, 599 frame intervals,
20.451 ms median CPU work. After the first optimizations and mipmaps:
**59.940 FPS**, 599 intervals, p95 17.384 ms, maximum 19.050 ms,
no frames over 35 ms; music active for all 599 intervals.

Longer Chinese Wall measurement improved from **46.339 FPS** to **59.741 FPS**
over 1,799 intervals after object batching, early submission and the one-pass
terrain path. These development measurements still included periodic USB
profiling writes. Final testing is tracked separately; do not treat development
results as a blanket stable-60 guarantee.

## Controls and graphics

The FPS counter is at bottom left. Start opens Pause while racing; Circle has
no racing action. Pause contains Resume, Controls and End race. Controls stays
within the paused state; closing it returns to Pause, not Racing. End race
still requires confirmation.

The guide uses photo references of an actual PSP-1004 and original-generation
PSP, with native labels. The screen image is an unchanged physical-PSP game
capture, composited by the game, not generated scenery. References, licenses
and the full image-model prompt are in [the artwork record](artwork/psp-guide.md).

Hardware screenshots: [controls](images/psp-hardware-controls.png) and
[pause menu](images/psp-hardware-pause.png). The controls capture includes the
final photo-referenced revision running on the physical PSP.
