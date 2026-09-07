#!/usr/bin/env python3
"""Exercise version selection, real Git changelogs and resumable publishing."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('publish', Path(__file__).with_name('publish-release.py'))
publish = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publish)


class ReleaseTests(unittest.TestCase):
    def test_numeric_versions_and_legacy_tags(self):
        self.assertEqual(publish.next_version([]), ('v0.1.0', None))
        releases = [{'tag_name': tag} for tag in ['build-7-1', 'v0.9.0', 'v0.10.2', 'v1.0.0']]
        self.assertEqual(publish.next_version(releases), ('v0.11.0', 'v0.10.2'))
        releases.append({'tag_name': 'v0.11.0', 'draft': True})
        self.assertEqual(publish.next_version(releases), ('v0.12.0', 'v0.10.2'))

    def test_real_commit_subjects_and_bodies(self):
        with tempfile.TemporaryDirectory() as directory:
            old = os.getcwd()
            os.chdir(directory)
            try:
                def git(*args):
                    return subprocess.check_output(['git', *args], text=True).strip()
                git('init', '-q')
                git('config', 'commit.gpgsign', 'false')
                git('config', 'tag.gpgsign', 'false')
                git('config', 'user.name', 'CI test')
                git('config', 'user.email', 'ci@example.invalid')
                git('commit', '--allow-empty', '-qm', 'Old commit')
                git('tag', 'v0.1.0')
                git('commit', '--allow-empty', '-qm', 'Fix steering\n\nPreserve curves and ümlauts.\nSecond body line.')
                git('commit', '--allow-empty', '-qm', 'Tune UI')
                result = publish.notes('HEAD', 'v0.1.0')
                self.assertNotIn('Old commit', result)
                for line in ('Fix steering', 'Preserve curves and ümlauts.', 'Second body line.', 'Tune UI'):
                    self.assertIn(line, result)
                self.assertLess(result.index('Fix steering'), result.index('Tune UI'))
                self.assertIn('no new commits', publish.notes('v0.1.0', 'v0.1.0'))
            finally:
                os.chdir(old)

    def run_publish(self, releases, missing=False, fail_upload=False):
        with tempfile.TemporaryDirectory() as directory:
            old = os.getcwd()
            os.chdir(directory)
            try:
                Path('dist').mkdir()
                if not missing:
                    for name in ('extremetuxracer-psp.zip', 'sources.tar.gz', 'SHA256SUMS'):
                        (Path('dist') / name).write_text('test asset')
                def run(args, **kwargs):
                    if fail_upload and args[2] == 'upload':
                        raise subprocess.CalledProcessError(1, args)
                with patch.dict(os.environ, GH_REPO='example/repo', GITHUB_SHA='abc123', GITHUB_RUN_ID='42'), \
                     patch.object(publish.subprocess, 'check_output', return_value=json.dumps([releases])), \
                     patch.object(publish, 'notes', return_value='Commit notes'), \
                     patch.object(publish.subprocess, 'run', side_effect=run) as calls:
                    if missing:
                        with self.assertRaises(SystemExit):
                            publish.main()
                    elif fail_upload:
                        with self.assertRaises(subprocess.CalledProcessError):
                            publish.main()
                    else:
                        publish.main()
                    return [call.args[0] for call in calls.call_args_list]
            finally:
                os.chdir(old)

    def test_publish_only_after_assets_upload(self):
        calls = self.run_publish([{'tag_name': 'v0.1.0'}])
        self.assertEqual([c[2] for c in calls], ['create', 'upload', 'edit'])
        self.assertTrue(all(c[3] == 'v0.2.0' for c in calls))
        self.assertIn('--draft', calls[0])
        self.assertIn('abc123', calls[0])
        self.assertIn('--draft=false', calls[-1])
        self.assertEqual(self.run_publish([], missing=True), [])

    def test_upload_failure_does_not_publish(self):
        calls = self.run_publish([], fail_upload=True)
        self.assertEqual([c[2] for c in calls], ['create', 'upload'])

    def test_retry_resumes_draft_or_skips_published_release(self):
        release = {'tag_name': 'v0.2.0', 'draft': True, 'body': '<!-- ci-run:42 -->'}
        calls = self.run_publish([release])
        self.assertEqual([c[2] for c in calls], ['upload', 'edit'])
        self.assertTrue(all(c[3] == 'v0.2.0' for c in calls))
        release['draft'] = False
        self.assertEqual(self.run_publish([release]), [])


if __name__ == '__main__':
    unittest.main()
