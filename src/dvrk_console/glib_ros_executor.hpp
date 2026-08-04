#ifndef DVRK_CONSOLE_GLIB_ROS_EXECUTOR_HPP
#define DVRK_CONSOLE_GLIB_ROS_EXECUTOR_HPP

#include <glib.h>
#include <rclcpp/rclcpp.hpp>

#include <memory>

namespace dvrk_console {

class GlibRosExecutor {
public:
  explicit GlibRosExecutor(const std::shared_ptr<rclcpp::Node> &node,
                           guint interval_ms = 20);
  ~GlibRosExecutor();

  GlibRosExecutor(const GlibRosExecutor &) = delete;
  GlibRosExecutor &operator=(const GlibRosExecutor &) = delete;

  void stop();

private:
  static gboolean on_tick(gpointer user_data);

  std::shared_ptr<rclcpp::Node> m_node;
  guint m_source_id = 0;
};

}  // namespace dvrk_console

#endif
