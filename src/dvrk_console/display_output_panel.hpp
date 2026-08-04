#ifndef DVRK_DISPLAY_OUTPUT_PANEL_HPP
#define DVRK_DISPLAY_OUTPUT_PANEL_HPP

#include <functional>
#include <memory>
#include <string>
#include <vector>

typedef struct _GtkWidget GtkWidget;

namespace Gtk {
class Frame;
}

namespace dvrk_data {
struct AppConfig;
}

namespace dvrk_console {

class DisplayOutputPanel {
public:
  struct SinkDescriptor {
    std::string sink_name;
    std::string label;
    std::string window_title;
    int default_width = 640;
    int default_height = 480;
    GtkWidget *gtk_widget = nullptr;
  };

  DisplayOutputPanel(const dvrk_data::AppConfig &cfg, std::string settings_suffix,
                     std::function<void()> quit_cb);
  ~DisplayOutputPanel();

  DisplayOutputPanel(DisplayOutputPanel &&) noexcept;
  DisplayOutputPanel &operator=(DisplayOutputPanel &&) noexcept;

  DisplayOutputPanel(const DisplayOutputPanel &) = delete;
  DisplayOutputPanel &operator=(const DisplayOutputPanel &) = delete;

  Gtk::Frame &widget();
  void rebuild(const std::vector<SinkDescriptor> &descriptors);

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace dvrk_console

#endif
