import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from launch import LaunchContext

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('stereo_video_launch', ROOT / 'launch/stereo_video_pipeline.launch.py')
launch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launch)


class PipelineLaunchTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        for name in ['stereo_source.json', 'stereo_display.json']:
            (self.directory / name).write_text('{}')

    def run_setup(self, **overrides):
        context = LaunchContext()
        context.launch_configurations.update(dict(config_dir=str(self.directory), system='',
            config_parent='', source_config='stereo_source.json', display_config='stereo_display.json',
            alignment_config='stereo_alignment.json', rectify_config='stereo_rectify.json',
            color_config='', processor='auto', **{}))
        context.launch_configurations.update(overrides)
        with patch.object(launch, 'Node', side_effect=lambda **kw: kw), \
             patch.object(launch, 'TimerAction', side_effect=lambda **kw: kw):
            return launch.launch_setup(context)

    def test_alignment_default(self):
        (self.directory / 'stereo_alignment.json').write_text('{}')
        nodes = self.run_setup()
        self.assertEqual(nodes[1]['actions'][0]['executable'], 'stereo_alignment')
        self.assertEqual(nodes[2]['actions'][0]['arguments'][-2:], ['--gst-hint', 'stereo_alignment'])

    def test_rectify_and_custom_sockets(self):
        (self.directory / 'stereo_rectify.json').write_text('{}')
        nodes = self.run_setup()
        self.assertEqual(nodes[1]['actions'][0]['executable'], 'stereo_rectify')
        self.assertEqual(nodes[2]['actions'][0]['arguments'][-2:], ['--gst-hint', 'stereo_rectify'])
        (self.directory / 'stereo_source.json').write_text(json.dumps({'gst_output':'@dvrk:camera:custom'}))
        (self.directory / 'stereo_rectify.json').write_text(json.dumps({'gst_output':'@dvrk:rectify:custom'}))
        nodes = self.run_setup(processor='rectify')
        self.assertEqual(nodes[1]['actions'][0]['arguments'][-2:], ['-i','@dvrk:camera:custom'])
        self.assertEqual(nodes[2]['actions'][0]['arguments'][-2:], ['--gst-hint','@dvrk:rectify:custom'])

    def test_both_require_selection(self):
        for name in ['stereo_alignment.json','stereo_rectify.json']:
            (self.directory / name).write_text('{}')
        with self.assertRaisesRegex(RuntimeError, 'Choose processor'):
            self.run_setup()
        nodes = self.run_setup(processor='rectify')
        self.assertEqual(nodes[1]['actions'][0]['executable'], 'stereo_rectify')

    def test_system_and_colors(self):
        (self.directory / 'stereo_rectify.json').write_text('{}')
        colors = self.directory / 'stereo_color_adjustment.json'
        colors.write_text('{}')
        nodes = self.run_setup(system=self.directory.name, config_parent=str(self.directory.parent))
        self.assertEqual(nodes[1]['actions'][0]['arguments'][-2:], ['--color-adjustment',str(colors)])

    def test_missing_config(self):
        with self.assertRaisesRegex(RuntimeError,'No stereo processing'):
            self.run_setup()
        with self.assertRaisesRegex(RuntimeError,'Missing selected'):
            self.run_setup(processor='rectify')
        (self.directory / 'stereo_alignment.json').write_text('{}')
        (self.directory / 'stereo_display.json').unlink()
        with self.assertRaisesRegex(RuntimeError,'Missing configuration'):
            self.run_setup()


if __name__ == '__main__':
    unittest.main()
