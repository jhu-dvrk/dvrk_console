#include "frame_timing.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
void check(bool good, const char *message) { if (!good) throw std::runtime_error(message); }
void close_to(const std::optional<double> &value, double expected, const char *message) {
  check(value && std::abs(*value - expected) < 0.001, message);
}
int main() {
  using dvrk_console::FrameTiming;
  FrameTiming stats(0);
  DcFrameTimestamps times;
  times.left_source_ts = 10 * GST_SECOND - 100 * GST_MSECOND;
  times.right_source_ts = 10 * GST_SECOND - 50 * GST_MSECOND;
  stats.observe(times, 3, 10 * GST_SECOND, 0);
  stats.observe(times, 3, 10 * GST_SECOND + 200 * GST_MSECOND, 200 * GST_MSECOND);
  auto result = stats.snapshot(GST_SECOND);
  close_to(result.eyes[0].current, 1100, "Current age must grow without new buffers");
  close_to(result.eyes[0].average, 200, "Incorrect rolling average");
  close_to(result.eyes[0].maximum, 300, "Incorrect rolling maximum");
  close_to(result.eyes[0].peak, 1100, "Peak must include a stalled frame");
  check(result.eyes[0].stale && result.eyes[0].fps == 1.0, "Repeated buffers counted as fresh source frames");
  close_to(result.delta.current, -50, "Signed L minus R is incorrect");
  close_to(result.delta.average, 50, "Delta mean must use absolute differences");
  result = stats.snapshot(6 * GST_SECOND);
  check(!result.eyes[0].average && !result.eyes[0].maximum && result.eyes[0].fps == 0,
        "Rolling window retained expired samples");
  close_to(result.eyes[0].current, 6100, "A stall must remain visible after window expiration");
  stats.reset(6 * GST_SECOND);
  times.left_source_ts = 16 * GST_SECOND - 10 * GST_MSECOND;
  stats.observe(times, 1, 16 * GST_SECOND, 6 * GST_SECOND);
  result = stats.snapshot(6 * GST_SECOND + 100 * GST_MSECOND);
  close_to(result.eyes[0].peak, 110, "Reset retained old peak");
  close_to(result.eyes[1].current, 6150, "Updating left eye changed the right eye's displayed age");
  times.left_source_ts = 0;
  stats.observe(times, 1, 17 * GST_SECOND, 7 * GST_SECOND);
  result = stats.snapshot(7 * GST_SECOND);
  check(!result.eyes[0].current && !result.delta.current, "Missing timestamps became zero-age video");
  times.left_source_ts = 18 * GST_SECOND;
  stats.observe(times, 1, 17 * GST_SECOND, 7 * GST_SECOND);
  check(!stats.snapshot(7 * GST_SECOND).eyes[0].current, "Future wall-clock timestamps became negative ages");
  std::cout << "Frame age, rolling statistics, distinct FPS, stalls, reset, missing times and signed channel delta passed\n";
}
