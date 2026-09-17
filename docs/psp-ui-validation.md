# PSP menu and controls validation — 2026-09-17

Base: `89d2ad0cc8774bde406424ff2a4f8acc46e680da`, plus the local UI/control changes.
Tested binary SHA-256: `1d0faac9e63f9dcc8ef0775ba1b88735800091c40a7ef78ce981e3c2d605e7eb`.

## Presentation and input

- Main menu: left-hand selection cards, right-hand graphical PSP control legend, bottom navigation hints, original game logo. Native 480 × 272 framebuffer. The user-requested image-generated icon set uses one 256 × 128 atlas (64 KiB); no background texture loads. Original artwork and full generation prompt are in [the asset record](artwork/psp-controls.md).
- Cross confirms; Circle goes back. Main-menu Circle opens player selection. Player selection and the Quit item both require explicit confirmation before exiting.
- Start does nothing in menus; Start/Circle pause a race. In pause, Start/Circle resume. End race requires selection and confirmation; Circle cancels the confirmation.
- Held buttons retain their original binding until released. Directional input held through an intro or pause is restored when racing resumes.
- Help and credits do not close on directional input. Shoulder buttons, Square and Triangle do not trigger hidden menu actions.
- Main-menu focus is retained when returning from a submenu.

Manual PPSSPP checks: main-menu layout, inactive menu actions, practice selection, racing, Circle-to-pause, pause selection, end-race confirmation cancellation, Start-to-resume, help navigation, return focus, Quit confirmation, cancellation and successful exit to PPSSPP. Inspected screenshots have no visible overlapping controls or missing terrain/HUD geometry in the checked scenes.

## Performance

PPSSPP 1.20.4, emulated CPU 333 MHz, default detail level 1, 600-frame scripted runs (599 measured intervals), including steering:

| Course | FPS | p95 frame | Maximum frame | Frames >35 ms | Music frames |
| --- | ---: | ---: | ---: | ---: | ---: |
| Frozen River | 59.94 | 16.684 ms | 17.214 ms | 0 | 599/599 |
| Chinese Wall | 59.94 | 16.684 ms | 17.561 ms | 0 | 599/599 |

This is emulator validation, not a physical-PSP compatibility or performance guarantee. These short runs supplement, not replace, the earlier all-course and endurance results in `benchmarks/graphics-2026-09-17.json`.

## Regression checks

PSP build and `git diff --check` pass. Host tests pass: numerics; production terrain clipping including 10,000 boundary cases; save corruption/rollback; single-stream music lifetime; seven release tests including transient asset-upload retries; and the new controller test compiling the actual platform bindings (33 contextual mappings, held-button transitions, disabled menu actions, simultaneous controls). Controller tests also run in CI.

## Screenshot evidence

Captured from the running emulator, not UI mockups:

- [Main menu](images/psp-main-menu.png), also [at native PSP size](images/psp-main-menu-native.png).
- [Graphical controls help](images/psp-controls-help.png).
- [Paused race](images/psp-pause-menu.png).
