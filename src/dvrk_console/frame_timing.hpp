#ifndef DVRK_CONSOLE_FRAME_TIMING_HPP
#define DVRK_CONSOLE_FRAME_TIMING_HPP

#include <dvrk_data/cpu_timestamp_meta.hpp>
#include <array>
#include <deque>
#include <mutex>
#include <optional>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace dvrk_console {
inline gint64 timing_monotonic_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Samples describe arrival at the display sink, not physical monitor scanout.
// Rolling means/maxima are per submitted buffer. FPS counts distinct source
// timestamps. Current age and the resettable peak continue growing on a stall.
class FrameTiming {
public:
  struct Summary {
    std::optional<double> current, average, maximum, peak;
    double fps = 0.0;
    bool stale = false;
  };
  struct Snapshot {
    std::array<Summary, 2> eyes;
    Summary delta;
  };
  explicit FrameTiming(gint64 now = timing_monotonic_ns()) : m_reset_at(now) {}

  void observe(const DcFrameTimestamps &timestamps, unsigned mask,
               gint64 wall_now = dc_clock_realtime_ns(),
               gint64 now = timing_monotonic_ns()) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::array<gint64, 2> origins{timestamps.left_source_ts, timestamps.right_source_ts};
    for (unsigned side = 0; side < 2; ++side) {
      if (!(mask & (1u << side))) continue;
      auto &eye = m_eyes[side];
      const auto origin = origins[side];
      eye.available = origin > 0 && origin <= wall_now;
      if (!eye.available) { eye.origin = 0; continue; }
      if (origin != eye.origin) {
        eye.updates.push_back(now);
        eye.changed_at = now;
      }
      eye.origin = origin;
      eye.arrived_at = now;
      eye.age_at_arrival = static_cast<double>(wall_now - origin) / 1e6;
      eye.samples.push_back({now, eye.age_at_arrival});
      update_peak(eye.peak, eye.age_at_arrival);
      prune(eye.samples, now);
      prune(eye.updates, now);
    }
    if (m_eyes[0].available && m_eyes[1].available) {
      const double delta = static_cast<double>(m_eyes[0].origin - m_eyes[1].origin) / 1e6;
      m_deltas.push_back({now, std::abs(delta)});
      prune(m_deltas, now);
    }
  }

  Snapshot snapshot(gint64 now = timing_monotonic_ns()) {
    std::lock_guard<std::mutex> lock(m_mutex);
    Snapshot result;
    const double seconds = std::max(0.001, std::min(5.0, static_cast<double>(now - m_reset_at) / 1e9));
    for (unsigned side = 0; side < 2; ++side) {
      auto &eye = m_eyes[side];
      auto &summary = result.eyes[side];
      prune(eye.samples, now);
      prune(eye.updates, now);
      summarize(eye.samples, summary);
      if (eye.available) {
        summary.current = eye.age_at_arrival + static_cast<double>(now - eye.arrived_at) / 1e6;
        summary.fps = eye.updates.size() / seconds;
        summary.stale = now - eye.changed_at >= 500 * GST_MSECOND;
        update_peak(eye.peak, *summary.current);
      }
      summary.peak = eye.peak;
    }
    prune(m_deltas, now);
    summarize(m_deltas, result.delta);
    if (m_eyes[0].available && m_eyes[1].available)
      result.delta.current = static_cast<double>(m_eyes[0].origin - m_eyes[1].origin) / 1e6;
    return result;
  }

  void reset(gint64 now = timing_monotonic_ns()) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_reset_at = now;
    m_deltas.clear();
    for (auto &eye : m_eyes) {
      eye.samples.clear();
      eye.updates.clear();
      eye.peak.reset();
    }
  }

private:
  struct Point { gint64 at; double value; };
  struct Eye {
    gint64 origin = 0, arrived_at = 0, changed_at = 0;
    double age_at_arrival = 0;
    bool available = false;
    std::deque<Point> samples;
    std::deque<gint64> updates;
    std::optional<double> peak;
  };
  static void prune(std::deque<Point> &points, gint64 now) {
    while (!points.empty() && points.front().at <= now - 5 * GST_SECOND) points.pop_front();
  }
  static void prune(std::deque<gint64> &points, gint64 now) {
    while (!points.empty() && points.front() <= now - 5 * GST_SECOND) points.pop_front();
  }
  static void update_peak(std::optional<double> &peak, double value) {
    peak = peak ? std::max(*peak, value) : value;
  }
  static void summarize(const std::deque<Point> &points, Summary &summary) {
    if (points.empty()) return;
    double sum = 0, maximum = 0;
    for (const auto &point : points) { sum += point.value; maximum = std::max(maximum, point.value); }
    summary.average = sum / points.size();
    summary.maximum = maximum;
  }
  std::array<Eye, 2> m_eyes;
  std::deque<Point> m_deltas;
  gint64 m_reset_at;
  std::mutex m_mutex;
};
} // namespace dvrk_console
#endif
