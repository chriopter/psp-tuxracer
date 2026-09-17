# Physical PSP validation

The current measurements are from PPSSPP, not physical hardware. PSP-1000 (32 MB) and PSP-2000/3000 (64 MB) are tested as emulator configurations. Neither confirms hardware compatibility or GPU timing, Memory Stick latency, suspend/resume, or utility memory requirements on real firmware.

## Repeat the same performance workload on a PSP

1. Copy the release's `PSP/GAME/ExtremeTuxRacer` directory to the Memory Stick.
2. In that game's `config/` directory, create a text file named `benchmark` containing `3600 frozen_river` or `3600 path_of_daggers`.
3. Start the EBOOT. The test chooses the course, paddles and alternates left/right steering automatically. Native save loading/saving is disabled in benchmark mode, so the test does not overwrite a normal profile.
4. After the race finishes or pauses, quit and retrieve `config/benchmark-result.json`. Retain a copy for each course, together with PSP model, firmware, Memory Stick type and the release version.
5. Remove `config/benchmark` to return to normal play.

The game requests 333/333/166 MHz and records the reported CPU frequency. Frameskip is not implemented by the port. Results contain whole-race and steering-only presentation intervals, the CPU-side interval before swapping buffers, and sampled heap/system-memory figures. Loading and the first race frame are excluded. Heap/system-memory samples are taken every 60 frames; they do not establish transient peaks or the largest contiguous free block. The pre-swap interval is not an isolated GPU timing measurement.

The target is approximately 59.94 FPS, with occasional intervals around 33.4 ms acceptable for a 30 Hz floor. The `over35ms` count highlights larger stalls; investigate each on hardware. Also inspect tight turns, course edges, fog, trees, particles and the pause screen visually.

## Native savedata and lifecycle checks

- Start with a separate test profile; keep any existing native save backed up as a whole directory.
- Create a player with the native keyboard, select it, change an audio setting, complete a race and unlock a cup.
- Confirm the profile exists at `PSP/SAVEDATA/ETRX00001PROFILE/`, with its icon and metadata in the PSP savedata manager.
- Restart the game and verify the selected player, settings, high scores and cup progress.
- Exercise manual Save/Load and both cancellation paths; resume gameplay afterward.
- Test saving after a race, when course resources have been allocated, as well as before racing.
- Check unavailable/full Memory Stick errors without sacrificing an existing save. Deliberately damaged-data tests belong on a copy.
- Check HOME exit and suspend/resume during menus and racing. Unexpected power loss during a write has not been validated; the local working-file rollback is not a guarantee of atomic native storage.

Initial hardware feedback (exact build/model not yet confirmed): character, terrain and environment loading failed after common textures on firmware reported as 6.60 ME-1.3. A synthetic file-handle quota reproduces the same failure in the old code; the one-stream fix passes that test, but still requires an on-device retest. See [startup investigation](startup-validation.md). Passing physical performance and savedata results remain pending.
