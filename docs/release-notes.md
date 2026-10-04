The release in five lines:

- 🐧 Race against one to five computer penguins — in Practice and in events, on the PSP only.
- ⚡ Much faster on a real PSP: 58 FPS on average in clear weather, and falling snow without jerks.
- 🎛️ New menus made for the PSP's buttons: lists you move through up and down, larger text, the weather signs at the head of their rows.
- 💾 Saving and loading through the PSP's own dialogs now works on real hardware.
- 🌍 The game asks for your language at the first start; everything on screen is translated in all twelve.

✨ New

- Play against penguins: choose none or one to five in the race or event selection. Each is one of the game's characters. The best of them is a little quicker than you usually are; they bump into you, your place is on screen, and the result lists everyone's time.
- Menus as vertical PSP lists, with the course picture and description beside the race selection.
- Language selection at the first start.
- Controls picture with a real photograph of a PSP, labelled in your language.
- New icon, background and preview film in the PSP's game menu, made from the game running on a PSP.

⚡ Faster

- Clear weather: 58 FPS on average over all courses; the fullest scenes, with many trees and five penguins, 55–57.
- Snowfall: light and medium from 30 to about 50 FPS; heavy snow runs at an even 30 instead of jerking between 30 and 60.
- Loading: the first menu appears after about 6 seconds instead of 15; a race starts about 3 seconds sooner.
- About 3 MB less memory in use during a race.
- Nothing is written to the Memory Stick except your saved data.

🐛 Fixed

- Saved data did not work on a real PSP; system dialogs showed white boxes.
- The clock showed 00:00.00 for an instant just before each full second.
- Trees, flags and herring stand where the courses have them: on every course most of them were in the wrong place or outside it.
- A crash when pressing Triangle (back onto the course) on some courses.
- Penguins no longer drive into each other or stop with a gap between them: each collides by the size of its own body.
- You no longer always win against the penguins: they drive by the slope, not by your speed.
- A slope in the distance no longer turns from ice to snow in one frame; no trench of snow on ice.
- The wind dial no longer runs over the edge of the screen.
- The horizon no longer pops in: terrain and trees come out of the fog.
- Tux is rounder: his outline showed corners on the PSP's screen.
- Text that was English in every language: race options, pause menu, questions before quitting.

🧪 Validation

Tested on a PSP-1000 (firmware 6.61, 333 MHz) with music on: 817 automated starts of the game in this round — races to the finish line, races in the worst weather against five penguins, walks through every menu, forty seconds of random keys in a race, resets in the middle of loading — after 669 in the round before. Four of the 817 are counted as failed: three were faults of the test itself, one was a crash of the game, found by the random keys and fixed. Frame rates for every course, method and limits are in [hardware validation](https://github.com/chriopter/psp-tuxracer/blob/main/docs/psp-hardware-validation.md); what is still open is in [TODO](https://github.com/chriopter/psp-tuxracer/blob/main/TODO.md). Other PSP models are not tested.

📦 Downloads

- `extremetuxracer-psp.zip` — copy `PSP/GAME/ExtremeTuxRacer` to your homebrew-enabled PSP, or open its EBOOT.PBP in PPSSPP.
- `sources.tar.gz` — corresponding game/dependency sources and build records.
- `SHA256SUMS` — checksums for both archives.
