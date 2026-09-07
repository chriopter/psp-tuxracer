#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$project_dir"
docker build -t extremetuxracer-psp-build ports/extremetuxracer/psp
docker run --rm --network none --user "$(id -u):$(id -g)" \
  -v "$project_dir:/work" -w /work/ports/extremetuxracer/psp \
  extremetuxracer-psp-build make -j"${JOBS:-4}"
printf 'PSP build: %s\n' "$project_dir/ports/extremetuxracer/psp/EBOOT.PBP"
