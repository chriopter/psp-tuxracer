# psp-devloop integration

This directory wires the project into
[psp-devloop](https://github.com/chriopter/psp-devloop), a staged validation
loop: host tests and build, then emulators, then real PSP hardware. The harness
is a separate checkout; nothing here may assume a path inside it beyond
`$PSP_DEVLOOP_ROOT`.

## The contract

Every runner and every stage answers with one of three exit codes, and nothing
else is meaningful:

| Code | Meaning |
|---|---|
| `0` | pass |
| `77` | skipped on purpose, for example an emulator that is not installed |
| anything else | failure; the loop stops and keeps this code |

Prefer `77` over a failure for anything merely unconfigured. A loop that fails
on a missing optional emulator gets ignored, and then it stops being run.

Decide success from a result file the application writes, never from emulator
output: that output is noisy, differs between emulators, and changes with their
versions.

Never describe a passing emulator stage as proof that the application works on
a PSP. Emulators are a fast filter; only stage 03 answers that question.

## The normal shape

`config` names a command per stage. The two runners source
`$PSP_DEVLOOP_ROOT/lib/project.sh` and call `run_ppsspp` or `run_jpcsp`, which
handle waiting, timeouts, process cleanup and the skip reporting. What belongs
to this project lives in `common.sh`:

- `result_file` — the file the application writes
- `stage` — prepare the memory stick, and ask the application for the run you want
- `check` — decide whether that file means a pass; non-zero fails the stage
- `cleanup` — undo what `stage` left behind; runs on every exit, including a timeout

Give each emulator its own timeout. JPCSP interprets rather than recompiles and
needs several times as long as PPSSPP for the same work.

## When this shape does not fit

It is not mandatory. A `*_TEST_COMMAND` is an ordinary shell command, so a
project that does not fit can point it anywhere and ignore `lib/project.sh`
entirely. If you do that, keep the parts that the library was doing for you:

- exit 77 rather than failing when something is only unconfigured
- bound the run with a timeout, so a hung emulator cannot stall the loop
- stop everything you started, including processes it forked
- remove the result file before the run, so a leftover cannot pass

Do not copy code out of the harness into this directory to achieve that. If
something generic is missing, it belongs in the harness, where every project
gets it and a fix reaches all of them.
