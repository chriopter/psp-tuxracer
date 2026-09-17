# Remaining work

## Hardware performance

- [ ] Run longer, repeatable physical-PSP benchmarks on multiple courses with music enabled.
- [ ] Record frame-time distributions and investigate frames exceeding the 16.7 ms budget, rather than relying on average FPS alone.
- [ ] Measure the CPU and memory cost of snow-track history as a race progresses, including the full track-buffer case.
- [ ] Profile terrain preparation, track rendering, texture access and GPU synchronization before selecting further optimizations.
- [ ] Compare equivalent routes and elapsed gameplay time when evaluating performance changes.

## Visual and functional validation

- [ ] Preserve the current terrain texture detail, view distance and classic snow trench during optimization.
- [ ] Compare screenshots and moving gameplay on physical hardware; reject changes that introduce terrain artifacts or reduce visible detail.
- [ ] Repeat multi-course and endurance checks with the final renderer, including pause/help, restart, course changes and native saves.
- [ ] Document tested hardware and settings, frame-time results and remaining limitations for each release.

## Current baseline

The short Bunny Hill hardware test with snow tracks measured 53.103 FPS over
598 frame intervals at 333 MHz, with music enabled, forward distance 60 and
course detail 20. The p95 frame interval was 33.374 ms. Longer manual play
showed lower instantaneous frame rates. Stable 60 FPS on physical hardware
has not been established.

See [hardware validation](docs/psp-hardware-validation.md) for measurements
and the distinction between accepted rendering and rejected visual tradeoffs.
