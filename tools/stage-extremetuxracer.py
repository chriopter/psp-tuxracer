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
shutil.copy2(source/'psp/icon0.png',target/'data/psp-icon.png')
for p in (target/'data').rglob('elev.png'):
    w,h=map(int,subprocess.check_output(identify+['-format','%w %h',str(p)],text=True).split())
    if w*h>16000:
        scale=math.sqrt(16000/(w*h));size=f'{max(2,int(w*scale))}x{max(2,int(h*scale))}!'
        for name in ('elev.png','terrain.png'):
            f=p.parent/name
            subprocess.run(convert+[str(f),'-filter','point','-resize',size,str(f)],check=True)
# Match the runtime's 256-pixel skybox cap before decoding on the PSP.
# This avoids keeping two full 512x512 RGBA images during texture upload.
for p in (target/'data/env').rglob('*.png'):
    w,h=map(int,subprocess.check_output(identify+['-format','%w %h',str(p)],text=True).split())
    if w>256 or h>256:
        subprocess.run(convert+[str(p),'-filter','point','-resize',f'{min(w,256)}x{min(h,256)}!',str(p)],check=True)
# Upstream environments supply three sides. PSP turns can expose all six.
# Extend the existing artwork instead of binding missing/uninitialized textures.
for front in (target/'data/env').rglob('front.png'):
    w,h=map(int,subprocess.check_output(identify+['-format','%w %h',str(front)],text=True).split())
    for name,y in (('top.png',0),('bottom.png',h-1)):
        face=front.parent/name
        if not (source/'data'/face.relative_to(target/'data')).exists():
            subprocess.run(convert+[str(front),'-crop',f'{w}x1+0+{y}','+repage','-scale',f'{w}x{h}!',str(face)],check=True)
    back=front.parent/'back.png'
    if not (source/'data'/back.relative_to(target/'data')).exists():
        subprocess.run(convert+[str(front),'-flop',str(back)],check=True)
# The three snow-curtain pictures are 512x512 and stand some 60 pixels wide
# on the PSP's screen; the GE reads a texture that much larger than its
# picture very slowly. 128x128 is still larger than they are shown. The game
# drew every curtain twice a frame and now draws it once: alpha' =
# 1-(1-alpha)^2 is what the two passes came to, so the look stays.
for name in ('snow1.png', 'snow2.png', 'snow3.png'):
    f=target/'data/textures'/name
    if f.exists():
        subprocess.run(convert+[str(source/'data/textures'/name),'-filter','Lanczos','-resize','128x128!',
                                '-channel','A','-fx','1-(1-u)*(1-u)','+channel',str(f)],check=True)
# The game's sounds as the PSP's mixer plays them, 22050 Hz: read as they
# are, in half the bytes, instead of being resampled from 44100 Hz at every
# start.
for p in (target/'data/sounds').glob('*.wav'):
    subprocess.run(['ffmpeg','-v','error','-y','-i',str(source/'data/sounds'/p.name),'-ar','22050','-ac','2','-c:a','pcm_s16le',str(p)],check=True)
# Every course.dim of a group in one file, as CCourseList::Load reads it:
# "@" and the directory, then the lines as the game's list reader joins them
# (a line that does not begin with "*" continues the one before).
for group in (target/'data/courses').iterdir():
    if not (group/'courses.lst').exists(): continue
    out=[]
    for dim in sorted(group.glob('*/course.dim')):
        entries=[]
        for line in dim.read_text(encoding='utf-8',errors='surrogateescape').splitlines():
            if not line or line.startswith('#'): continue
            if line.startswith('*') or not entries: entries.append(line)
            else: entries[-1]+=line
        if entries: out+=['@'+dim.parent.name]+entries
    (group/'course-dims.lst').write_text('\n'.join(out)+'\n',encoding='utf-8',errors='surrogateescape')
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
