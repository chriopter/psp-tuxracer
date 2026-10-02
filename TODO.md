# Remaining work

State after the hardware round of 2026-10-01/02; measurements and method are
in [hardware validation](docs/psp-hardware-validation.md).

## Stability

- [ ] One unexplained freeze of the console in the soak (test 583); not
      reproduced in five repeats. `config/trace` keeps the log unbuffered
      for the next one.
- [ ] Finish the 1020-test soak and record the final count.

## Performance

- [ ] Heavy snow at 60 FPS. It is at about 42 on Bunny Hill; the flakes'
      update and the two curtains' fill are what is left.
- [ ] The sky's side faces in video memory. Only the front face fits beside
      the terrain and the trees; a turn brings the others, read from ordinary
      memory, into the picture.
- [ ] Courses with long views over ice (Tux at Home, Path of Daggers) with
      penguins and wind: 57–58 FPS, GE-bound.

## Computer penguins

- [ ] They follow the ground: no jumps, no flight over a drop.
- [ ] They do not collect herring or leave tracks in the snow.
- [ ] A cup takes no notice of the place among them; it is shown only.

## Not yet tested on hardware

- [ ] Saving with a full or missing Memory Stick.
- [ ] Suspend and resume during a race.
- [ ] PSP-2000, PSP-3000 and PSP Go.
