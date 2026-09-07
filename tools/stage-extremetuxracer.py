#!/usr/bin/env python3
"""Stage official ETR data for PSP; originals remain untouched."""
from pathlib import Path
import argparse, shutil, subprocess, math
root=Path(__file__).resolve().parent.parent
source=root/'ports/extremetuxracer'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, default=root/'state/extremetuxracer/config/ppsspp/PSP/GAME/ExtremeTuxRacer')
target=parser.parse_args().output.resolve()
magick=shutil.which('magick')
identify=[magick, 'identify'] if magick else ['identify']
convert=[magick] if magick else ['convert']
if not (source/'psp/EBOOT.PBP').is_file():
    raise SystemExit('Build EBOOT.PBP before staging.')
target.mkdir(parents=True,exist_ok=True)
shutil.copytree(source/'data',target/'data',dirs_exist_ok=True)
for p in (target/'data').rglob('elev.png'):
    w,h=map(int,subprocess.check_output(identify+['-format','%w %h',str(p)],text=True).split())
    if w*h>16000:
        scale=math.sqrt(16000/(w*h));size=f'{max(2,int(w*scale))}x{max(2,int(h*scale))}!'
        for name in ('elev.png','terrain.png'):
            f=p.parent/name
            subprocess.run(convert+[str(f),'-filter','point','-resize',size,str(f)],check=True)
for p in (target/'data/music').glob('*.ogg'):
    subprocess.run(['ffmpeg','-v','error','-y','-i',str(p),'-ar','22050','-ac','1',str(p.with_suffix('.wav'))],check=True)
    p.unlink()
for p in (target/'data/music').glob('*.lst'):
    p.write_text(p.read_text().replace('.ogg','.wav'))
(target/'config').mkdir(exist_ok=True)
options=target/'config/options.txt'
if not options.exists(): options.write_text('[fullscreen] 1 [res_type] 0 [detail_level] 1 [language] en_GB [sound_volume] 80 [music_volume] 25 [framerate] 60 [forward_clip_distance] 60 [backward_clip_distance] 12 [fov] 60 [bpp_mode] 16 [tree_detail_distance] 15 [tux_sphere_divisions] 4 [tux_shadow_sphere_div] 2 [course_detail_level] 20 [use_papercut_font] 0 [ice_cursor] 0 [full_skybox] 0 [use_quad_scale] 0\n')
if (source/'psp/EBOOT.PBP').exists():shutil.copy2(source/'psp/EBOOT.PBP',target/'EBOOT.PBP')
profile=target.parent.parent/'SYSTEM'
profile.mkdir(parents=True,exist_ok=True)
for name in ('ppsspp.ini','controls.ini'):
    if not (profile/name).exists():shutil.copy2(source/'psp'/name,profile/name)
print(target)
