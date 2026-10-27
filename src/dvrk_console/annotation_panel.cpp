#include "annotation_panel.hpp"
#include <dvrk_data/dvrk_gst_socket.hpp>

namespace dvrk_console {
AnnotationPanel::AnnotationPanel(rclcpp::Node *node, const AnnotationConfig &config)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 4), m_node(node), m_config(config) {
  m_disparity_sub = m_node->create_subscription<std_msgs::msg::Int32>(
      "/stereo_display/widget_horizontal_disparity_px",
      rclcpp::QoS(1).reliable().transient_local(),
      [this](const std_msgs::msg::Int32::SharedPtr value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_model.disparity_px = value->data;
      });
  m_eye.append("Left eye"); m_eye.append("Right eye"); m_eye.set_active(0);
  m_color.append("Yellow"); m_color.append("Red"); m_color.append("Green");
  m_color.append("White"); m_color.set_active(0);
  m_width.append("Fine"); m_width.append("Medium"); m_width.append("Thick");
  m_width.set_active(1);
  for (auto *choice : {&m_eye, &m_color, &m_width})
    choice->get_style_context()->add_class("annotation-choice");
  m_visible.set_active(true);
  for (auto *widget : std::vector<Gtk::Widget*>{&m_eye, &m_color, &m_width, &m_both}) {
    widget->set_size_request(-1, 44);
    m_tools.pack_start(*widget, Gtk::PACK_SHRINK);
  }
  for (auto *widget : std::vector<Gtk::Widget*>{&m_visible, &m_undo, &m_clear, &m_clear_all, &m_retry}) {
    widget->set_size_request(-1, 44);
    m_actions.pack_start(*widget, Gtk::PACK_SHRINK);
  }
  m_both.set_tooltip_text("Draw in both eyes at widget depth using the live display disparity; undo and clear affect both eyes");
  pack_start(m_tools, Gtk::PACK_SHRINK);
  pack_start(m_actions, Gtk::PACK_SHRINK);
  m_canvas.set_size_request(160, 120);
  m_canvas.set_hexpand(true); m_canvas.set_vexpand(true);
  pack_start(m_canvas, Gtk::PACK_EXPAND_WIDGET);
  m_canvas.signal_draw().connect(sigc::mem_fun(*this, &AnnotationPanel::draw));
  m_eye.signal_changed().connect([this] { m_drawing = false; m_canvas.queue_draw(); });
  m_visible.signal_toggled().connect([this] {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_model.visible = m_visible.get_active(); m_drawing = false;
    m_canvas.queue_draw();
  });
  m_undo.signal_clicked().connect([this] {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_drawing = false; m_model.undo(target_eyes()); m_canvas.queue_draw();
  });
  m_clear.signal_clicked().connect([this] {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_drawing = false; m_model.clear(target_eyes()); m_canvas.queue_draw();
  });
  m_clear_all.signal_clicked().connect([this] {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_drawing = false; m_model.clear(3); m_canvas.queue_draw();
  });
  m_retry.signal_clicked().connect([this] {
    stop_input();
    m_output_failed = false;
    start();
  });
  m_drag = Gtk::GestureDrag::create(m_canvas);
  m_drag->set_button(1);
  m_drag->signal_drag_begin().connect([this](double x, double y) {
    m_start_x = x; m_start_y = y; begin(x, y);
  });
  m_drag->signal_drag_update().connect([this](double x, double y) { extend(m_start_x + x, m_start_y + y); });
  m_drag->signal_drag_end().connect([this](double x, double y) {
    extend(m_start_x + x, m_start_y + y); m_drawing = false;
  });
}

AnnotationPanel::~AnnotationPanel() {
  m_timer.disconnect();
  stop_input();
  stop_output();
}

void AnnotationPanel::stop_input() {
  m_input_runtime.stop();
  if (m_input) { gst_element_set_state(m_input, GST_STATE_NULL); gst_object_unref(m_input); m_input = nullptr; }
  if (m_sink) { gst_object_unref(m_sink); m_sink = nullptr; }
  if (m_sample) { gst_sample_unref(m_sample); m_sample = nullptr; }
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_live = false; m_drawing = false;
  }
  m_last_frame = 0;
  m_canvas.queue_draw();
}

void AnnotationPanel::set_enabled(bool enabled) {
  const bool changed = m_enabled != enabled;
  m_enabled = enabled;
  set_sensitive(enabled);
  m_tools.set_sensitive(enabled);
  m_actions.set_sensitive(enabled);
  if (enabled) {
    if (changed) m_output_failed = false;
    start();
  } else if (changed || m_input) {
    stop_input();
    std::lock_guard<std::mutex> lock(m_mutex);
    m_model.clear(3);
    set_status("Annotations disabled");
  }
}

void AnnotationPanel::set_status(const std::string &status) {
  if (status == m_status_text) return;
  m_status_text = status;
  m_retry.set_tooltip_text(status);
  m_canvas.set_tooltip_text(status);
}

void AnnotationPanel::place_controls(Gtk::Box &container) {
  // Use compact labels when all tools share the narrow row below the preview.
  m_eye.remove_all(); m_eye.append("Left"); m_eye.append("Right"); m_eye.set_active(0);
  m_width.remove_all(); m_width.append("Fine"); m_width.append("Med");
  m_width.append("Thick"); m_width.set_active(1);
  // At the bottom edge of a small screen, a one-row popup avoids GTK's
  // scrolling menu arrows hiding the other options.
  m_eye.set_wrap_width(2);
  m_color.set_wrap_width(4);
  m_width.set_wrap_width(3);
  m_both.set_label("Both");
  m_visible.set_label("Show");
  m_clear_all.set_label("All");
  m_clear_all.set_tooltip_text("Clear drawings in both eyes");
  m_retry.set_label("Retry");
  m_eye.set_tooltip_text("Annotation channel: left or right eye");
  m_color.set_tooltip_text("Drawing color");
  m_width.set_tooltip_text("Stroke width");
  m_visible.set_tooltip_text("Show or hide drawings");

  // Preserve each widget while moving it from the standalone panel to the row.
  for (auto *parent : {&m_tools, &m_actions}) {
    for (auto *widget : parent->get_children()) {
      g_object_ref(widget->gobj());
      parent->remove(*widget);
      container.pack_start(*widget, Gtk::PACK_SHRINK);
      g_object_unref(widget->gobj());
    }
  }
}

void AnnotationPanel::start() {
  if (m_input || !m_enabled) return;
  GError *error = nullptr;
  const std::string description = dvrk_gst::build_input(m_config.gst_input, {}) +
      " ! queue max-size-buffers=1 max-size-time=0 max-size-bytes=0 leaky=downstream"
      " ! videoconvert ! video/x-raw,format=BGRx,pixel-aspect-ratio=1/1"
      " ! appsink name=preview sync=false max-buffers=1 drop=true";
  m_input = gst_parse_launch(description.c_str(), &error);
  if (error || !m_input) {
    set_status(std::string("Preview unavailable: ") + (error ? error->message : "invalid pipeline"));
    if (error) g_error_free(error);
    if (m_input) { gst_object_unref(m_input); m_input = nullptr; }
    return;
  }
  m_sink = gst_bin_get_by_name(GST_BIN(m_input), "preview");
  m_input_runtime.start(m_input, m_node, "control_panel_annotations");
  gst_element_set_state(m_input, GST_STATE_PLAYING);
  set_status("Waiting for overlay stereo video…");
  if (!m_timer.connected())
    m_timer = Glib::signal_timeout().connect(sigc::mem_fun(*this, &AnnotationPanel::tick), 33);
}

void AnnotationPanel::stop_output() {
  if (m_output) {
    gst_element_set_state(m_output, GST_STATE_NULL);
    gst_object_unref(m_output); m_output = nullptr;
  }
}

bool AnnotationPanel::start_output() {
  const std::string description = "videotestsrc pattern=solid-color foreground-color=0 background-color=0 is-live=true"
      " ! video/x-raw,format=BGRA,width=" + std::to_string(2 * m_eye_width) +
      ",height=" + std::to_string(m_eye_height) + ",framerate=30/1"
      " ! cairooverlay name=draw_stereo"
      " ! queue max-size-buffers=2 max-size-time=0 max-size-bytes=0 leaky=downstream ! " +
      dvrk_gst::build_sink(m_config.gst_output);
  GError *error = nullptr;
  m_output = gst_parse_launch(description.c_str(), &error);
  if (error || !m_output) {
    set_status(std::string("Drawing output unavailable: ") + (error ? error->message : "invalid pipeline"));
    if (error) g_error_free(error);
    stop_output(); return false;
  }
  auto *overlay = gst_bin_get_by_name(GST_BIN(m_output), "draw_stereo");
  g_signal_connect(overlay, "draw", G_CALLBACK(draw_stereo), this);
  gst_object_unref(overlay);
  if (gst_element_set_state(m_output, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
    set_status("Drawing output failed to start; check socket names, then retry");
    stop_output(); return false;
  }
  return true;
}

bool AnnotationPanel::tick() {
  if (m_output) {
    GstBus *bus = gst_element_get_bus(m_output);
    GstMessage *message = gst_bus_pop_filtered(bus, GstMessageType(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
    gst_object_unref(bus);
    if (message) {
      GError *error = nullptr; gchar *debug = nullptr;
      if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) gst_message_parse_error(message, &error, &debug);
      set_status(std::string("Drawing output stopped: ") + (error ? error->message : "end of stream"));
      if (error) g_error_free(error);
      g_free(debug); gst_message_unref(message);
      stop_output(); m_output_failed = true;
    }
  }
  GstSample *sample = m_sink ? gst_app_sink_try_pull_sample(GST_APP_SINK(m_sink), 0) : nullptr;
  if (sample) {
    GstVideoInfo info;
    if (!gst_video_info_from_caps(&info, gst_sample_get_caps(sample)) ||
        GST_VIDEO_INFO_WIDTH(&info) % 2 != 0 || GST_VIDEO_INFO_WIDTH(&info) < 2) {
      gst_sample_unref(sample);
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_live = false; m_drawing = false; m_model.clear(3);
      }
      m_canvas.queue_draw();
      set_status("Annotation input must be equal-width side-by-side stereo");
      return true;
    }
    const int width = GST_VIDEO_INFO_WIDTH(&info) / 2, height = GST_VIDEO_INFO_HEIGHT(&info);
    if (width != m_eye_width || height != m_eye_height) {
      stop_output();
      std::lock_guard<std::mutex> lock(m_mutex);
      m_eye_width = width; m_eye_height = height;
      m_model.clear(3); m_drawing = false; m_output_failed = false;
    }
    if (m_sample) gst_sample_unref(m_sample);
    m_sample = sample; m_info = info;
    m_last_frame = g_get_monotonic_time();
    { std::lock_guard<std::mutex> lock(m_mutex); m_live = true; }
    if (!m_output && !m_output_failed) m_output_failed = !start_output();
    if (!m_output_failed) set_status("Annotation video connected");
    m_canvas.queue_draw();
  } else if (m_last_frame && g_get_monotonic_time() - m_last_frame > G_USEC_PER_SEC) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_live = false; m_drawing = false; m_model.clear(3);
    set_status("Video disconnected — drawings cleared; waiting for video…");
    m_canvas.queue_draw();
  }
  return true;
}

AnnotationViewport AnnotationPanel::viewport() const {
  return AnnotationViewport::fit(m_canvas.get_allocated_width(), m_canvas.get_allocated_height(), m_eye_width, m_eye_height);
}
unsigned AnnotationPanel::target_eyes() const { return m_both.get_active() ? 3 : (m_eye.get_active_row_number() == 0 ? 1 : 2); }

bool AnnotationPanel::draw(const Cairo::RefPtr<Cairo::Context> &context) {
  cairo_t *cr = context->cobj();
  cairo_set_source_rgb(cr, 0.05, 0.05, 0.05); cairo_paint(cr);
  if (!m_sample || !m_live) return true;
  GstVideoFrame frame;
  if (!gst_video_frame_map(&frame, &m_info, gst_sample_get_buffer(m_sample), GST_MAP_READ)) return true;
  const auto view = viewport();
  const int eye = m_eye.get_active_row_number();
  auto *pixels = static_cast<unsigned char*>(GST_VIDEO_FRAME_PLANE_DATA(&frame, 0));
  auto *surface = cairo_image_surface_create_for_data(pixels + eye * m_eye_width * 4,
      CAIRO_FORMAT_RGB24, m_eye_width, m_eye_height, GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 0));
  cairo_save(cr);
  cairo_translate(cr, view.x, view.y);
  cairo_scale(cr, view.width / m_eye_width, view.height / m_eye_height);
  cairo_rectangle(cr, 0, 0, m_eye_width, m_eye_height); cairo_clip(cr);
  cairo_set_source_surface(cr, surface, 0, 0); cairo_paint(cr);
  // The overlay preview already contains the published annotations.
  cairo_restore(cr);
  cairo_surface_destroy(surface);
  gst_video_frame_unmap(&frame);
  return true;
}

void AnnotationPanel::begin(double x, double y) {
  AnnotationPoint point;
  if (!m_live || !m_visible.get_active() || !viewport().point(x, y, point)) return;
  const int color = m_color.get_active_row_number();
  const double widths[] = {0.002, 0.004, 0.008};
  std::lock_guard<std::mutex> lock(m_mutex);
  m_model.strokes.push_back({target_eyes(), color == 2 ? 0.0 : 1.0,
                            color == 1 ? 0.0 : 1.0, color == 3 ? 1.0 : 0.0,
                            widths[m_width.get_active_row_number()], {point},
                            m_eye.get_active_row_number() == 0 ? 1u : 2u,
                            m_both.get_active()});
  m_drawing = true; m_canvas.queue_draw();
}
void AnnotationPanel::extend(double x, double y) {
  if (!m_drawing) return;
  AnnotationPoint point;
  // End at the image boundary rather than joining across the letterbox.
  if (!viewport().point(x, y, point)) { m_drawing = false; return; }
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!m_model.strokes.empty()) m_model.strokes.back().points.push_back(point);
  m_canvas.queue_draw();
}
void AnnotationPanel::render(cairo_t *context, unsigned eye) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_live) m_model.render(context, eye, m_eye_width, m_eye_height);
}
void AnnotationPanel::draw_stereo(GstElement *, cairo_t *cr, guint64, guint64, gpointer self) {
  auto *panel = static_cast<AnnotationPanel*>(self);
  panel->render(cr, 1);
  cairo_save(cr);
  cairo_translate(cr, panel->m_eye_width, 0);
  panel->render(cr, 2);
  cairo_restore(cr);
}
} // namespace dvrk_console
