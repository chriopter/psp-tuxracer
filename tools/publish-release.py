#!/usr/bin/env python3
"""Publish complete v0.x.0 releases with concise, delta-only release notes."""
import json
import os
from pathlib import Path
import re
import subprocess
import time


def upload_assets(version, assets):
    """Retry a failed asset, not every successful upload, before publishing."""
    for asset in assets:
        for attempt in range(3):
            try:
                subprocess.run(['gh', 'release', 'upload', version, str(asset), '--clobber'], check=True, timeout=180)
                break
            except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
                if attempt == 2:
                    raise
                time.sleep(5 * (attempt + 1))


def next_version(releases):
    versions = []
    published = []
    for release in releases:
        match = re.fullmatch(r'v0\.(\d+)\.(\d+)', release['tag_name'])
        if match:
            version = (int(match[1]), int(match[2]), release['tag_name'])
            versions.append(version)
            if not release.get('draft', False):
                published.append(version)
    # Reserve abandoned draft versions too, avoiding tag collisions.
    minor = max(versions)[0] + 1 if versions else 1
    return f'v0.{minor}.0', max(published)[2] if published else None


def notes(commit, previous):
    ancestry = subprocess.run(
        ['git', 'merge-base', '--is-ancestor', previous, commit],
        check=False) if previous else None
    if ancestry and ancestry.returncode not in (0, 1):
        ancestry.check_returncode()
    base = previous if ancestry and ancestry.returncode == 0 else None
    curated = Path('docs/release-notes.md')
    if curated.is_file():
        changed = subprocess.run(
            ['git', 'diff', '--quiet', base, commit, '--', str(curated)],
            check=False) if base else None
        if changed and changed.returncode not in (0, 1):
            changed.check_returncode()
        if not changed or changed.returncode == 1:
            return curated.read_text(encoding='utf-8').rstrip() + '\n'
    # Without an ancestral release, describe the current change only. Import
    # commits and retired histories are not user-facing release notes.
    revision = [f'{base}..{commit}'] if base else ['-1', commit]
    messages = subprocess.check_output(
        ['git', 'log', '--reverse', '--format=%h%n%s%x00', *revision], text=True)
    added, fixed = [], []
    repo = os.environ.get('GH_REPO', 'chriopter/psp-tuxracer')
    for message in messages.split('\0'):
        lines = message.strip().splitlines()
        if len(lines) >= 2:
            target = fixed if re.match(r'(?i)(fix|repair|prevent|correct|avoid)\b', lines[1]) else added
            target.append(f'- {lines[1]} ([{lines[0]}](https://github.com/{repo}/commit/{lines[0]})).')
    changes = ''
    if added:
        changes += '✨ New\n\n' + '\n'.join(added) + '\n\n'
    if fixed:
        changes += '🐛 Fixed\n\n' + '\n'.join(fixed) + '\n\n'
    if not changes:
        changes = '🔧 Build\n\n- Rebuild of the same commit; no new commits.\n\n'
    compare = (f'[Changes since {previous}](https://github.com/{repo}/compare/{previous}...{commit}).'
               if base else f'[Build commit](https://github.com/{repo}/commit/{commit}).')
    return ('The release in five lines:\n\n'
            '- 🎮 Extreme Tux Racer for PSP and PPSSPP.\n'
            '- 📝 Only changes for this build are listed below.\n'
            '- 🧪 Build and automated regression tests passed.\n'
            '- 🔬 Automated CI does not verify physical PSP compatibility or performance.\n'
            '- 📦 Game, corresponding sources and SHA-256 checksums are included.\n\n'
            + changes + compare + '\n\n'
            '📦 Downloads\n\n'
            '- `extremetuxracer-psp.zip` — EBOOT.PBP, game data and licenses.\n'
            '- `sources.tar.gz` — corresponding sources and build records.\n'
            '- `SHA256SUMS` — checksums for both archives.\n')


def main():
    repo = os.environ['GH_REPO']
    commit = os.environ['GITHUB_SHA']
    # Run ID survives "Re-run jobs"; a new manual build gets a new ID.
    marker = f'<!-- ci-run:{os.environ["GITHUB_RUN_ID"]} -->'
    pages = json.loads(subprocess.check_output(
        ['gh', 'api', '--paginate', '--slurp', f'repos/{repo}/releases'], text=True))
    releases = [release for page in pages for release in page]
    existing = next((r for r in releases if marker in (r.get('body') or '')), None)
    if existing and not existing['draft']:
        print(f'Release {existing["tag_name"]} already published for this run.')
        return
    assets = [Path('dist') / name for name in
              ('extremetuxracer-psp.zip', 'sources.tar.gz', 'SHA256SUMS')]
    for asset in assets:
        if not asset.is_file():
            raise SystemExit(f'Missing release asset: {asset}')
    if existing:
        version = existing['tag_name']
    else:
        version, previous = next_version(releases)
        path = Path('release-notes.md')
        path.write_text(notes(commit, previous) + '\n' + marker + '\n', encoding='utf-8')
        subprocess.run(['gh', 'release', 'create', version, '--draft',
                        '--target', commit, '--title', version,
                        '--notes-file', str(path)], check=True)
    # Keep incomplete uploads private; a retry resumes the same draft.
    upload_assets(version, assets)
    subprocess.run(['gh', 'release', 'edit', version, '--draft=false', '--latest'], check=True)


if __name__ == '__main__':
    main()
