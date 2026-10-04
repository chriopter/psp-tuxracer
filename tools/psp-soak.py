#!/usr/bin/env python3
"""Soak the game on a real PSP through PSPLink: fresh start, one action, verdict.

  tools/psp-soak.py <total> <seed>     run until results.jsonl holds <total> lines
  tools/psp-soak.py record             walk every menu path once and keep its screens as references
  tools/psp-soak.py one <kind> [args]  one test, printed, not recorded (kinds: measured, finish, or a walk)
  tools/psp-soak.py report             the figures of results.jsonl for the documentation

Kinds of test, drawn by the seed and the test's number:
  measured  a race of 600-1800 frames with the fixed benchmark input; random
            course, light, snow, wind, mirror and penguins
  finish    a race driven by the autopilot to the finish line (up to ten minutes)
  marathon  the same with everything at its worst: night, heavy snow, strong
            wind, mirrored, five penguins
  chaos     into a race through the menus, then forty seconds of random keys
            (never Circle, so never back to the main menu and its saves)
  abort     the game reset by PSPLink while it loads, then a measured race
  menu      a walk through the menus by key presses (remotejoy), each screen
            compared with a reference picture taken by "record"
  film      (tests past 600) a self-driven race of which ninety frames in a
            row are written out by the game itself and kept as PNGs under
            $BENCH/video/clips/sNNN: footage for the wall of the launch video

Needs usbhostfs_pc running in $BENCH/host0, the staged game with its PRX in
$BENCH/host0/etr-psp-test, remotejoy.prx in $BENCH/host0, and a relay in
~/.config/pspkit-autoboot.env (RELAY=shelly, SHELLY_URL=...) for a console
that stops answering. Stop between two tests with: touch $BENCH/runs/soak/STOP

Before it ends (total reached or STOP) it copies the owner's save back from
$BENCH/host0/$SAVE_BACKUP: the tests play into it as a player would.

Exits 0 when the total is reached or STOP was found, 1 when the console
cannot be brought back.
"""
from pathlib import Path
import hashlib, json, os, random, shutil, socket, struct, subprocess, sys, time, urllib.request

ROOT = Path(__file__).resolve().parent.parent
BENCH = Path(os.environ.get('BENCH', Path.home() / 'git/psp-tuxracer-bench'))
HOST0 = BENCH / 'host0'
GAME = HOST0 / 'etr-psp-test'
RUNS = BENCH / 'runs/soak'
GOLDEN = BENCH / 'golden'
PSPSH = os.environ.get('PSPSH', str(Path.home() / '.local/opt/pspdev/bin/pspsh'))
MODULE = 'Extreme Tux Racer'
COURSES = sorted(p.name for p in (ROOT / 'ports/extremetuxracer/data/courses/default').iterdir() if p.is_dir())
BUTTONS = {'select': 0x1, 'start': 0x8, 'ltrigger': 0x100, 'rtrigger': 0x200, 'up': 0x10, 'right': 0x20, 'down': 0x40, 'left': 0x80,
           'triangle': 0x1000, 'circle': 0x2000, 'cross': 0x4000, 'square': 0x8000}


def pspsh(command, timeout=20):
    try:
        return subprocess.run([PSPSH, '-e', command], capture_output=True, text=True, timeout=timeout).stdout
    except subprocess.TimeoutExpired:
        return ''


def alive():
    return 'PSPLink' in pspsh('ver', 8)


def relay(on):
    settings = dict(line.strip().split('=', 1) for line in
                    Path(os.environ.get('PSPKIT_ENV', Path.home() / '.config/pspkit-autoboot.env')).read_text().splitlines()
                    if '=' in line and not line.startswith('#'))
    urllib.request.urlopen(f"{settings['SHELLY_URL']}/rpc/Switch.Set?id=0&on={'true' if on else 'false'}", timeout=5).read()


def wait_alive(seconds):
    end = time.time() + seconds
    while time.time() < end:
        if alive():
            return True
        time.sleep(0.5)
    return False


def power_cycle():
    relay(False)
    time.sleep(15)
    relay(True)
    return wait_alive(90)


def fresh_psplink():
    """PSPLink without the game: a reset, and the relay when that does not answer."""
    if alive():
        pspsh('reset', 10)
        time.sleep(2)
        if wait_alive(40) and MODULE not in pspsh('modlist'):
            return True
    return power_cycle()


def launch():
    for name in ('etr-errors.log', 'etr.log', 'config/benchmark-result.json', 'config/race-log.txt',
                 'config/sound-log.txt', 'config/timing.log', 'config/frame-times-us.json', 'config/frame-work-us.json',
                 'config/frame-stats.txt'):
        (GAME / name).unlink(missing_ok=True)
    (GAME / 'config/trace').touch()
    script = BENCH / 'start.psh'
    script.write_text('cd host0:/etr-psp-test\n./extremetuxracer.prx\n')
    out = subprocess.run([PSPSH, str(script)], capture_output=True, text=True, timeout=30).stdout
    return MODULE in out


def log_text():
    try:
        return (GAME / 'etr-errors.log').read_text(errors='replace')
    except OSError:
        return ''


def wait_result(seconds):
    """The result, or None after the given time."""
    path = GAME / 'config/benchmark-result.json'
    end = time.time() + seconds
    while time.time() < end:
        try:
            return json.loads(path.read_text())
        except (OSError, ValueError):
            time.sleep(1)
    return None


def race(n, rng, finish, worst=False):
    course = rng.choice(COURSES)
    cond = [3, 3, 3, 1, 5] if worst else \
        [rng.randrange(4), rng.randrange(4), rng.randrange(4), rng.randrange(2), rng.randrange(6)]
    # Nearly six minutes of race: those that reach the line took up to 260 s;
    # an autopilot caught in a hollow is not waited for longer.
    frames = 21000 if finish else rng.randrange(600, 1801, 300)
    record = run_race(frames, course, cond, finish)
    if worst:
        record['kind'] = 'marathon'
    return record


def run_race(frames, course, cond, finish):
    record = {'kind': 'finish' if finish else 'measured', 'course': course, 'cond': cond, 'frames_asked': frames}
    (GAME / 'config/benchmark').write_text(f"{frames} {course} {' '.join(map(str, cond))}\n")
    if finish:
        (GAME / 'config/benchmark-capture').write_text('999999\n')
    else:
        (GAME / 'config/benchmark-capture').unlink(missing_ok=True)
    if not launch():
        return record | {'ok': False, 'reason': 'the game did not start'}
    # Loading from host0 takes about 15 s; the slowest snow race ran near 20
    # FPS. A race to the line ends at the line, mostly within five minutes.
    # Ten minutes of race at 20 FPS and more, for an autopilot caught in a
    # hollow until the frame limit; on while a race with penguins logs.
    result = wait_result(60 + frames / 20)
    if result is None:
        reason = 'no answer from PSPLink' if not alive() else 'no result in time'
        return record | {'ok': False, 'reason': reason}
    measured = result['all']
    # The frame rate of every race: the mean, the middle and the slow end of
    # the frame times, and how many frames stood for two blanks or more.
    record |= {'fps': measured['fps'], 'frames': measured['frames'], 'max_us': measured['max_us'],
               'median_us': measured['median_us'], 'p95_us': measured['p95_us'], 'missed': measured.get('missed'),
               'heap': result['heap_peak_bytes'], 'free_user': result['min_free_user_bytes'], 'cpu_mhz': result['cpu_mhz']}
    record['race_time'] = result.get('race_time')
    if finish:
        record['reached_finish'] = bool(result.get('finished'))
        # The race log is the penguins' (src/opponents.cpp): none without them.
        if cond[4] and not (GAME / 'config/race-log.txt').exists():
            return record | {'ok': False, 'reason': 'no race log from the self-driven race'}
    elif measured['frames'] < min(frames, 7200) - 5:
        return record | {'ok': False, 'reason': f"only {measured['frames']} of {frames} frames"}
    if result['cpu_mhz'] != 333:
        return record | {'ok': False, 'reason': f"CPU at {result['cpu_mhz']} MHz"}
    if MODULE not in pspsh('modlist'):
        return record | {'ok': False, 'reason': 'game or PSPLink gone after the race'}
    return record | {'ok': True}


# ---- menus -----------------------------------------------------------------

class Pad:
    """remotejoy's input channel as usbhostfs_pc offers it on 127.0.0.1:10004."""
    def __init__(self):
        self.socket = socket.create_connection(('127.0.0.1', 10004), timeout=5)
        self.send(2, 0xFFFFFFFF)
    def send(self, kind, mask):
        self.socket.sendall(struct.pack('<III', 0x909ACCEF, kind, mask))
    def tap(self, button, after=0.6):
        self.send(1, BUTTONS[button])
        time.sleep(0.08)
        self.send(2, BUTTONS[button])
        time.sleep(after)
    def close(self):
        self.send(2, 0xFFFFFFFF)
        self.socket.close()


def screenshot():
    """The screen as rows of (r, g, b), top first, or None."""
    path = HOST0 / 'shot.bmp'
    path.unlink(missing_ok=True)
    pspsh('scrshot host0:/shot.bmp', 20)
    try:
        data = path.read_bytes()
    except OSError:
        return None
    if len(data) < 54 or data[:2] != b'BM':
        return None
    offset, = struct.unpack_from('<I', data, 10)
    width, height, _, bits = struct.unpack_from('<iiHH', data, 18)
    if bits != 24 or len(data) < offset + abs(height) * ((width * 3 + 3) & ~3):
        return None
    stride = (width * 3 + 3) & ~3
    rows = [data[offset + y * stride: offset + y * stride + width * 3] for y in range(abs(height))]
    return rows[::-1] if height > 0 else rows


def difference(a, b, box=None):
    """Mean absolute difference of two screens, 0-255, every fourth pixel."""
    x0, y0, x1, y1 = box or (0, 0, 480, 272)
    total = count = 0
    for y in range(y0, y1, 4):
        ra, rb = a[y], b[y]
        for x in range(x0 * 3, x1 * 3, 12):
            total += abs(ra[x] - rb[x]) + abs(ra[x + 1] - rb[x + 1]) + abs(ra[x + 2] - rb[x + 2])
            count += 3
    return total / count


# Parts of a screen that do not move: the pause box and the question over a race.
BOXES = {'pause': (90, 60, 395, 205), 'pause-end-row': (90, 60, 395, 205), 'end-question': (75, 95, 405, 185),
         # the rows under the course, which the walk has changed
         'race-select-after': (0, 88, 270, 272)}
LIMIT = 14.0     # falling snow behind a menu is about 3; another screen is 30 and more


class Walk:
    def __init__(self, name, recording):
        self.name, self.recording, self.pad, self.problem = name, recording, None, None
        self.seen = []
    def keys(self, *tokens):
        for token in tokens:
            if self.problem:
                return
            button, _, after = token.partition(':')
            self.pad.tap(button, float(after or 0.6))
    def screen(self, label):
        if self.problem:
            return
        shot = screenshot()
        if shot is None:
            self.problem = f'no picture at {label}'
            return
        reference = GOLDEN / f'{label}.bmp'
        if self.recording:
            GOLDEN.mkdir(parents=True, exist_ok=True)
            shutil.copy(HOST0 / 'shot.bmp', GOLDEN / f'{self.name}--{label}.bmp')
            if not reference.exists():
                shutil.copy(HOST0 / 'shot.bmp', reference)
            return
        golden = bmp_rows(reference)
        value = difference(shot, golden, BOXES.get(label))
        # A screen that loads from the host can be late: look again before
        # calling it wrong (test 425 showed the blank page of a load).
        for _ in range(3):
            if value <= LIMIT:
                break
            time.sleep(2)
            shot = screenshot() or shot
            value = difference(shot, golden, BOXES.get(label))
        self.seen.append((label, round(value, 1)))
        if value > LIMIT:
            shutil.copy(HOST0 / 'shot.bmp', RUNS / f'fail-shot-{label}.bmp')
            self.problem = f'screen "{label}" differs from its reference by {value:.1f}'
    def moving(self, label):
        """Two pictures a moment apart must differ: the race is running."""
        if self.problem:
            return
        first = screenshot()
        time.sleep(0.5)
        second = screenshot()
        if first is None or second is None:
            self.problem = f'no picture at {label}'
        elif not self.recording and difference(first, second) < 0.3:
            self.problem = f'the picture stands still at {label}'
    def racing(self, count=1):
        if not self.problem and log_text().count('STEP racing') < count:
            self.problem = 'the race did not start'


def bmp_rows(path):
    data = path.read_bytes()
    offset, = struct.unpack_from('<I', data, 10)
    width, height = struct.unpack_from('<ii', data, 18)
    stride = (width * 3 + 3) & ~3
    rows = [data[offset + y * stride: offset + y * stride + width * 3] for y in range(abs(height))]
    return rows[::-1] if height > 0 else rows


def to_menu(w):
    w.keys('cross:3')            # player selection -> controls picture
    w.screen('controls')
    w.keys('start:1.5')
    w.screen('menu')

def to_race(w, rng):
    w.keys('down', 'cross:2')    # Training
    w.screen('race-select')
    w.keys(*['right:0.5'] * rng.randrange(0, 6))     # another course
    w.keys('cross:18')
    w.racing()
    w.moving('race')

def walk_practice(w, rng):
    to_menu(w); to_race(w, rng)
    w.keys('start:1.5'); w.screen('pause')
    w.keys('down', 'down', 'cross:1.5'); w.screen('end-question')
    w.keys('cross:3', 'cross:3')
    w.screen('race-select-after')

def walk_pause(w, rng):
    to_menu(w); to_race(w, rng)
    w.keys('start:1.5'); w.screen('pause')
    w.keys('down', 'cross:1.5'); w.screen('controls-paused')
    w.keys('start:2.5'); w.screen('pause')
    w.keys('circle:1.5'); w.moving('race resumed')
    w.keys('start:1.5'); w.screen('pause')
    w.keys('down', 'down', 'cross:1.5', 'circle:1.5'); w.screen('pause-end-row')

def walk_reset(w, rng):
    to_menu(w); to_race(w, rng)
    w.keys('triangle:2'); w.moving('race after reset')
    w.keys('square:0.3', 'left:0.3', 'cross:1', 'triangle:2'); w.moving('race after second reset')
    w.keys('start:1.5'); w.screen('pause')

def walk_event(w, rng):
    to_menu(w)
    w.keys('cross:2'); w.screen('event-select')
    w.keys('cross:2'); w.screen('event')
    w.keys('cross:18'); w.racing(); w.moving('cup race')
    w.keys('start:1.5'); w.screen('pause')

def walk_config(w, rng):
    to_menu(w)
    w.keys('down', 'down', 'cross:2'); w.screen('config')
    w.keys('down', 'down', 'down', 'up', 'circle:2'); w.screen('menu-config-row')

def walk_score(w, rng):
    to_menu(w)
    w.keys('down', 'down', 'down', 'cross:2'); w.screen('score')
    w.keys('down', 'up', 'circle:2'); w.screen('menu-score-row')

def walk_help(w, rng):
    to_menu(w)
    w.keys('down', 'down', 'down', 'down', 'cross:2'); w.screen('help')
    w.keys('start:2'); w.screen('menu-help-row')

def walk_credits(w, rng):
    to_menu(w)
    w.keys('down', 'down', 'down', 'down', 'down', 'cross:2'); w.moving('credits')
    w.keys('circle:2'); w.screen('menu-credits-row')

def walk_player(w, rng):
    w.keys('right', 'left', 'down', 'right', 'left', 'up'); w.screen('player')
    to_menu(w)
    w.keys('circle:2'); w.screen('player')

CHAOS_KEYS = ['cross', 'square', 'triangle', 'ltrigger', 'rtrigger', 'up', 'down', 'left', 'right', 'start']


def chaos(w, rng):
    """Into a race, then forty seconds of whatever: jumps, tricks, resets,
    pauses, braking, steering, ending the race and starting the next."""
    to_menu(w)
    w.keys('down', 'cross:2')                        # Training
    w.keys(*['right:0.3'] * rng.randrange(0, 22))    # any course
    w.keys('down', 'down', 'down', 'down', 'down', 'down')   # the penguins' row
    w.keys(*['right:0.3'] * rng.randrange(0, 6))
    w.keys('cross:18')
    w.racing()
    end = time.time() + 40
    while not w.problem and time.time() < end:
        key = rng.choice(CHAOS_KEYS)
        hold = rng.choice([0.05, 0.08, 0.2, 0.6, 1.5])
        w.pad.send(1, BUTTONS[key])
        if rng.random() < 0.3:                       # two at once: a trick, a jump while steering
            w.pad.send(1, BUTTONS[rng.choice(CHAOS_KEYS[:5])])
        time.sleep(hold)
        w.pad.send(2, 0xFFFFFFFF)
        time.sleep(rng.choice([0.02, 0.1, 0.3]))
    # Whatever it ended in, the game must still be drawing: a second picture
    # differs. The pause and the controls picture stand still by design, so
    # out of them: Start closes the picture, Circle resumes from the pause.
    for keys in ([], ['start:2'], ['circle:2'], ['start:2', 'circle:2']):
        w.keys(*keys)
        if w.problem:
            return
        first = screenshot(); time.sleep(0.7); second = screenshot()
        if first is None or second is None:
            w.problem = 'no picture after the keys'
        elif difference(first, second) >= 0.3:
            return
    shutil.copy(HOST0 / 'shot.bmp', RUNS / 'fail-shot-chaos-end.bmp')
    w.problem = w.problem or 'the picture stands still after the keys'


WALKS = {f.__name__[5:]: f for f in (walk_practice, walk_pause, walk_reset, walk_event, walk_config,
                                    walk_score, walk_help, walk_credits, walk_player)}


def run_walk(name, rng, recording=False, steps=None):
    record = {'kind': 'chaos' if steps else 'menu', 'walk': name}
    (GAME / 'config/benchmark').unlink(missing_ok=True)
    (GAME / 'config/benchmark-capture').unlink(missing_ok=True)
    if not launch():
        return record | {'ok': False, 'reason': 'the game did not start'}
    time.sleep(14)
    if 'RemoteJoy' not in pspsh('ldstart host0:/remotejoy.prx'):
        return record | {'ok': False, 'reason': 'remotejoy did not load'}
    walk = Walk(name, recording)
    try:
        walk.pad = Pad()
        (steps or WALKS[name])(walk, rng)
        if steps and not walk.problem:
            walk.keys('start:1.5')      # out of the race, so that the game notes its frame rate
        walk.pad.close()
    except OSError as error:
        walk.problem = walk.problem or f'key channel: {error}'
    record['screens'] = walk.seen
    record |= play_frame_rate()
    if walk.problem:
        if not alive():
            walk.problem = 'no answer from PSPLink (' + walk.problem + ')'
        return record | {'ok': False, 'reason': walk.problem}
    if MODULE not in pspsh('modlist'):
        return record | {'ok': False, 'reason': 'game or PSPLink gone after the walk'}
    if 'xception' in log_text():
        return record | {'ok': False, 'reason': 'an exception in the game log'}
    return record | {'ok': True}


def frame_rate_before(first):
    """The frame rate of a film test's race before its frames are written
    out (writing them stalls the game): from the interval of every frame
    (config/frame-times-us.json), the mean, the worst second, and the
    frames that stood for two blanks or more."""
    try:
        times = json.loads((GAME / 'config/frame-times-us.json').read_text())[:max(0, first - 3)]
    except (OSError, ValueError):
        return {}
    if len(times) < 60:
        return {}
    worst, window, start = 1e9, 0, 0
    for end, t in enumerate(times):                 # the slowest stretch of about a second
        window += t
        while window - times[start] >= 1000000:
            window -= times[start]
            start += 1
        if window >= 1000000:
            worst = min(worst, (end - start + 1) * 1000000 / window)
    return {'fps': round(len(times) * 1000000 / sum(times), 3), 'fps_frames': len(times),
            'worst_second': round(worst, 1) if worst < 1e9 else None, 'max_us': max(times),
            'missed': sum(1 for t in times if t > 25000)}


CLIPS = BENCH / 'video/clips'
FILM_FRAMES = 90


def write_png(path, rgb, width, height):
    import zlib
    def chunk(kind, data):
        body = kind + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body))
    rows = b''.join(b'\0' + rgb[y * width * 3:(y + 1) * width * 3] for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(rows, 6)) + chunk(b'IEND', b''))


_rgb565 = None
def film(n, rng):
    """Ninety frames in a row of a self-driven race, dumped by the game
    (config/benchmark-capture: raw 512x272 RGB565), kept as 480x272 PNGs."""
    global _rgb565
    from array import array
    course = rng.choice(COURSES)
    cond = [rng.randrange(4), rng.choice([0, 0, 1, 2, 3]), rng.randrange(4), rng.randrange(2), rng.randrange(6)]
    first = rng.randrange(150, 1200, 30)
    record = {'kind': 'film', 'course': course, 'cond': cond, 'first_frame': first}
    for old in (GAME / 'config').glob('capture-*.raw'):
        old.unlink()
    (GAME / 'config/benchmark').write_text(f"{first + FILM_FRAMES + 30} {course} {' '.join(map(str, cond))}\n")
    (GAME / 'config/benchmark-capture').write_text(' '.join(str(first + k) for k in range(FILM_FRAMES)) + '\n')
    if not launch():
        return record | {'ok': False, 'reason': 'the game did not start'}
    result = wait_result(240 + first / 20)
    if result is None:
        return record | {'ok': False, 'reason': 'no answer from PSPLink' if not alive() else 'no result in time'}
    record |= {'heap': result['heap_peak_bytes'], 'race_time': result.get('race_time'), 'reached_finish': bool(result.get('finished'))}
    record |= frame_rate_before(first)
    raws = sorted((GAME / 'config').glob('capture-*.raw'), key=lambda f: int(f.stem.split('-')[1]))
    # a race that reached the line before the frames were due has fewer: still a race, but no clip
    good = [f for f in raws if f.stat().st_size >= 512 * 272 * 2]
    record['frames'] = len(good)
    if len(good) >= 60:
        if _rgb565 is None:
            _rgb565 = [bytes(((p >> 0 & 31) * 255 // 31, (p >> 5 & 63) * 255 // 63, (p >> 11) * 255 // 31)) for p in range(65536)]
        out = CLIPS / f's{n:03d}'
        out.mkdir(parents=True, exist_ok=True)
        for k, raw in enumerate(good):
            pixels = array('H', raw.read_bytes()[:512 * 272 * 2])
            rgb = b''.join(b''.join(_rgb565[p] for p in pixels[y * 512:y * 512 + 480]) for y in range(272))
            write_png(out / f'f{k:03d}.png', rgb, 480, 272)
        record['clip'] = out.name
    elif not record['reached_finish']:
        return record | {'ok': False, 'reason': f'only {len(good)} of {FILM_FRAMES} frames written'}
    for raw in raws:
        raw.unlink()
    if MODULE not in pspsh('modlist'):
        return record | {'ok': False, 'reason': 'game or PSPLink gone after the race'}
    return record | {'ok': True}


def play_frame_rate():
    """The frame rate of the races of a walk, as the game notes it when a
    race is left (config/frame-stats.txt: frames, FPS, slowest frame in
    microseconds, frames of two blanks or more)."""
    try:
        rows = [line.split() for line in (GAME / 'config/frame-stats.txt').read_text().splitlines()]
    except OSError:
        return {}
    rows = [(int(a), float(b), int(c), int(d)) for a, b, c, d in rows]
    frames = sum(row[0] for row in rows)
    if not frames:
        return {}
    return {'frames': frames, 'fps': round(frames / sum(row[0] / row[1] for row in rows), 3),
            'max_us': max(row[2] for row in rows), 'missed': sum(row[3] for row in rows)}


def aborted_start(n, rng):
    """The game reset in the middle of loading, then started again for a race."""
    wait = rng.uniform(1, 14)
    record = {'kind': 'abort', 'reset_after': round(wait, 1)}
    (GAME / 'config/benchmark').write_text('600 bunny_hill\n')     # saves are off in a benchmark
    (GAME / 'config/benchmark-capture').unlink(missing_ok=True)
    if not launch():
        return record | {'ok': False, 'reason': 'the game did not start'}
    time.sleep(wait)
    if not fresh_psplink():
        return record | {'ok': False, 'reason': 'no PSPLink after the reset in loading'}
    return record | {k: v for k, v in run_race(600, rng.choice(COURSES), [0, 0, 0, 0, rng.randrange(6)], False).items()
                     if k not in ('kind',)}


def one_test(n, seed):
    rng = random.Random(seed * 1000003 + n)
    draw = rng.random()
    if 600 < n <= 817:              # footage for the wall, after the six hundred
        return film(n, rng)
    if n <= 114:                    # the mix the first 114 were drawn from
        if draw < 0.55:
            return race(n, rng, finish=False)
        if draw < 0.70:
            return race(n, rng, finish=True)
        return run_walk(rng.choice(sorted(WALKS)), rng)
    if draw < 0.30:
        return race(n, rng, finish=False)
    if draw < 0.55:
        return race(n, rng, finish=True)
    if draw < 0.65:
        return race(n, rng, finish=True, worst=True)
    if draw < 0.80:
        return run_walk('chaos', rng, steps=chaos)
    if draw < 0.85:
        return aborted_start(n, rng)
    return run_walk(rng.choice(sorted(WALKS)), rng)


SAVE = 'ms0:/PSP/SAVEDATA/ETRX00001PROFILE'
SAVE_BACKUP = os.environ.get('SAVE_BACKUP', 'save-backup-2026-10-03')     # in host0


def restore_save():
    """The menu walks and the races of the chaos test save as a player
    would, into the owner's own save. Put back the copy taken before."""
    backup = HOST0 / SAVE_BACKUP
    if not backup.is_dir() or not fresh_psplink():
        return
    for f in sorted(backup.iterdir()):
        print(pspsh(f'cp host0:/{SAVE_BACKUP}/{f.name} {SAVE}/{f.name}', 30).strip(), flush=True)


def build_id():
    return hashlib.md5((GAME / 'extremetuxracer.prx').read_bytes()).hexdigest()[:8]


def report():
    """The figures for docs/psp-hardware-validation.md, from results.jsonl."""
    import collections, statistics
    rows = [json.loads(line) for line in (RUNS / 'results.jsonl').read_text().splitlines()]
    kinds = collections.Counter((r['kind'], r['ok']) for r in rows)
    print(f"{len(rows)} tests, {sum(1 for r in rows if not r['ok'])} counted as failed")
    for kind in sorted({k for k, _ in kinds}):
        print(f"  {kind}: {kinds[(kind, True)]} passed, {kinds[(kind, False)]} failed")
    for r in rows:
        if not r['ok']:
            print(f"  failed {r['n']} ({r['kind']}, build {r['build']}): {r['reason']}")
    builds = collections.Counter(r['build'] for r in rows)
    print('builds:', ', '.join(f'{b} x{n}' for b, n in builds.items()))
    last = list(builds)[-1]
    # frame rates: only the builds with the objects where the course has them
    good = [r for r in rows if r.get('fps') and r['n'] > 118]
    races = [r for r in good if r['kind'] in ('measured', 'abort')]
    print(f"\nmeasured races with the fixed benchmark input: {len(races)}")
    print('| Weather | Races | Slowest | Mean |\n|---|---|---|---|')
    for snow, name in enumerate(('clear', 'light snow', 'medium snow', 'heavy snow')):
        v = [r['fps'] for r in races if r['cond'][1] == snow]
        if v:
            print(f"| {name} | {len(v)} | {min(v):.1f} FPS | {statistics.mean(v):.1f} FPS |")
    print('\n| Course | Races | Slowest | Mean | Slowest in clear weather |\n|---|---|---|---|---|')
    by = collections.defaultdict(list)
    for r in races:
        by[r['course']].append(r)
    for course, v in sorted(by.items(), key=lambda kv: statistics.mean(r['fps'] for r in kv[1])):
        clear = [r['fps'] for r in v if r['cond'][1] == 0]
        print(f"| {course} | {len(v)} | {min(r['fps'] for r in v):.1f} | {statistics.mean(r['fps'] for r in v):.1f} | "
              + (f"{min(clear):.1f}" if clear else '-') + ' |')
    timed = [r for r in good if r.get('missed') is not None and r.get('frames')]
    print('\nframes that stood for two blanks or more, by kind of test (since the game counts them):')
    for kind in ('measured', 'finish', 'marathon', 'chaos', 'menu'):
        v = [r for r in timed if r['kind'] == kind]
        if v:
            print(f"  {kind}: {len(v)} tests, mean {statistics.mean(r['fps'] for r in v):.1f} FPS, slowest {min(r['fps'] for r in v):.1f}, "
                  f"{100 * sum(r['missed'] for r in v) / sum(r['frames'] for r in v):.0f}% of frames")
    films = [r for r in rows if r['kind'] == 'film' and r.get('fps')]
    if films:
        print(f"\nfilm tests with the frame rate of every frame before the capture: {len(films)}")
        print('| Weather | Races | Slowest | Mean | Worst second | Frames of two blanks |\n|---|---|---|---|---|---|')
        for snow, name in enumerate(('clear', 'light snow', 'medium snow', 'heavy snow')):
            v = [r for r in films if r['cond'][1] == snow]
            if v:
                seconds = [r['worst_second'] for r in v if r.get('worst_second')]
                print(f"| {name} | {len(v)} | {min(r['fps'] for r in v):.1f} | {statistics.mean(r['fps'] for r in v):.1f} | "
                      f"{min(seconds):.1f} | {100 * sum(r['missed'] for r in v) / sum(r['fps_frames'] for r in v):.0f}% |")
        slow = [r for r in films if r['fps'] < (50 if r['cond'][1] == 0 else 30)]
        print(f"  below 50 FPS in clear weather or 30 in snow: {len(slow)}")
        for r in sorted(slow, key=lambda r: r['fps']):
            print(f"    {r['n']} {r['course']} {r['cond']}: {r['fps']:.1f} FPS, worst second {r.get('worst_second')}, build {r['build']}")
    lines = [r for r in rows if r['kind'] in ('finish', 'marathon') and r['n'] > 124]
    print(f"\nraces to the line: {sum(1 for r in lines if r.get('reached_finish'))} of {len(lines)} reached it")
    heaps = [r['heap'] for r in rows if r.get('heap')]
    print(f"largest heap peak: {max(heaps) / 1e6:.1f} MB")


def main():
    RUNS.mkdir(parents=True, exist_ok=True)
    if len(sys.argv) >= 2 and sys.argv[1] == 'report':
        report()
        return
    if len(sys.argv) >= 2 and sys.argv[1] == 'record':
        for name in sys.argv[2:] or sorted(WALKS):
            if not fresh_psplink():
                sys.exit('console does not come back')
            print(name, run_walk(name, random.Random(1), recording=True), flush=True)
        fresh_psplink()
        return
    if len(sys.argv) >= 3 and sys.argv[1] == 'one':
        if not fresh_psplink():
            sys.exit('console does not come back')
        kind = sys.argv[2]
        if kind == 'chaos':
            record = run_walk('chaos', random.Random(int(sys.argv[3]) if len(sys.argv) > 3 else 1), steps=chaos)
            print(record)
            if not record['ok']:
                print(pspsh('exprint', 10)); print(pspsh('thlist', 15))
        elif kind == 'film':
            print(film(int(sys.argv[3]) if len(sys.argv) > 3 else 999, random.Random(int(sys.argv[3]) if len(sys.argv) > 3 else 1)))
        elif kind == 'abort':
            print(aborted_start(0, random.Random(int(sys.argv[3]) if len(sys.argv) > 3 else 1)))
        elif kind in WALKS:
            print(run_walk(kind, random.Random(int(sys.argv[3]) if len(sys.argv) > 3 else 1)))
        else:
            frames, course, *cond = sys.argv[3:]
            print(run_race(int(frames), course, [int(c) for c in cond], kind == 'finish'))
        return
    total, seed = int(sys.argv[1]), int(sys.argv[2])
    results = RUNS / 'results.jsonl'
    done = len(results.read_text().splitlines()) if results.exists() else 0
    while done < total:
        if (RUNS / 'STOP').exists():
            print('STOP found', flush=True)
            restore_save()
            return
        if not fresh_psplink():
            sys.exit('console does not come back')
        n = done + 1
        started = time.time()
        record = one_test(n, seed)
        record = {'n': n, 'time': time.strftime('%Y-%m-%dT%H:%M:%S'), 'build': build_id()} | record
        record['seconds'] = round(time.time() - started, 1)
        if not record['ok']:
            record['exception'] = pspsh('exprint', 10)[-600:]
            record['threads'] = pspsh('thlist', 15)[-1500:]
            for name in ('etr-errors.log', 'etr.log'):
                if (GAME / name).exists():
                    shutil.copy(GAME / name, RUNS / f'fail-{n}-{name}')
            for shot in RUNS.glob('fail-shot-*.bmp'):
                shot.rename(RUNS / f'fail-{n}-{shot.name[10:]}')
        with results.open('a') as out:
            out.write(json.dumps(record) + '\n')
        print(json.dumps(record), flush=True)
        done = n
    restore_save()


if __name__ == '__main__':
    main()
