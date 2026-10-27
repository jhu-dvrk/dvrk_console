#include "annotation_panel.hpp"
#include "glib_ros_executor.hpp"
#include <dvrk_data/dvrk_gst_socket.hpp>
#include <unistd.h>
#include <iostream>
#include <stdexcept>

void check(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
}
void pump(int ms) {
  const auto until = g_get_monotonic_time() + ms * 1000;
  while (g_get_monotonic_time() < until) {
    while (g_main_context_iteration(nullptr, false)) {}
    g_usleep(1000);
  }
}
int main(int argc, char **argv) {
  gst_init(&argc, &argv); rclcpp::init(argc, argv);
  if (!gtk_init_check(&argc, &argv)) return 77;
  Gtk::Main gtk(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("annotation_panel_test");
  dvrk_console::GlibRosExecutor executor(node);
  auto disparity_pub = node->create_publisher<std_msgs::msg::Int32>(
      "/stereo_display/widget_horizontal_disparity_px", rclcpp::QoS(1).reliable().transient_local());
  auto set_disparity = [&](int value) {
    std_msgs::msg::Int32 message; message.data = value; disparity_pub->publish(message);
  };
  set_disparity(20); // Publish before the panel subscribes: exercise latching.
  dvrk_console::AnnotationConfig config;
  config.gst_input = "videotestsrc is-live=true pattern=smpte ! video/x-raw,width=640,height=240,framerate=30/1";
  config.gst_output = "@dvrk:test:annotations_" + std::to_string(getpid());
  {
    dvrk_console::AnnotationPanel panel(node.get(), config);
    Gtk::OffscreenWindow window;
    window.set_default_size(800, 600); window.add(panel); window.show_all();
    panel.set_enabled(false);
    panel.start(); pump(100);
    const auto inactive_sockets = dvrk_gst::list_sockets();
    check(std::find(inactive_sockets.begin(), inactive_sockets.end(), config.gst_output) == inactive_sockets.end(),
          "Annotations started while touchscreen mode was disabled");
    panel.set_enabled(true); pump(1000);
    GError *error = nullptr;
    auto *consumer = gst_parse_launch((dvrk_gst::build_src(config.gst_output) +
        " ! appsink name=result sync=false max-buffers=1 drop=true").c_str(), &error);
    check(!error && consumer, "Cannot build output consumer");
    auto *sink = gst_bin_get_by_name(GST_BIN(consumer), "result");
    gst_element_set_state(consumer, GST_STATE_PLAYING);
    auto alpha = [&](unsigned expected_eyes) {
      for (int i = 0; i < 100; ++i) {
        pump(20);
        auto *sample = gst_app_sink_try_pull_sample(GST_APP_SINK(sink), 0);
        if (!sample) continue;
        GstVideoInfo info;
        check(gst_video_info_from_caps(&info, gst_sample_get_caps(sample)), "Missing video caps");
        check(GST_VIDEO_INFO_WIDTH(&info) == 640 && GST_VIDEO_INFO_HEIGHT(&info) == 240,
              "Output dimensions do not match stereo input");
        check(GST_VIDEO_INFO_FORMAT(&info) == GST_VIDEO_FORMAT_BGRA, "Output lost alpha format");
        GstMapInfo map;
        check(gst_buffer_map(gst_sample_get_buffer(sample), &map, GST_MAP_READ), "Map failed");
        unsigned eyes = 0;
        const int stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
        for (int y = 0; y < 240; ++y)
          for (int x = 0; x < 640; ++x)
            if (map.data[y * stride + x * 4 + 3]) eyes |= x < 320 ? 1 : 2;
        gst_buffer_unmap(gst_sample_get_buffer(sample), &map); gst_sample_unref(sample);
        if (eyes == expected_eyes) return true;
      }
      return false;
    };
    check(alpha(0), "Empty panel did not publish transparent output");
    Gtk::DrawingArea *canvas = nullptr;
    for (auto *child : panel.get_children()) if (auto *area = dynamic_cast<Gtk::DrawingArea*>(child)) canvas = area;
    check(canvas, "No annotation canvas");
    auto send = [&](GdkEventType type, double x, double y) {
      auto *event = gdk_event_new(type);
      event->any.window = GDK_WINDOW(g_object_ref(canvas->get_window()->gobj()));
      event->any.send_event = true;
      if (type == GDK_MOTION_NOTIFY) {
        event->motion.x = x; event->motion.y = y; event->motion.state = GDK_BUTTON1_MASK;
      } else {
        event->button.x = x; event->button.y = y; event->button.button = 1;
        event->button.state = type == GDK_BUTTON_RELEASE ? GDK_BUTTON1_MASK : 0;
      }
      gdk_event_set_device(event, gdk_seat_get_pointer(gdk_display_get_default_seat(gdk_display_get_default())));
      gtk_main_do_event(event); gdk_event_free(event); pump(30);
    };
    const double x = canvas->get_allocated_width() / 2.0, y = canvas->get_allocated_height() / 2.0;
    send(GDK_BUTTON_PRESS, x, y);
    send(GDK_MOTION_NOTIFY, x + 30, y);
    send(GDK_BUTTON_RELEASE, x + 30, y);
    check(alpha(1), "Pointer stroke did not reach the output socket");
    Gtk::CheckButton *both = nullptr;
    Gtk::ComboBoxText *eye = nullptr;
    Gtk::Button *clear = nullptr;
    for (auto *child : panel.get_children()) {
      if (auto *box = dynamic_cast<Gtk::Box*>(child)) {
        for (auto *widget : box->get_children()) {
          if (auto *button = dynamic_cast<Gtk::Button*>(widget))
            if (button->get_label() == "Clear all") clear = button;
          if (auto *button = dynamic_cast<Gtk::CheckButton*>(widget))
            if (button->get_label() == "Both eyes") both = button;
          if (auto *combo = dynamic_cast<Gtk::ComboBoxText*>(widget))
            if (combo->get_active_text() == "Left eye") eye = combo;
        }
      }
    }
    check(both && eye && clear, "Missing annotation controls");
    clear->clicked();
    check(alpha(0), "Clear all left opaque pixels on the socket");
    auto tap_positions = [&](int left_x, int right_x, int old_right_x) {
      for (int i = 0; i < 100; ++i) {
        pump(20);
        auto *sample = gst_app_sink_try_pull_sample(GST_APP_SINK(sink), 0);
        if (!sample) continue;
        GstVideoInfo info;
        gst_video_info_from_caps(&info, gst_sample_get_caps(sample));
        GstMapInfo map;
        check(gst_buffer_map(gst_sample_get_buffer(sample), &map, GST_MAP_READ), "Cannot map tap output");
        const int stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
        auto opaque = [&](int px) { return map.data[120 * stride + px * 4 + 3] != 0; };
        const bool matches = opaque(left_x) && opaque(320 + right_x) && !opaque(320 + old_right_x);
        gst_buffer_unmap(gst_sample_get_buffer(sample), &map);
        gst_sample_unref(sample);
        if (matches) return true;
      }
      return false;
    };
    both->set_active(true);
    send(GDK_BUTTON_PRESS, x, y); send(GDK_BUTTON_RELEASE, x, y);
    check(tap_positions(160, 140, 160), "Late-joined disparity did not position both-eye tap");
    set_disparity(-20);
    check(tap_positions(160, 180, 140), "Live disparity did not reposition existing drawing");
    clear->clicked(); check(alpha(0), "Both-eye clear failed");
    eye->set_active(1);
    send(GDK_BUTTON_PRESS, x, y); send(GDK_BUTTON_RELEASE, x, y);
    check(tap_positions(140, 160, 180), "Right-eye source used the wrong disparity direction");
    clear->clicked(); check(alpha(0), "Cannot clear right-source drawing");
    eye->set_active(0); both->set_active(false);
    send(GDK_BUTTON_PRESS, x, y);
    send(GDK_BUTTON_RELEASE, x, y);
    check(alpha(1), "Cannot draw after clearing");
    panel.set_enabled(false);
    check(alpha(0), "Disabling touchscreen mode left visible drawings");
    panel.set_enabled(true); pump(300);
    send(GDK_BUTTON_PRESS, x, y);
    send(GDK_BUTTON_RELEASE, x, y);
    check(alpha(1), "Re-enabling touchscreen mode did not restart annotation input");
    gst_element_set_state(consumer, GST_STATE_NULL); gst_object_unref(sink); gst_object_unref(consumer);
    window.remove();
  }
  rclcpp::shutdown();
  std::cout << "Embedded panel preview, socket dimensions, pointer strokes and clear passed\n";
}
