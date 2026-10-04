# Remaining work

State after the hardware round of 2026-10-02/04; measurements and method are
in [hardware validation](docs/psp-hardware-validation.md).

## Stability

- [ ] One unexplained freeze of the console in the soak of the second round
      (test 583); not reproduced in five repeats then, nor in the 817 tests of
      the third round.
- [x] The soak: 817 tests on the console (third round), with the 669 of the
      second round 1486 starts of the game.

## Performance

- [ ] Heavy snow at 60 FPS. It runs at an even 30 (frame pacing); 3000
      flakes cost the GE what 60 would need, 1500 would give about 52.
- [ ] Clear weather with many trees in view (Explore Mountains, Tux at Home):
      55–57 FPS, the GE at the trees' alpha-tested sheets.
- [ ] One run of Explore Mountains (mirrored, five penguins, clear) at 36.6
      FPS among five at 55; not seen again, not explained.
- [ ] The sky's side faces in video memory. Only the front face fits beside
      the terrain and the trees; a turn brings the others, read from ordinary
      memory, into the picture.
- [ ] Courses with long views over ice (Tux at Home, Path of Daggers) with
      penguins and wind: 57–58 FPS, GE-bound.

## Computer penguins

- [ ] They follow the ground: no jumps, no flight over a drop.
- [ ] They do not collect herring or leave tracks in the snow.
- [ ] A cup takes no notice of the place among them; it is shown only.
- [ ] Their pace (own physics, scaled by the player's learned level) is
      tuned against the self-driving player only; it wants a human's races.

## Not yet tested on hardware

- [ ] Saving with a full or missing Memory Stick.
- [ ] Suspend and resume during a race.
- [ ] PSP-2000, PSP-3000 and PSP Go.
