# PSP menu and controls validation — 2026-09-17

This record covers the hardware-port revision after `v0.3.0`. Earlier emulator
measurements remain in the historical benchmark files. Physical-hardware
results and limitations are recorded [separately](psp-hardware-validation.md).

## Presentation and input

- Main menu: restored upstream layout, large original logo, ice-corner artwork and falling snow. Snow uses a single shared atlas batch rather than hundreds of individual sprite draws.
- After player selection, the first visit to the main menu is preceded by an illustrated PSP control guide. Only Start continues; ordinary direction/action buttons do not skip it. Help reopens the same diagram; Start, Cross or Circle return from Help.
- The diagram uses a 512 × 256 RGB565 background (256 KiB), generated with genuine PSP-1004/original-generation PSP photographs as references. A separate, unchanged physical-PSP gameplay capture fills the display. Labels and leader lines are drawn at native resolution. The button atlas remains 256 × 128 (64 KiB). Artwork, photo attribution, generation prompts and preparation commands are recorded in [the diagram record](artwork/psp-guide.md) and [the button record](artwork/psp-controls.md).
- Cross confirms; Circle goes back. Main-menu Circle opens player selection. Player selection and the Quit item both require explicit confirmation before exiting.
- Start does nothing in ordinary menus; its explicit exceptions are the introduction and Help. Only Start pauses a race; Circle has no racing action. Pause offers Resume, Controls and End race. Controls remains inside the paused state, and closing the diagram returns to Pause without resuming. In the ordinary pause menu, Start/Circle resume. End race requires selection and confirmation; Circle cancels the confirmation.
- Held buttons retain their original binding until released. Directional input held through an intro or pause is restored when racing resumes.
- Help and credits do not close on directional input. Shoulder buttons, Square and Triangle do not trigger hidden menu actions.
- Main-menu focus is retained when returning from a submenu.

Manual checks: original snowy main-menu layout; introduction at native PSP resolution; non-Start buttons leave the introduction open; held Start enters the menu without activating an item; Help reopens the diagram. The new pause-to-controls path was exercised on the physical PSP and the updated photo-referenced guide inspected on both hardware and PPSSPP. Earlier checks cover practice selection, racing, end-race confirmation/cancellation, return focus, Quit confirmation/cancellation and successful exit to PPSSPP. Inspected screenshots have no visible overlapping controls or missing terrain/HUD geometry in the checked scenes.

## Performance

Historical v0.3.0 measurements: PPSSPP 1.20.4, emulated CPU 333 MHz, default detail level 1, 600-frame scripted runs (599 measured intervals), including steering:

| Course | FPS | p95 frame | Maximum frame | Frames >35 ms | Music frames |
| --- | ---: | ---: | ---: | ---: | ---: |
| Frozen River | 59.94 | 16.684 ms | 17.214 ms | 0 | 599/599 |
| Chinese Wall | 59.84 | 16.684 ms | 33.367 ms | 0 | 599/599 |

This is emulator validation, not a physical-PSP compatibility or performance guarantee. These short runs supplement, not replace, the earlier all-course and endurance results in `benchmarks/graphics-2026-09-17.json`.

The current snow-track renderer was additionally tested on Bunny Hill and
Frozen River in PPSSPP: both recorded 59.940 FPS over 598 measured intervals,
with music active throughout. Physical hardware is slower; see the hardware
validation report and [remaining work](../TODO.md). The older all-course
results do not certify the current renderer.

## Regression checks

PSP build and `git diff --check` pass. Host tests pass: numerics; production terrain clipping including 10,000 boundary cases; save corruption/rollback; single-stream music lifetime; eight release tests including upload retries/timeouts; actual platform bindings (55 contextual mappings, held-button transitions and simultaneous controls); actual GUI snow geometry/UV/alpha with up to 4,000 flakes in one batch; actual introduction handler's Start-only gate; and runtime asset dimensions. All tests run in CI.

## Screenshot evidence

Captured from the running game, not UI mockups. Menu/intro images are from
PPSSPP; the updated Help and Pause images are native physical-PSP captures:

- [Main menu](images/psp-main-menu.png), also [at native PSP size](images/psp-main-menu-native.png).
- [First-run controls introduction](images/psp-controls-intro.png), also [at native PSP size](images/psp-controls-intro-native.png).
- [Graphical controls help](images/psp-controls-help.png).
- [Paused race](images/psp-pause-menu.png).
