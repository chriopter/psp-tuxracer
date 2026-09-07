#!/usr/bin/env python3
"""Publish one v0.x.0 release per successful CI run, with full commit messages."""
import json
import os
from pathlib import Path
import re
import subprocess


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
    revision = f'{previous}..{commit}' if previous else commit
    messages = subprocess.check_output(
        ['git', 'log', '--reverse', '--format=%h%n%B%x00', revision], text=True)
    entries = []
    for message in messages.split('\0'):
        lines = message.strip().splitlines()
        if len(lines) >= 2:
            entries.append(f'- {lines[1]} ({lines[0]})' +
                           ''.join('\n  ' + line for line in lines[2:]))
    changes = '\n'.join(entries) or '- Rebuild of the same commit; no new commits.'
    return (f'Automated PSP build from `{commit}`.\n\n'
            f'## Commits since {previous or "the beginning"}\n\n{changes}\n\n'
            '## Downloads\n\n'
            '- Game ZIP with EBOOT.PBP, game data and licenses\n'
            '- Corresponding source package\n- SHA-256 checksums\n\n'
            'Build and automated tests passed. Physical PSP compatibility and '
            'performance require hardware testing; emulator results are documented in the repository.\n')


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
                        '--target', commit, '--title', f'Extreme Tux Racer PSP {version}',
                        '--notes-file', str(path)], check=True)
    # Keep incomplete uploads private; a retry resumes the same draft.
    subprocess.run(['gh', 'release', 'upload', version, *map(str, assets), '--clobber'], check=True)
    subprocess.run(['gh', 'release', 'edit', version, '--draft=false', '--latest'], check=True)


if __name__ == '__main__':
    main()
