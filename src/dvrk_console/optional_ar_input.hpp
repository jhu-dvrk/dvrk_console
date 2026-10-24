#ifndef DVRK_CONSOLE_OPTIONAL_AR_INPUT_HPP
#define DVRK_CONSOLE_OPTIONAL_AR_INPUT_HPP

#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <glibmm/main.h>
#include <dvrk_data/dvrk_gst_socket.hpp>
#include <algorithm>
#include "gst_pipeline_runtime.hpp"

namespace dvrk_console {
// A missing AR producer supplies transparent frames. Socket inputs are opened
// only while their producer exists, so the display can start before the panel.
class OptionalARInput {
public:
  ~OptionalARInput() { stop(); }
  static std::string source(const std::string &name) {
    return "appsrc name=" + name +
        " is-live=true format=time do-timestamp=true block=false"
        " max-buffers=2 max-bytes=0 leaky-type=downstream";
  }
  void start(GstElement *display, const std::string &name,
             const std::string &input, int width, int height, rclcpp::Node *node) {
    stop();
    m_source = gst_bin_get_by_name(GST_BIN(display), name.c_str());
    if (!m_source) return;
    m_name = name; m_input = input; m_width = width; m_height = height; m_node = node;
    if (dvrk_gst::is_socket_reference(input)) m_socket = dvrk_gst::resolve(input);
    GstCaps *caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "RGBA",
        "width", G_TYPE_INT, width, "height", G_TYPE_INT, height,
        "framerate", GST_TYPE_FRACTION, 30, 1,
        "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1, nullptr);
    gst_app_src_set_caps(GST_APP_SRC(m_source), caps);
    gst_caps_unref(caps);
    m_blank = gst_buffer_new_allocate(nullptr, static_cast<gsize>(width) * height * 4, nullptr);
    gst_buffer_memset(m_blank, 0, 0, gst_buffer_get_size(m_blank));
    if (m_socket.empty()) start_producer();
    m_timer = Glib::signal_timeout().connect([this] { return tick(); }, 33);
  }
  void stop() {
    m_timer.disconnect();
    stop_producer();
    if (m_source) gst_object_unref(m_source);
    if (m_blank) gst_buffer_unref(m_blank);
    m_source = nullptr; m_blank = nullptr; m_node = nullptr;
    m_input.clear(); m_socket.clear(); m_name.clear();
    m_last_socket_check = 0;
  }
private:
  void start_producer() {
    const std::string description = dvrk_gst::build_input(m_input, dvrk_gst::ROLE_STEREO_DISPLAY) +
        " ! queue max-size-buffers=1 max-size-time=0 max-size-bytes=0 leaky=downstream"
        " ! videoconvert ! videoscale ! video/x-raw,format=RGBA,width=" + std::to_string(m_width) +
        ",height=" + std::to_string(m_height) + ",pixel-aspect-ratio=1/1"
        " ! appsink name=ar_input sync=false max-buffers=1 drop=true";
    GError *error = nullptr;
    m_pipeline = gst_parse_launch(description.c_str(), &error);
    if (error || !m_pipeline) {
      RCLCPP_ERROR(m_node->get_logger(), "Optional AR input %s unavailable: %s", m_name.c_str(),
                   error ? error->message : "invalid pipeline");
      if (error) g_error_free(error);
      if (m_pipeline) { gst_object_unref(m_pipeline); m_pipeline = nullptr; }
      return;
    }
    m_sink = gst_bin_get_by_name(GST_BIN(m_pipeline), "ar_input");
    m_runtime.start(m_pipeline, m_node, m_name);
    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
  }
  void stop_producer() {
    m_runtime.stop();
    if (m_pipeline) { gst_element_set_state(m_pipeline, GST_STATE_NULL); gst_object_unref(m_pipeline); }
    if (m_sink) gst_object_unref(m_sink);
    if (m_latest) gst_sample_unref(m_latest);
    m_pipeline = m_sink = nullptr; m_latest = nullptr; m_received = 0;
  }
  bool tick() {
    const gint64 now = g_get_monotonic_time();
    if (!m_socket.empty() && now - m_last_socket_check >= 500 * 1000) {
      m_last_socket_check = now;
      const auto sockets = dvrk_gst::list_sockets();
      const bool available = std::find(sockets.begin(), sockets.end(), m_socket) != sockets.end();
      if (available && !m_pipeline) start_producer();
      else if (!available && m_pipeline) stop_producer();
    }
    auto *sample = m_sink ? gst_app_sink_try_pull_sample(GST_APP_SINK(m_sink), 0) : nullptr;
    if (sample) {
      if (m_latest) gst_sample_unref(m_latest);
      m_latest = sample; m_received = now;
    }
    GstBuffer *frame = m_latest && now - m_received < G_USEC_PER_SEC
        ? gst_sample_get_buffer(m_latest) : m_blank;
    auto *buffer = gst_buffer_copy(frame);
    GST_BUFFER_PTS(buffer) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_DURATION(buffer) = GST_SECOND / 30;
    gst_app_src_push_buffer(GST_APP_SRC(m_source), buffer);
    return true;
  }
  GstElement *m_pipeline = nullptr, *m_sink = nullptr, *m_source = nullptr;
  GstSample *m_latest = nullptr;
  GstBuffer *m_blank = nullptr;
  gint64 m_received = 0, m_last_socket_check = 0;
  int m_width = 0, m_height = 0;
  rclcpp::Node *m_node = nullptr;
  std::string m_name, m_input, m_socket;
  GstPipelineRuntime m_runtime;
  sigc::connection m_timer;
};
} // namespace dvrk_console
#endif
