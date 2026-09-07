#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
export PATH="$project_dir/emulator/xvfb/usr/bin:$PATH"
export SDL_VIDEODRIVER=x11
unset WAYLAND_DISPLAY EGL_PLATFORM
if [[ "${TUXRACER_BACKGROUND_AUDIO:-0}" == 1 ]]; then
    export SDL_AUDIODRIVER=pulseaudio
else
    export SDL_AUDIODRIVER=dummy
fi
exec xvfb-run -a -s '-screen 0 960x544x24 -nolisten tcp' "$project_dir/start-extremetuxracer.sh"
