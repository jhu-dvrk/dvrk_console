#ifndef DVRK_CONSOLE_FRAME_TIMING_PANEL_HPP
#define DVRK_CONSOLE_FRAME_TIMING_PANEL_HPP

#include "frame_timing.hpp"
#include <gtkmm.h>
#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>

namespace dvrk_console {
class FrameTimingPanel : public Gtk::Frame {
public:
  FrameTimingPanel() : Gtk::Frame("Frame timing"), m_stats(std::make_shared<FrameTiming>()) {
    m_box.set_border_width(8);
    m_box.set_spacing(5);
    add(m_box);
    m_values.set_xalign(0);
    m_values.set_selectable(true);
    m_values.override_font(Pango::FontDescription("Monospace 9"));
    m_box.pack_start(m_values, Gtk::PACK_SHRINK);
    m_note.set_text("Ages at display input (ms). Avg/max: last 5 s.\nΔt = L − R; averages/maxima use |Δt|. CPU source times.");
    m_note.set_xalign(0);
    m_box.pack_start(m_note, Gtk::PACK_SHRINK);
    m_reset.set_label("Reset statistics");
    m_reset.signal_clicked().connect([this] { m_stats->reset(); refresh(); });
    m_box.pack_start(m_reset, Gtk::PACK_SHRINK);
    m_timer = Glib::signal_timeout().connect([this] { refresh(); return true; }, 250);
    refresh();
  }
  ~FrameTimingPanel() override { m_timer.disconnect(); detach(); }

  void attach(GstElement *pipeline) {
    detach();
    bool separate = add_probe(pipeline, "__left_eye_sink__", "sink", 1);
    separate = add_probe(pipeline, "__right_eye_sink__", "sink", 2) || separate;
    if (!separate && !add_probe(pipeline, "__stereo_sink__", "sink", 3) &&
        !add_probe(pipeline, "__stereo_output_q__", "src", 3))
      add_probe(pipeline, "__timing_sink__", "sink", 3);
  }

private:
  struct Binding { GstPad *pad; gulong id; };
  struct Observer { std::shared_ptr<FrameTiming> stats; unsigned mask; };
  bool add_probe(GstElement *pipeline, const char *element_name, const char *pad_name, unsigned mask) {
    auto *element = gst_bin_get_by_name(GST_BIN(pipeline), element_name);
    if (!element) return false;
    auto *pad = gst_element_get_static_pad(element, pad_name);
    gst_object_unref(element);
    if (!pad) return false;
    auto *observer = new Observer{m_stats, mask};
    const auto id = gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER,
      [](GstPad *, GstPadProbeInfo *info, gpointer data) {
        auto &observer = *static_cast<Observer *>(data);
        observer.stats->observe(dc_buffer_get_frame_timestamps(GST_PAD_PROBE_INFO_BUFFER(info)), observer.mask);
        return GST_PAD_PROBE_OK;
      }, observer, [](gpointer data) { delete static_cast<Observer *>(data); });
    m_bindings.push_back({pad, id});
    return true;
  }
  void detach() {
    for (const auto &binding : m_bindings) {
      gst_pad_remove_probe(binding.pad, binding.id);
      gst_object_unref(binding.pad);
    }
    m_bindings.clear();
  }
  static std::string number(const std::optional<double> &value) {
    if (!value) return "--";
    std::ostringstream text;
    text << std::fixed << std::setprecision(1) << *value;
    return text.str();
  }
  void refresh() {
    const auto timing = m_stats->snapshot();
    std::ostringstream text;
    text << "         Current      Avg      Max     Peak    FPS\n";
    for (unsigned side = 0; side < 2; ++side) {
      const auto &eye = timing.eyes[side];
      text << (side ? "Right  " : "Left   ") << std::setw(9) << number(eye.current)
           << std::setw(9) << number(eye.average) << std::setw(9) << number(eye.maximum)
           << std::setw(9) << number(eye.peak) << std::setw(7)
           << (eye.current ? number(eye.fps) : "--") << (eye.stale ? "  STALE" : "") << '\n';
    }
    text << "Δt     " << std::setw(9) << number(timing.delta.current)
         << std::setw(9) << number(timing.delta.average) << std::setw(9) << number(timing.delta.maximum);
    if (!timing.eyes[0].current || !timing.eyes[1].current)
      text << "\nWaiting for source timestamps (-- = unavailable).";
    m_values.set_text(text.str());
  }
  std::shared_ptr<FrameTiming> m_stats;
  Gtk::Box m_box{Gtk::ORIENTATION_VERTICAL};
  Gtk::Label m_values, m_note;
  Gtk::Button m_reset;
  sigc::connection m_timer;
  std::vector<Binding> m_bindings;
};
} // namespace dvrk_console
#endif
