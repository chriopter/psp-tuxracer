#!/usr/bin/env python3
"""Produce a clean PSP game ZIP and matching source bundle (GPL-2.0-or-later)."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tarfile
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parent.parent
LOCK = ROOT / 'docs/dependencies-lock.json'


def run(*args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def sha256(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-cache', type=Path, default=ROOT / '.cache/dependency-sources')
    args = parser.parse_args()
    lock = json.loads(LOCK.read_text())
    cache = args.source_cache.resolve()
    cache.mkdir(parents=True, exist_ok=True)
    dist = ROOT / 'dist'
    dist.mkdir(exist_ok=True)
    run('git', '-C', str(ROOT), 'diff', '--quiet', 'HEAD', '--')
    commit = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
    eboot = ROOT / 'ports/extremetuxracer/psp/EBOOT.PBP'
    raw = eboot.read_bytes()
    if len(raw) < 40 or raw[:4] != b'\0PBP':
        raise RuntimeError('Build did not produce a PSP PBP executable')
    offsets = struct.unpack_from('<8I', raw, 8)
    if list(offsets) != sorted(offsets) or offsets[0] < 40 or offsets[-1] > len(raw):
        raise RuntimeError('Invalid PBP section offsets')
    archives = lock['sdk_sources'] + [s for p in lock['packages'] for s in p['sources']]

    def download(item):
        target = cache / item['file']
        if not target.exists() or sha256(target) != item['sha256']:
            temporary = target.with_suffix(target.suffix + '.part')
            run('curl', '--fail', '--location', '--retry', '3', '--max-time', '600',
                '--silent', '--show-error', item['url'], '-o', str(temporary))
            if sha256(temporary) != item['sha256']:
                temporary.unlink()
                raise RuntimeError('Source checksum mismatch: ' + item['file'])
            temporary.replace(target)
        print('Verified source:', item['file'], flush=True)

    with ThreadPoolExecutor(max_workers=4) as pool:
        list(pool.map(download, archives))

    with tempfile.TemporaryDirectory(prefix='etr-package-') as directory:
        work = Path(directory)
        source_root = work / 'sources'
        sdk = source_root / 'sdk-build-records'
        sdk.mkdir(parents=True)
        # Export only package metadata and licenses, never SDK binaries.
        script = 'set -eu\ncp /usr/local/pspdev/build.txt /out/build.txt\nmkdir -p /out/licenses\n'
        for package in lock['packages']:
            name, version = package['name'], package['version']
            script += f'tar -xOf /usr/local/pspdev/var/cache/pacman/pkg/{name}-{version}-any.pkg.tar.gz .BUILDINFO > /out/{name}.BUILDINFO\n'
            script += f'cp -r /usr/local/pspdev/psp/share/licenses/{name} /out/licenses/\n'
        run('docker', 'run', '--rm', '--network', 'none', '--user', f'{os.getuid()}:{os.getgid()}',
            '-v', f'{sdk}:/out', lock['sdk_image'], 'sh', '-c', script)
        recipe_archive = next(a for a in archives if a['file'] == 'psp-packages.tar.gz')
        with tarfile.open(cache / recipe_archive['file']) as recipes:
            for package in lock['packages']:
                name = package['name']
                expected = 'pkgbuild_sha256sum = ' + package['recipe_sha256']
                if expected not in (sdk / f'{name}.BUILDINFO').read_text().splitlines():
                    raise RuntimeError('SDK build recipe changed: ' + name)
                member = next(m for m in recipes.getmembers() if m.name.endswith(f'/{name}/PSPBUILD'))
                if hashlib.sha256(recipes.extractfile(member).read()).hexdigest() != package['recipe_sha256']:
                    raise RuntimeError('Recipe source mismatch: ' + name)
        for item in archives:
            shutil.copy2(cache / item['file'], source_root / item['file'])
        shutil.copy2(LOCK, source_root / LOCK.name)
        shutil.copy2(ROOT / 'docs/binary-distribution.md', source_root / 'README.md')
        run('git', '-C', str(ROOT), 'archive', '--format=tar.gz', '--prefix=extremetuxracer-psp/',
            '-o', str(source_root / 'game-source.tar.gz'), 'HEAD')
        game_root = work / 'game'
        game = game_root / 'PSP/GAME/ExtremeTuxRacer'
        run('python3', str(ROOT / 'tools/stage-extremetuxracer.py'), '--output', str(game))
        shutil.copytree(sdk / 'licenses', game_root / 'LICENSES/dependencies')
        shutil.copy2(ROOT / 'LICENSE', game_root / 'LICENSES/ExtremeTuxRacer-GPL.txt')
        shutil.copy2(ROOT / 'docs/upstream-copyright.txt', game_root / 'LICENSES/upstream-copyright.txt')
        (game_root / 'INSTALL.txt').write_text(
            'Copy PSP/GAME/ExtremeTuxRacer to the PSP memory stick or open its EBOOT.PBP in PPSSPP.\n'
            'Requires PSP homebrew support. Tested in PPSSPP at 333 MHz, not physical hardware.\n'
            'The optional PSP/SYSTEM files are PPSSPP defaults; keep your existing settings if preferred.\n'
            'Distribute the accompanying sources.tar.gz and its license/build records with this game.\n')
        metadata = {'commit': commit, 'eboot_sha256': sha256(eboot), 'sdk_image': lock['sdk_image'],
                    'dependency_lock_sha256': sha256(LOCK), 'validation': 'PBP structure and numerical tests; no CI hardware/FPS test'}
        for folder in (game_root, source_root):
            (folder / 'build.json').write_text(json.dumps(metadata, indent=2) + '\n')
        with zipfile.ZipFile(dist / 'extremetuxracer-psp.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
            for path in sorted(game_root.rglob('*')):
                if path.is_file():
                    archive.write(path, path.relative_to(game_root))
        # Most inputs are already compressed; avoid spending CI CPU recompressing them.
        with tarfile.open(dist / 'sources.tar.gz', 'w:gz', compresslevel=1) as archive:
            archive.add(source_root, arcname='sources')
    outputs = [dist / 'extremetuxracer-psp.zip', dist / 'sources.tar.gz']
    (dist / 'SHA256SUMS').write_text(''.join(f'{sha256(p)}  {p.name}\n' for p in outputs))
    print('Distribution ready:', dist)


if __name__ == '__main__':
    main()
