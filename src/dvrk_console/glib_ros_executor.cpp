#include "glib_ros_executor.hpp"

namespace dvrk_console {

GlibRosExecutor::GlibRosExecutor(const std::shared_ptr<rclcpp::Node> &node,
                                 guint interval_ms)
    : m_node(node),
      m_source_id(g_timeout_add(interval_ms, &GlibRosExecutor::on_tick, this)) {}

GlibRosExecutor::~GlibRosExecutor() {
  stop();
}

void GlibRosExecutor::stop() {
  if (m_source_id != 0) {
    g_source_remove(m_source_id);
    m_source_id = 0;
  }
}

gboolean GlibRosExecutor::on_tick(gpointer user_data) {
  auto *self = static_cast<GlibRosExecutor *>(user_data);
  if (self == nullptr || !self->m_node || !rclcpp::ok()) {
    if (self != nullptr) {
      self->m_source_id = 0;
    }
    return G_SOURCE_REMOVE;
  }

  rclcpp::spin_some(self->m_node->get_node_base_interface());
  return G_SOURCE_CONTINUE;
}

}  // namespace dvrk_console
