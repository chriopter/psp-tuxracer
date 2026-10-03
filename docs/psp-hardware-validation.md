# Physical PSP validation

Two rounds: the first on 2026-09-17 (below, unchanged), the second on
2026-10-01/02 on a PSP-1000, firmware 6.61 with ARK, 333 MHz, through PSPLink
3.2.1 with the game in an isolated `host0:/etr-psp-test` directory, music and
sound on, default detail. Neither is a certification of every PSP model,
firmware or course.

## Second round — 2026-10-01/02

### What changed, and what each change bought

Bunny Hill, clear weather, 3600 measured frames, was 44 FPS at the start of
the round. All figures are frame intervals measured on the console, not
emulator numbers.

- **Snow tracks** in chunks of 64 marks with a box each; a chunk outside the
  view is skipped whole, the others are clipped once. Ring of 4096 marks.
- **Terrain** drawn by 16-bit index from a PSPGL buffer object that is also the
  course's own vertex array (one copy of the course, no per-frame copy);
  triangles wholly inside a guard band skip the clipper; visibility of
  quadtree squares is cached within the frame.
- **Trees and items** found through an index by their z instead of walking
  every object of the course each frame; the same index serves collisions.
  Tree vertices live in a buffer object, a tree in view is twelve indices.
- **The frame is shown after the next frame's computing.** The race used to
  present at the end of its pass and so waited for the GE on top of its own
  work. Now controls, physics, view, quadtree update, wind and snow run first,
  then the previous frame is presented, then the new one is drawn. A frame of
  14 ms no longer misses its vertical blank. Bunny Hill 56 → 59.6 FPS.
- **Character**: its spheres are triangle strips (4n+2 vertices a ring, not
  12n), the buffer is bound and the material set only when they change.
  1.5 → 1.1 ms a frame.
- **HUD digits** of a frame are one draw with the colour in the vertices.
- **Video memory**: PSPGL puts a texture into the GE's own memory while there
  is room and into ordinary memory after, and the GE reads the latter several
  times slower. The snow textures (flakes and curtains, 104 KB) are now among
  the ones placed first. Objects leave room for one face of the sky, which is
  loaded front face first; the finish banner, flags and herring go without.
  Tux at Home with wind and four penguins 54 → 58 FPS, Bunny Hill with five
  penguins 55 → 59. The sky as DXT1, which would fit all six faces, was tried
  and was slower (58 → 40 FPS): rejected.
- **Falling snow**: besides the textures, all flakes are one draw of GE
  sprites from a buffer filled in place (two vertices a flake, no colour of
  their own); each flake is tested only against the view planes that cut its
  area; the curtains are drawn once a frame instead of twice (their staged
  textures carry the alpha of both passes); and there are two curtains
  instead of three — the one at 60 m stood at the edge of the view in the
  fog. Bunny Hill light / medium / heavy snow: 30 / 30 / 20 → 58 / 55 / 42 FPS.
  The two-curtain change is the only one in this round that removes something
  that was drawn; before and after were compared on the console.
- **Fog** is full at the view distance, so terrain and trees come out of the
  haze instead of appearing at the clip plane.
- **Memory**: the preview pictures of the courses are read when the race
  selection shows them and freed when a course loads. Heap peak on Bunny Hill
  13.7 → 11.9 MB of 15.3; the largest course measured (Explore Mountains with
  five penguins) peaks at 14.3 MB.

- **Loading**, measured from the Memory Stick with time-stamped steps
  (`STEP` lines in `etr-errors.log` of a benchmark run): from the program's
  start to the first menu 15.4 → 6.1 s, to the start of a race on Bunny Hill
  through the menus 24.7 → 12.6 s.
  - Both logs were unbuffered files on the Memory Stick, five hundred lines
    and as many writes before a race began. A normal start now writes no
    log at all; `config/trace` or a benchmark turns them on.
  - Every pixel of every texture went through a call into SDL and two
    divisions; the conversion took as long as decoding the PNG. Now a table
    of columns and direct byte reads.
  - Three fonts of 224 glyphs each were drawn at the start, one is used:
    a font is drawn when first written with.
  - Player pictures, character shapes, keyframes and previews are read when
    first needed; every `course.dim` of a group is gathered into one file at
    staging; the sounds are staged in the mixer's own 22050 Hz.

Tried and removed: drawing the sky behind the terrain by depth (PSPGL's depth
range made it flicker), and DXT1 for the sky (above).

### Computer penguins

A race can be run against one to five computer penguins (race and event
selection, one row: none or one to five). They cost 0.2 ms of CPU a frame for
five (each is one draw of a mesh made once) and the GE's time for about 2500
vertices each. See `src/opponents.h` for how they drive;
`tools/test-psp-opponents.py` runs their logic on the host.

Changed on 2026-10-03:

- **Collision box** drawn on the console (`config/debug-collision`) was
  0.95 × 1.5 m against a body of about 0.6 × 1.0 m: one ran into nothing that
  could be seen. Now 0.6 × 1.05 m, and running into one brakes over a few
  frames instead of in one.
- **Collision by the body's own size** (2026-10-04): one box for all was
  the fault behind both complaints, penguins driving into each other and a
  gap one could see. The bodies are measured from the spheres they are made
  of (Tux 0.78 m wide, 0.67 m ahead and 0.53 m behind where he is put; another
  character 0.77, 0.72, 0.70), and two racers stop where those meet, 82 % of
  the widths across and 90 % of the lengths along, a body being round. Side
  by side they are put apart at once; in line the one behind stays behind.
  Checked on the console with `config/debug-collision-test`, which brings a
  penguin up to the player by 0.2 m every three quarters of a second from
  the right, the left and ahead: at rest the flipper tips meet.
- **Pace**: they took their speed from the player's, so a clean run always
  won. Now each drives by the slope, the ground's friction and the air, as the
  player does, scaled by what the player makes of a slope (learned over the
  session) and by its skill: the best a little quicker than the player's
  usual, one their equal, the rest behind. Only beyond 15 m are they held back
  or pressed on. Against the self-driving player on Bunny Hill, Twisty Slope
  and Frozen River the places came out 2nd to 6th.
- **Result screen**: under the figures, every racer's time in order, the
  player's row in red; a penguin still on the course is given the time it
  would come in at.

### Results

Soak on the console, 669 tests so far of a planned 1020 (it was still
running when this was written): each a fresh start of the game and an
action — 373 measured races with a random course, light, snow, wind,
mirror and zero to five penguins; 101 self-driven races towards the finish
line; 195 walks through the menus by key presses (player, race and event
selection into a race, configuration, scores, help, credits, pause, end
race, reset). Eight were counted as failed. Six were the test harness (a
pause or result screen that stands still by design, a wait too short for a
snow race); one was a leak in the test-only sound log, fixed. One is
unexplained: test 583, a self-driven race on Path of Daggers at night with
medium snow, strong wind, mirrored, no penguins — the console stopped
answering, PSPLink included. The same configuration then ran five times
without fault.

Measured races since the last change to the renderer (173 of them):

| Weather | Races | Slowest | Mean |
|---|---|---|---|
| clear | 79 | 58.2 FPS | 59.8 FPS |
| light snow | 29 | 43.2 | 56.3 |
| medium snow | 32 | 38.3 | 54.1 |
| heavy snow | 33 | 31.3 | 41.1 |

Largest heap peak in any of them: 12.5 MB of 15.3.

### Found and fixed in this round

- The native save did not work on the console at all: the dialog needs the
  parameter block of firmware 2.00 and later and a key (error `0x80110388`).
  After a `glClear` the GE was still in clear mode, and the system dialogs
  drew white boxes. Save and load verified through the PSP's own dialogs.
- The clock read `00:00.00` at 0.996 s and a second too little at 59.999 s:
  minutes, seconds and hundredths were rounded apart.
- `GetHeight` clamped the wrong variable at the far edge of a course.
- Pressing Triangle (back onto the course) would have lined the penguins up
  beside the player again; they start with the race only.
- The result screen's German label for the average speed ran into its number.
- Race selection, controls picture, pause menu and the questions before
  leaving were partly English in every language; all text on screen is now in
  the twelve translation files.

### Limits

- One freeze in the soak is unexplained (see Results).
- Heavy snow does not reach 60 FPS (42 on Bunny Hill); with penguins and wind
  on top it is lower. Courses with long views over ice (Tux at Home, Path of
  Daggers) are the slowest in clear weather.
- The penguins are not simulated as the player is: they follow the ground and
  do not jump; their pace is tuned against the self-driving player only.
- Not tested: a full or missing Memory Stick during a save, suspend and
  resume during a race, PSP-2000/3000/Go.

# First round — 2026-09-17

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
