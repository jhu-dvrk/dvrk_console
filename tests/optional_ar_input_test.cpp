#include "optional_ar_input.hpp"
#include <dvrk_data/dvrk_gst_socket.hpp>
#include <unistd.h>
#include <iostream>
#include <stdexcept>

void check(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}
GstElement *pipeline(const std::string &description) {
  GError *error = nullptr;
  auto *result = gst_parse_launch(description.c_str(), &error);
  if (error) throw std::runtime_error(error->message);
  check(result, "Pipeline missing"); return result;
}
int main(int argc, char **argv) {
  gst_init(&argc, &argv); rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("optional_ar_test");
  const std::string socket = "@dvrk:test:ar_" + std::to_string(getpid());
  auto *display = pipeline(dvrk_console::OptionalARInput::source("ar") +
      " ! appsink name=result sync=false max-buffers=1 drop=true");
  auto *sink = gst_bin_get_by_name(GST_BIN(display), "result");
  dvrk_console::OptionalARInput input;
  input.start(display, "ar", socket, 64, 48, node.get());
  gst_element_set_state(display, GST_STATE_PLAYING);
  auto wait_pixel = [&](bool opaque, int timeout_ms) {
    const auto until = g_get_monotonic_time() + timeout_ms * 1000;
    while (g_get_monotonic_time() < until) {
      while (g_main_context_iteration(nullptr, false)) {}
      auto *sample = gst_app_sink_try_pull_sample(GST_APP_SINK(sink), GST_MSECOND * 10);
      if (!sample) continue;
      GstMapInfo map;
      check(gst_buffer_map(gst_sample_get_buffer(sample), &map, GST_MAP_READ), "Map failed");
      const bool match = map.size == 64 * 48 * 4 &&
          (opaque ? (map.data[0] == 255 && map.data[1] == 0 && map.data[2] == 0 && map.data[3] == 255)
                  : map.data[3] == 0);
      gst_buffer_unmap(gst_sample_get_buffer(sample), &map); gst_sample_unref(sample);
      if (match) return true;
    }
    return false;
  };
  check(wait_pixel(false, 2000), "Missing producer did not yield transparent frames");
  auto *producer = pipeline("videotestsrc is-live=true pattern=solid-color foreground-color=0xffff0000"
      " ! video/x-raw,format=BGRA,width=64,height=48,framerate=30/1 ! " + dvrk_gst::build_sink(socket));
  gst_element_set_state(producer, GST_STATE_PLAYING);
  check(wait_pixel(true, 5000), "Late BGRA producer did not connect with preserved alpha/color");
  gst_element_set_state(producer, GST_STATE_NULL);
  check(wait_pixel(false, 3000), "Disconnect left stale AR pixels");
  gst_element_set_state(producer, GST_STATE_PLAYING);
  check(wait_pixel(true, 5000), "Producer failed to reconnect");
  input.stop();
  gst_element_set_state(producer, GST_STATE_NULL); gst_object_unref(producer);
  gst_element_set_state(display, GST_STATE_NULL); gst_object_unref(sink); gst_object_unref(display);
  rclcpp::shutdown();
  std::cout << "Optional AR transparency, late startup, disconnect and reconnect passed\n";
}
