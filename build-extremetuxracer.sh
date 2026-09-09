#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$project_dir"
docker build -t extremetuxracer-psp-build ports/extremetuxracer/psp
user_args=(--user "$(id -u):$(id -g)")
if docker info --format '{{json .SecurityOptions}}' | grep -q 'rootless'; then
  # Root in a rootless container maps to the invoking host user. Passing the
  # host UID again maps it to an unprivileged subordinate UID instead.
  user_args=()
fi
docker run --rm --network none "${user_args[@]}" \
  -v "$project_dir:/work" -w /work/ports/extremetuxracer/psp \
  extremetuxracer-psp-build make -j"${JOBS:-4}"
printf 'PSP build: %s\n' "$project_dir/ports/extremetuxracer/psp/EBOOT.PBP"
