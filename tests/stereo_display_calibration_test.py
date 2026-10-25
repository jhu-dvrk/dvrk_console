#!/usr/bin/env python3
"""Focused checks for stereo display calibration preview and keyboard controls."""

import runpy
import time
import unittest
from pathlib import Path

import numpy as np
import gi

gi.require_version("Gdk", "3.0")
from gi.repository import Gdk


CALIBRATION = runpy.run_path(
    str(Path(__file__).resolve().parents[1] / "scripts" / "stereo_display_calibration"),
    run_name="calibration_test",
)


class Adjustment:
    def __init__(self):
        self.value = 0

    def get_value(self):
        return self.value

    def set_value(self, value):
        self.value = value


class Controls:
    video_shift = Adjustment()
    widget_shift = Adjustment()


class Event:
    def __init__(self, keyval):
        self.keyval = keyval
        self.state = 0


class DisplayCalibrationTest(unittest.TestCase):
    def test_shift_is_bounded_to_each_eye_with_runtime_rounding(self):
        frame = np.empty((80, 320, 3), dtype=np.uint8)
        frame[:, :160] = 80
        frame[:, 160:] = 120
        render = CALIBRATION["render_preview"]
        for disparity in (-5, -1, 0, 1, 5):
            left, right = render(frame, 160, 80, disparity, 0, False)
            left_shift, right_shift = CALIBRATION["split_shift"](disparity)
            self.assertEqual(left_shift - right_shift, disparity)
            for image, pixels, value in ((left, left_shift, 80),
                                         (right, right_shift, 120)):
                if pixels >= 0:
                    self.assertTrue(np.all(image[:, :pixels] == 0))
                    self.assertTrue(np.all(image[:, pixels:] == value))
                else:
                    self.assertTrue(np.all(image[:, :160 + pixels] == value))
                    self.assertTrue(np.all(image[:, 160 + pixels:] == 0))

    def test_hud_disparity_is_independent_of_video_shift(self):
        draw = CALIBRATION["draw_reference_icons"]
        left = np.zeros((360, 640, 3), dtype=np.uint8)
        right = left.copy()
        draw(left, 24, True)
        draw(right, 24, False)
        def green_center(image):
            green = (image[:, :, 1] > 140) & (image[:, :, 2] < 100)
            return float(np.mean(np.where(green)[1]))
        self.assertAlmostEqual(green_center(left) - green_center(right), 24, delta=0.5)

    def test_arrow_keys_adjust_the_two_independent_settings(self):
        controls = Controls()
        handler = CALIBRATION["CalibrationWindow"]._on_key_press
        for key in (Gdk.KEY_Right, Gdk.KEY_KP_Right, Gdk.KEY_Up):
            self.assertTrue(handler(controls, None, Event(key)))
        self.assertEqual(controls.video_shift.get_value(), 2)
        self.assertEqual(controls.widget_shift.get_value(), 1)
        for key in (Gdk.KEY_Left, Gdk.KEY_Down):
            handler(controls, None, Event(key))
        self.assertEqual(controls.video_shift.get_value(), 1)
        self.assertEqual(controls.widget_shift.get_value(), 0)

    def test_gstreamer_capture_times_out_and_reconnects(self):
        source = "videotestsrc num-buffers=1 ! video/x-raw,width=64,height=32"
        capture = CALIBRATION["open_capture"](source, timeout=2)
        try:
            ok, frame = capture.read()
            self.assertTrue(ok)
            self.assertEqual(frame.shape, (32, 64, 3))
            started = time.monotonic()
            ok, _ = capture.read(timeout=0.1)
            self.assertFalse(ok)
            self.assertLess(time.monotonic() - started, 0.5)
            capture.reconnect()
            ok, frame = capture.read(timeout=1)
            self.assertTrue(ok)
            self.assertEqual(frame.shape, (32, 64, 3))
        finally:
            capture.release()

    def test_legacy_widget_offset_uses_crop_scale(self):
        config = {"display_horizontal_offset_px": 5, "preserve_size": True,
                  "camera": {"size": {"width": 300}, "crop": {"width": 200}}}
        self.assertEqual(CALIBRATION["legacy_widget_disparity"](config, 300), -8)


if __name__ == "__main__":
    unittest.main()
