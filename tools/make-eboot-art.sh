#!/bin/sh
# The pictures the XMB shows for the game, made from the game itself as a
# real PSP drew it: ICON0 (the card), PIC1 (the background behind it) and
# ICON1 (the film the card turns into).
#
#   sh tools/make-eboot-art.sh <film.mp4> [still seconds] [film start seconds]
#
# <film.mp4> is a recording of a race without the HUD, 480x272 or a whole
# multiple of it. The one in use was dumped frame by frame from a PSP-1000
# (config/benchmark-capture with config/benchmark-nohud, see
# docs/psp-hardware-validation.md): Bunny Hill against five penguins.
#
# PIC1.PNG   one frame of it, 480x272, with the game's title at the top
#            right, clear of the card and its film (left) and the clock.
# icon0.png  the same frame at 144x80 with the game's title over it. It is
#            the savedata icon too: staging copies it to data/psp-icon.png.
# ICON1.PMF  six seconds, 144x80, H.264 Main 2.1 in Sony's PSMF layout,
#            marked 0014 -- the only kind the XMB of a PSP-1000 plays.
#            Encoding and wrapping are pspdx-app's
#            dev/tools/eboot-media/make-icon1.sh; MAKE_ICON1 names it.
#
# Needs ffmpeg, ImageMagick 7 and a C compiler.
set -e
film="$1"; still="${2:-0.75}"; start="${3:-0}"
[ -f "$film" ] || { echo "usage: sh tools/make-eboot-art.sh <film.mp4> [still seconds] [film start seconds]" >&2; exit 2; }
here="$(cd "$(dirname "$0")/.." && pwd)"
psp="$here/ports/extremetuxracer/psp"
title="$here/ports/extremetuxracer/data/textures/menu_title.png"
make_icon1="${MAKE_ICON1:-$HOME/git/pspdx-app/dev/tools/eboot-media/make-icon1.sh}"
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT

ffmpeg -nostdin -v error -y -ss "$still" -i "$film" -frames:v 1 -vf scale=480:272:flags=neighbor "$tmp/still.png"

# The title with a soft dark edge, so it holds on snow: small for the card,
# large for the background.
for size in 134 230; do
	magick "$title" -trim +repage -resize ${size}x PNG32:"$tmp/title-$size.png"
	magick "$tmp/title-$size.png" \( +clone -background '#06204f' -shadow 85x1.5+0+1 \) +swap \
		-background none -layers merge +repage PNG32:"$tmp/title-edge-$size.png"
done
magick "$tmp/still.png" "$tmp/title-edge-230.png" -gravity northeast -geometry +22+32 -composite PNG24:"$psp/PIC1.PNG"
magick "$tmp/still.png" -resize '144x80^' -gravity center -extent 144x80 \
	"$tmp/title-edge-134.png" -gravity north -composite PNG24:"$psp/icon0.png"

sh "$make_icon1" "$film" "$psp/ICON1.PMF" "$start"
