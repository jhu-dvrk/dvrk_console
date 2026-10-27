#include "widget_disparity.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  using dvrk_console::WidgetDisparity;
  auto node = std::make_shared<rclcpp::Node>("disparity_owner");
  dvrk_data::AppConfig config;
  config.original_width = 320;
  config.widget_horizontal_disparity_px = -18;
  auto overlay = std::make_shared<dvrk_console::OverlayState>();
  int moved = 0;
  WidgetDisparity disparity(*node, config, overlay, [&](int value) { moved = value; });
  check(config.widget_horizontal_disparity_px == -18 && overlay->display_horizontal_offset_px == 18,
        "Configured startup disparity did not reach overlay");
  auto observer = std::make_shared<rclcpp::Node>("disparity_observer");
  int received = 999, messages = 0;
  auto sub = observer->create_subscription<std_msgs::msg::Int32>(
      "/stereo_display/widget_horizontal_disparity_px", rclcpp::QoS(1).reliable().transient_local(),
      [&](const std_msgs::msg::Int32::SharedPtr value) { received = value->data; ++messages; });
  auto wait_for = [&](int expected) {
    for (int i = 0; i < 200; ++i) {
      rclcpp::spin_some(observer);
      if (received == expected) return true;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
  };
  check(wait_for(-18), "Late subscriber missed initial disparity");
  auto set = [&](int64_t value) { return node->set_parameter(rclcpp::Parameter("widget_horizontal_disparity_px", value)); };
  check(set(17).successful && moved == 17 && config.widget_horizontal_disparity_px == 17 &&
        overlay->display_horizontal_offset_px == -17 && wait_for(17), "Live disparity did not propagate");
  const int before = messages;
  check(!set(320).successful && !set(-320).successful && !set(INT64_MAX).successful,
        "Accepted out-of-range disparity");
  check(!node->set_parameter(rclcpp::Parameter("widget_horizontal_disparity_px", 2.5)).successful,
        "Accepted floating-point disparity");
  auto reject = node->add_on_set_parameters_callback([](const auto &) {
    rcl_interfaces::msg::SetParametersResult result;
    result.successful = false;
    result.reason = "test rejection";
    return result;
  });
  check(!set(10).successful, "Additional validator did not reject update");
  for (int i = 0; i < 20; ++i) {
    rclcpp::spin_some(observer);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  check(messages == before && received == 17 && moved == 17 && config.widget_horizontal_disparity_px == 17 &&
        overlay->display_horizontal_offset_px == -17 &&
        node->get_parameter("widget_horizontal_disparity_px").as_int() == 17,
        "Rejected parameter change had side effects");

  dvrk_data::AppConfig legacy;
  legacy.original_width = 640; legacy.crop_width = 320;
  legacy.legacy_display_horizontal_offset = true;
  legacy.display_horizontal_offset_px = 9;
  auto legacy_node = std::make_shared<rclcpp::Node>("legacy_disparity",
      rclcpp::NodeOptions().arguments({"--ros-args", "-r",
        "/stereo_display/widget_horizontal_disparity_px:=/test/legacy_disparity"}));
  auto legacy_overlay = std::make_shared<dvrk_console::OverlayState>();
  WidgetDisparity legacy_value(*legacy_node, legacy, legacy_overlay, [](int) {});
  check(legacy.widget_horizontal_disparity_px == -18 && legacy_overlay->display_horizontal_offset_px == 18,
        "Legacy sign/scale conversion incorrect");
  auto override_node = std::make_shared<rclcpp::Node>("override_disparity",
      rclcpp::NodeOptions().parameter_overrides({rclcpp::Parameter("widget_horizontal_disparity_px", 11)}));
  WidgetDisparity override_value(*override_node, legacy, legacy_overlay, [](int) {});
  check(legacy.widget_horizontal_disparity_px == 11 && legacy_overlay->display_horizontal_offset_px == -11,
        "ROS startup override did not beat legacy config");
  rclcpp::shutdown();
  std::cout << "Startup/legacy/override disparity, live updates, rejected changes and late joining passed\n";
}
