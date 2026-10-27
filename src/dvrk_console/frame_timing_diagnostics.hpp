#ifndef DVRK_CONSOLE_FRAME_TIMING_DIAGNOSTICS_HPP
#define DVRK_CONSOLE_FRAME_TIMING_DIAGNOSTICS_HPP

#include "frame_timing.hpp"
#include <diagnostic_msgs/msg/diagnostic_array.hpp>
#include <iomanip>
#include <sstream>

namespace dvrk_console {
inline diagnostic_msgs::msg::DiagnosticArray frame_timing_diagnostics(
    const FrameTiming::Snapshot &timing, const std::string &node_name,
    const std::string &hardware_id) {
  using diagnostic_msgs::msg::DiagnosticStatus;
  diagnostic_msgs::msg::DiagnosticArray message;
  auto value = [](DiagnosticStatus &status, const char *key, std::optional<double> number) {
    diagnostic_msgs::msg::KeyValue item;
    item.key = key;
    std::ostringstream text;
    if (number) text << std::fixed << std::setprecision(3) << *number;
    else text << "unavailable";
    item.value = text.str();
    status.values.push_back(std::move(item));
  };
  for (unsigned side = 0; side < 2; ++side) {
    const auto &eye = timing.eyes[side];
    DiagnosticStatus status;
    status.name = node_name + (side ? ": video/right" : ": video/left");
    status.hardware_id = hardware_id;
    status.level = !eye.current ? DiagnosticStatus::WARN
        : eye.stale ? DiagnosticStatus::STALE : DiagnosticStatus::OK;
    status.message = !eye.current ? "Source timestamps unavailable"
        : eye.stale ? "Source frames stale" : "Receiving source frames";
    value(status, "current_age_ms", eye.current);
    value(status, "average_age_ms", eye.average);
    value(status, "max_age_ms", eye.maximum);
    value(status, "peak_age_ms", eye.peak);
    value(status, "fps", eye.current ? std::optional<double>(eye.fps) : std::nullopt);
    value(status, "window_seconds", 5.0);
    message.status.push_back(std::move(status));
  }
  DiagnosticStatus delta;
  delta.name = node_name + ": video/stereo_delta";
  delta.hardware_id = hardware_id;
  delta.level = !timing.delta.current ? DiagnosticStatus::WARN
      : timing.eyes[0].stale || timing.eyes[1].stale ? DiagnosticStatus::STALE : DiagnosticStatus::OK;
  delta.message = !timing.delta.current ? "Stereo timestamps unavailable"
      : delta.level == DiagnosticStatus::STALE ? "Source frames stale" : "Stereo source timestamps available";
  value(delta, "current_left_minus_right_ms", timing.delta.current);
  value(delta, "average_absolute_delta_ms", timing.delta.average);
  value(delta, "max_absolute_delta_ms", timing.delta.maximum);
  message.status.push_back(std::move(delta));
  return message;
}
} // namespace dvrk_console
#endif
