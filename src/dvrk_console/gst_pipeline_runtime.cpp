#include "gst_pipeline_runtime.hpp"

namespace dvrk_console {

GstPipelineRuntime::~GstPipelineRuntime() {
  stop();
}

bool GstPipelineRuntime::start(GstElement *pipeline, rclcpp::Node *node,
                               const std::string &name) {
  if (pipeline == nullptr || node == nullptr) {
    return false;
  }

  stop();
  m_pipeline = pipeline;
  m_user_data.node = node;
  m_user_data.reconnector.start(pipeline, node, name);

  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
  if (bus == nullptr) {
    m_user_data.reconnector.stop();
    m_user_data.node = nullptr;
    m_pipeline = nullptr;
    return false;
  }

  m_bus_watch_id = gst_bus_add_watch(
      bus, dc_stereo::on_bus_message, &m_user_data);
  gst_object_unref(bus);
  if (m_bus_watch_id == 0) {
    m_user_data.reconnector.stop();
    m_user_data.node = nullptr;
    m_pipeline = nullptr;
    return false;
  }
  return true;
}

void GstPipelineRuntime::stop() {
  if (m_bus_watch_id != 0) {
    g_source_remove(m_bus_watch_id);
    m_bus_watch_id = 0;
  }
  m_user_data.reconnector.stop();
  m_user_data.node = nullptr;
  m_pipeline = nullptr;
}

}  // namespace dvrk_console
