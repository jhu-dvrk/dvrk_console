#ifndef DVRK_CONSOLE_ANNOTATION_PANEL_HPP
#define DVRK_CONSOLE_ANNOTATION_PANEL_HPP

#include <gtkmm.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <mutex>
#include "annotation_model.hpp"
#include "gst_pipeline_runtime.hpp"

namespace dvrk_console {
struct AnnotationConfig {
  std::string gst_input = "@dvrk:stereo_display:overlay";
  std::string gst_output = "@dvrk:control_panel:annotations";
};

class AnnotationPanel : public Gtk::Box {
public:
  AnnotationPanel(rclcpp::Node *node, const AnnotationConfig &config);
  ~AnnotationPanel() override;
  void start();
  void set_enabled(bool enabled);
  // Keep the drawing canvas in the preview and place its controls below it.
  void place_controls(Gtk::Box &container);
private:
  void stop_input();
  void stop_output();
  bool start_output();
  bool tick();
  bool draw(const Cairo::RefPtr<Cairo::Context> &context);
  void begin(double x, double y);
  void extend(double x, double y);
  unsigned target_eyes() const;
  AnnotationViewport viewport() const;
  static void draw_stereo(GstElement *, cairo_t *, guint64, guint64, gpointer);
  void render(cairo_t *context, unsigned eye);

  rclcpp::Node *m_node;
  AnnotationConfig m_config;
  Gtk::Box m_tools{Gtk::ORIENTATION_HORIZONTAL, 4};
  Gtk::Box m_actions{Gtk::ORIENTATION_HORIZONTAL, 4};
  Gtk::ComboBoxText m_eye, m_color, m_width;
  Gtk::CheckButton m_both{"Both eyes"};
  Gtk::ToggleButton m_visible{"Show drawings"};
  Gtk::Button m_undo{"Undo"}, m_clear{"Clear"}, m_clear_all{"Clear all"}, m_retry{"Reconnect"};
  void set_status(const std::string &status);
  std::string m_status_text;
  Gtk::DrawingArea m_canvas;
  Glib::RefPtr<Gtk::GestureDrag> m_drag;
  sigc::connection m_timer;
  GstElement *m_input = nullptr, *m_output = nullptr, *m_sink = nullptr;
  GstSample *m_sample = nullptr;
  GstVideoInfo m_info{};
  GstPipelineRuntime m_input_runtime;
  int m_eye_width = 0, m_eye_height = 0;
  double m_start_x = 0, m_start_y = 0;
  bool m_enabled = true;
  bool m_drawing = false, m_output_failed = false, m_live = false;
  gint64 m_last_frame = 0;
  std::mutex m_mutex;
  AnnotationModel m_model;
};
} // namespace dvrk_console
#endif
