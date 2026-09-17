# Original source and updates

This is an independent PSP port of [Extreme Tux Racer](https://sourceforge.net/projects/extremetuxracer/), based on the official 0.8.4 release archive. It is not a GitHub-native fork and does not use a submodule.

The initial history on `main` contains exactly three commits:

1. `Import original Extreme Tux Racer 0.8.4`: the unmodified release archive at the repository root.
2. `Move original sources into ports/extremetuxracer`: a pure directory move; file contents and modes are unchanged.
3. `Convert Extreme Tux Racer 0.8.4 to PSP`: the complete port, graphics fixes, tooling, tests and documentation.

The old development history, imported SVN branches/tags and previous releases have been retired. The new release series starts at `v0.1.0`. Source URLs, archive checksum and the new import commit are recorded in [upstream.json](upstream.json).

## Future upstream changes

`./tools/fetch-upstream.sh` maintains a separate SVN-to-Git mirror under `.cache/upstream-svn/` for reviewing new original commits. It does not add branches or historical commits to this repository, merge changes, publish anything, or write to SVN. Docker and Git are required; the first import can take a while.

Review the relevant changes in that separate mirror. Port selected patches into `ports/extremetuxracer/`, resolve PSP-specific differences, run the [tests and build](development.md), and verify in PPSSPP before committing. Do not merge the entire SVN history into `main` or push the mirror's refs. The release archive and SVN tag may differ in generated build files and development-only assets, so patches need review rather than blind application.

`git_base` remains the initial original-source import commit for release notes.
