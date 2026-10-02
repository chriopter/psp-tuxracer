#!/usr/bin/env python3
"""Soak the game on a real PSP through PSPLink: fresh start, one action, verdict.

  tools/psp-soak.py <total> <seed>     run until results.jsonl holds <total> lines
  tools/psp-soak.py record             walk every menu path once and keep its screens as references
  tools/psp-soak.py one <kind> [args]  one test, printed, not recorded (kinds: measured, finish, or a walk)

Three kinds of test, drawn by the seed and the test's number:
  measured  a race of 600-1800 frames with the fixed benchmark input; random
            course, light, snow, wind, mirror and penguins
  finish    a race driven by the autopilot towards the finish line
  menu      a walk through the menus by key presses (remotejoy), each screen
            compared with a reference picture taken by "record"

Needs usbhostfs_pc running in $BENCH/host0, the staged game with its PRX in
$BENCH/host0/etr-psp-test, remotejoy.prx in $BENCH/host0, and a relay in
~/.config/pspkit-autoboot.env (RELAY=shelly, SHELLY_URL=...) for a console
that stops answering. Stop between two tests with: touch $BENCH/runs/soak/STOP

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
BUTTONS = {'select': 0x1, 'start': 0x8, 'up': 0x10, 'right': 0x20, 'down': 0x40, 'left': 0x80,
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
                 'config/sound-log.txt', 'config/timing.log', 'config/frame-times-us.json', 'config/frame-work-us.json'):
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
    path = GAME / 'config/benchmark-result.json'
    end = time.time() + seconds
    while time.time() < end:
        try:
            return json.loads(path.read_text())
        except (OSError, ValueError):
            time.sleep(1)
    return None


def race(n, rng, finish):
    course = rng.choice(COURSES)
    cond = [rng.randrange(4), rng.randrange(4), rng.randrange(4), rng.randrange(2), rng.randrange(6)]
    frames = 7200 if finish else rng.randrange(600, 1801, 300)
    return run_race(frames, course, cond, finish)


def run_race(frames, course, cond, finish):
    record = {'kind': 'finish' if finish else 'measured', 'course': course, 'cond': cond, 'frames_asked': frames}
    (GAME / 'config/benchmark').write_text(f"{frames} {course} {' '.join(map(str, cond))}\n")
    if finish:
        (GAME / 'config/benchmark-capture').write_text('999999\n')
    else:
        (GAME / 'config/benchmark-capture').unlink(missing_ok=True)
    if not launch():
        return record | {'ok': False, 'reason': 'the game did not start'}
    # Loading from host0 takes about 15 s; the slowest snow race ran near 20 FPS.
    result = wait_result(60 + frames / 18)
    if result is None:
        reason = 'no answer from PSPLink' if not alive() else 'no result in time'
        return record | {'ok': False, 'reason': reason}
    measured = result['all']
    record |= {'fps': measured['fps'], 'frames': measured['frames'], 'max_us': measured['max_us'],
               'heap': result['heap_peak_bytes'], 'free_user': result['min_free_user_bytes'], 'cpu_mhz': result['cpu_mhz']}
    if finish:
        record['reached_finish'] = measured['frames'] < frames - 10
        if not (GAME / 'config/race-log.txt').exists():
            return record | {'ok': False, 'reason': 'no race log from the self-driven race'}
    elif measured['frames'] < frames - 5:
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

WALKS = {f.__name__[5:]: f for f in (walk_practice, walk_pause, walk_reset, walk_event, walk_config,
                                    walk_score, walk_help, walk_credits, walk_player)}


def run_walk(name, rng, recording=False):
    record = {'kind': 'menu', 'walk': name}
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
        WALKS[name](walk, rng)
        walk.pad.close()
    except OSError as error:
        walk.problem = walk.problem or f'key channel: {error}'
    record['screens'] = walk.seen
    if walk.problem:
        if not alive():
            walk.problem = 'no answer from PSPLink (' + walk.problem + ')'
        return record | {'ok': False, 'reason': walk.problem}
    if MODULE not in pspsh('modlist'):
        return record | {'ok': False, 'reason': 'game or PSPLink gone after the walk'}
    return record | {'ok': True}


def one_test(n, seed):
    rng = random.Random(seed * 1000003 + n)
    draw = rng.random()
    if draw < 0.55:
        return race(n, rng, finish=False)
    if draw < 0.70:
        return race(n, rng, finish=True)
    return run_walk(rng.choice(sorted(WALKS)), rng)


def build_id():
    return hashlib.md5((GAME / 'extremetuxracer.prx').read_bytes()).hexdigest()[:8]


def main():
    RUNS.mkdir(parents=True, exist_ok=True)
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
        if kind in WALKS:
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
            for name in ('etr-errors.log', 'etr.log'):
                if (GAME / name).exists():
                    shutil.copy(GAME / name, RUNS / f'fail-{n}-{name}')
            for shot in RUNS.glob('fail-shot-*.bmp'):
                shot.rename(RUNS / f'fail-{n}-{shot.name[10:]}')
        with results.open('a') as out:
            out.write(json.dumps(record) + '\n')
        print(json.dumps(record), flush=True)
        done = n
    fresh_psplink()


if __name__ == '__main__':
    main()
