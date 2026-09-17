# Native save validation — 2026-09-07

Tests use an isolated PPSSPP 1.20.4 profile, not personal savedata or physical PSP hardware.

| Check | Result |
|---|---|
| PSP system Save/Load dialogs, title and icon | Passed |
| Cancel Save without changing PROFILE.DAT | Passed; identical SHA-256 before/after |
| Create a player with the native keyboard and autosave | Passed in the 32 MB profile |
| Automatic restoration of options, players and high scores at startup | Passed; all three working files restored byte-for-byte after replacing a local file with a sentinel |
| Damaged native profile | Rejected; registration autosave did not overwrite it |
| Save container integrity | Host test detects every individual bit flip in the header and payload, truncation, extra data and size violations |
| Local restore write failure | Host test keeps every original working file |
| Local restore rename failure after partial backup | Host test restores the originals and allows a later successful restore |
| Final build: native load restores selected player and character preview | Passed |
| Final build: enter a race after native dialogs, pause, leave and autosave | Passed in the 32 MB profile; savedata timestamp advanced, success shown and no guest errors |
| Final build: keyboard opening press does not insert a character | Passed; unchanged default name saved exactly |
| Physical PSP / Memory Stick / suspend-resume | Not yet tested |

The host restore tests compile the actual `writeFiles` and existence-check functions from the PSP implementation. They do not emulate the PSP savedata utility or firmware. Repeated-load handling clears the in-memory highscore table before applying saved scores. Character previews are reloaded when returning to player selection.

The initial PSP-1000 memory stress run exposed the SDK's default 512 KiB external allocation threshold. The final build explicitly reserves 4 MiB with `PSP_HEAP_THRESHOLD_SIZE_KB(4096)` and stages skybox PNGs at their GPU resolution to avoid large temporary decode buffers. Missing skybox faces are supplied from the existing licensed artwork. See the final `native-save-*.json` benchmark records and the [physical-device test procedure](hardware-validation.md).
