#ifndef DVRK_CONSOLE_WIDGET_DISPARITY_HPP
#define DVRK_CONSOLE_WIDGET_DISPARITY_HPP

#include "overlay_state.hpp"
#include <dvrk_data/config.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/int32.hpp>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>

namespace dvrk_console {

// Owns the display parameter and its resolved, latched state. The owner runs
// parameter services on the GLib executor, serializing layout changes/rebuilds.
class WidgetDisparity {
public:
  WidgetDisparity(rclcpp::Node &node, dvrk_data::AppConfig &config,
                  std::shared_ptr<OverlayState> overlay,
                  std::function<void(int)> update_positions)
      : m_config(config), m_overlay(std::move(overlay)) {
    if (config.original_width <= 0) throw std::runtime_error("Invalid per-eye width");
    double initial = config.widget_horizontal_disparity_px;
    if (config.legacy_display_horizontal_offset) {
      const double scale = config.preserve_size && config.crop_width > 0
          ? static_cast<double>(config.original_width) / config.crop_width : 1.0;
      initial = -std::round(config.display_horizontal_offset_px * scale);
    }
    if (!std::isfinite(initial) || initial < std::numeric_limits<int>::min() ||
        initial > std::numeric_limits<int>::max())
      throw std::runtime_error("Widget disparity is outside the supported integer range");

    rcl_interfaces::msg::ParameterDescriptor descriptor;
    descriptor.description = "Signed widget disparity in final per-eye output pixels; positive appears nearer";
    rcl_interfaces::msg::IntegerRange range;
    range.from_value = -static_cast<int64_t>(config.original_width) + 1;
    range.to_value = static_cast<int64_t>(config.original_width) - 1;
    range.step = 1;
    descriptor.integer_range.push_back(range);
    const auto value = node.declare_parameter<int64_t>(
        "widget_horizontal_disparity_px", static_cast<int64_t>(initial), descriptor);
    m_publisher = node.create_publisher<std_msgs::msg::Int32>(
        "/stereo_display/widget_horizontal_disparity_px",
        rclcpp::QoS(1).reliable().transient_local());
    apply(static_cast<int>(value));
    m_callback = node.add_post_set_parameters_callback(
        [this, update_positions = std::move(update_positions)](const auto &parameters) {
          for (const auto &parameter : parameters) {
            if (parameter.get_name() != "widget_horizontal_disparity_px") continue;
            const int disparity = static_cast<int>(parameter.as_int());
            update_positions(disparity);
            apply(disparity);
          }
        });
  }

private:
  void apply(int disparity) {
    m_config.widget_horizontal_disparity_px = disparity;
    {
      std::lock_guard<std::mutex> lock(m_overlay->mutex);
      m_overlay->display_horizontal_offset_px = -disparity;
    }
    std_msgs::msg::Int32 value;
    value.data = disparity;
    m_publisher->publish(value);
  }
  dvrk_data::AppConfig &m_config;
  std::shared_ptr<OverlayState> m_overlay;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr m_publisher;
  rclcpp::node_interfaces::PostSetParametersCallbackHandle::SharedPtr m_callback;
};
} // namespace dvrk_console
#endif
