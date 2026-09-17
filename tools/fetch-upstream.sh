#!/usr/bin/env bash
# Fetch original SVN commits into Git. Does not merge, push, or run svn dcommit.
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
bridge_dir="${1:-$project_dir/.cache/upstream-svn}"
mkdir -p "$bridge_dir"
bridge_dir="$(cd -- "$bridge_dir" && pwd)"
if [[ "$bridge_dir" == "$project_dir" ]]; then
    echo 'The SVN bridge must be separate from the PSP working tree.' >&2
    exit 1
fi
docker build -t extremetuxracer-svn "$project_dir/tools/upstream"
user_args=(--user "$(id -u):$(id -g)")
if docker info --format '{{json .SecurityOptions}}' | grep -q rootless; then
    user_args=()
fi
docker run --rm "${user_args[@]}" \
    -v "$bridge_dir:/bridge" extremetuxracer-svn sh -eu -c '
    url=https://svn.code.sf.net/p/extremetuxracer/code
    if [ ! -d /bridge/.git ]; then
        git svn init --stdlayout --prefix=svn/ "$url" /bridge
    fi
    cd /bridge
    test "$(git config svn-remote.svn.url)" = "$url"
    test "$(git config svn-remote.svn.fetch)" = "trunk:refs/remotes/svn/trunk"
    git svn fetch
    '
printf '\nOriginal commits fetched into the separate mirror: %s\n' "$bridge_dir"
printf 'Review there, then port selected changes into ports/extremetuxracer/.\n'
printf 'No branches or historical commits were added to the PSP repository.\n'
