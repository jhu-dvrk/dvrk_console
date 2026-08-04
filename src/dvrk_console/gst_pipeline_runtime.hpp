#ifndef DVRK_CONSOLE_GST_PIPELINE_RUNTIME_HPP
#define DVRK_CONSOLE_GST_PIPELINE_RUNTIME_HPP

#include <gst/gst.h>
#include <rclcpp/rclcpp.hpp>

#include <string>

#include <dvrk_data/stereo_common.hpp>

namespace dvrk_console {

// Owns the GLib bus source and reconnect helper associated with a pipeline.
// The GstElement itself remains owned by the application so pipeline swaps can
// validate and prepare a replacement before committing it.
class GstPipelineRuntime {
public:
  GstPipelineRuntime() = default;
  ~GstPipelineRuntime();

  GstPipelineRuntime(const GstPipelineRuntime &) = delete;
  GstPipelineRuntime &operator=(const GstPipelineRuntime &) = delete;

  bool start(GstElement *pipeline, rclcpp::Node *node,
             const std::string &name);
  void stop();

private:
  dc_stereo::PipelineUserData m_user_data;
  GstElement *m_pipeline = nullptr;
  guint m_bus_watch_id = 0;
};

}  // namespace dvrk_console

#endif
