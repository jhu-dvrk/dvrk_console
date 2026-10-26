#include "stereo_pipeline.hpp"
#include <dvrk_data/cpu_timestamp_meta.hpp>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <iostream>
#include <stdexcept>
void check(bool good, const char *message) { if (!good) throw std::runtime_error(message); }
void replace_all(std::string &text, const std::string &before, const std::string &after) {
  size_t at = 0;
  while ((at = text.find(before, at)) != std::string::npos) {
    text.replace(at, before.size(), after); at += after.size();
  }
}
int main(int argc, char **argv) {
  gst_init(&argc, &argv);
  dc_frame_timestamps_meta_register();
  for (int variant = 0; variant < 6; ++variant) {
    dvrk_data::AppConfig cfg;
    cfg.original_width = 160; cfg.original_height = 120;
    cfg.stereo.gst_output.clear();
    cfg.stereo.gst_input = "appsrc name=camera is-live=true format=time ! video/x-raw,format=RGBA,width=320,height=120,framerate=30/1";
    cfg.sinks = variant == 1 ? std::vector<std::string>{"separate"} : std::vector<std::string>{"side_by_side"};
    if (variant >= 2) cfg.display_horizontal_shift_px = 5;
    if (variant >= 3) {
      cfg.ar.enabled = true;
      cfg.ar.left.gst_input = "unused";
      cfg.ar.right.gst_input = "unused";
      cfg.ar.stereo.gst_input = "unused";
    }
    if (variant >= 4) {
      cfg.pip_gst_inputs.monos.push_back("videotestsrc is-live=true ! video/x-raw,width=80,height=60,framerate=3/1");
      cfg.pip_gst_inputs.scale = 0.3;
    }
    std::vector<dvrk_video::LatestVideoSpec> specs;
    auto description = dvrk_console::build_pipeline_string(cfg, variant != 5, &specs);
    check(description.find("sync=true") == std::string::npos && description.find("compositor") == std::string::npos &&
          description.find("glvideomixer") == std::string::npos, "Pipeline reintroduced timestamp synchronization");
    // Exercise spatial layout and metadata headlessly; GTK/GL are checked live.
    replace_all(description, "gtkglsink", "appsink");
    replace_all(description, "force-aspect-ratio=false", "max-buffers=1 drop=true wait-on-eos=false");
    replace_all(description, " ! glupload ! glcolorconvert", "");
    GError *error = nullptr;
    auto *pipeline = gst_parse_launch(description.c_str(), &error);
    if (error) std::cerr << description << "\n" << error->message << '\n';
    check(pipeline && !error, "Cannot parse display pipeline");
    dvrk_video::LatestVideoMixers latest;
    std::string failure;
    check(latest.start(pipeline, specs, failure), failure.c_str());
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    auto *camera = gst_bin_get_by_name(GST_BIN(pipeline), "camera");
    auto *buffer = gst_buffer_new_allocate(nullptr, 320 * 120 * 4, nullptr);
    GstMapInfo pixels;
    check(gst_buffer_map(buffer, &pixels, GST_MAP_WRITE), "Cannot map camera frame");
    for (size_t i = 0; i < pixels.size; i += 4) {
      pixels.data[i] = 10; pixels.data[i + 1] = 20; pixels.data[i + 2] = 30; pixels.data[i + 3] = 255;
    }
    gst_buffer_unmap(buffer, &pixels);
    GST_BUFFER_PTS(buffer) = 3600 * GST_SECOND;
    DcFrameTimestamps times;
    times.left_source_ts = 111; times.right_source_ts = 222;
    times.source_stereo_output_ts = 333;
    dc_buffer_set_frame_timestamps(buffer, times);
    gst_app_src_push_buffer(GST_APP_SRC(camera), buffer);
    auto *sink = gst_bin_get_by_name(GST_BIN(pipeline), variant == 1 ? "__left_eye_sink__" : "__stereo_sink__");
    bool received_camera = false;
    for (int i = 0; i < 15; ++i) {
      auto *sample = gst_app_sink_try_pull_sample(GST_APP_SINK(sink), 250 * GST_MSECOND);
      if (!sample) continue;
      const auto seen = dc_buffer_get_frame_timestamps(gst_sample_get_buffer(sample));
      received_camera = seen.left_source_ts == 111 && seen.right_source_ts == 222;
      gst_sample_unref(sample);
      if (received_camera) break;
    }
    check(received_camera, "Missing/slow AR input blocked camera or lost provenance");
    if (variant >= 3) {
      auto *art = gst_bin_get_by_name(GST_BIN(pipeline), "left_ar_input");
      auto *caps = gst_caps_from_string("video/x-raw,format=RGBA,width=160,height=120,framerate=30/1");
      gst_app_src_set_caps(GST_APP_SRC(art), caps);
      gst_caps_unref(caps);
      auto *art_buffer = gst_buffer_new_allocate(nullptr, 160 * 120 * 4, nullptr);
      check(gst_buffer_map(art_buffer, &pixels, GST_MAP_WRITE), "Cannot map artwork");
      for (size_t i = 0; i < pixels.size; i += 4) {
        pixels.data[i] = 200; pixels.data[i + 1] = 0; pixels.data[i + 2] = 0; pixels.data[i + 3] = 128;
      }
      gst_buffer_unmap(art_buffer, &pixels);
      GST_BUFFER_PTS(art_buffer) = 7200 * GST_SECOND;
      gst_app_src_push_buffer(GST_APP_SRC(art), art_buffer);
      bool blended = false;
      for (int i = 0; i < 15; ++i) {
        auto *sample = gst_app_sink_try_pull_sample(GST_APP_SINK(sink), 250 * GST_MSECOND);
        if (!sample) continue;
        GstVideoInfo video_info;
        gst_video_info_from_caps(&video_info, gst_sample_get_caps(sample));
        GstVideoFrame frame;
        if (gst_video_frame_map(&frame, &video_info, gst_sample_get_buffer(sample), GST_MAP_READ)) {
          const auto *data = static_cast<const guint8 *>(GST_VIDEO_FRAME_PLANE_DATA(&frame, 0));
          const int value = data[10 * GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 0) + 40 * 4];
          blended = value > 80 && value < 140;
          gst_video_frame_unmap(&frame);
        }
        const auto seen = dc_buffer_get_frame_timestamps(gst_sample_get_buffer(sample));
        check(seen.left_source_ts == 111 && seen.right_source_ts == 222, "Artwork replaced camera timestamps");
        gst_sample_unref(sample);
        if (blended) break;
      }
      check(blended, "Latest artwork did not alpha-blend over the retained camera frame");
      gst_object_unref(art);
    }
    gst_element_set_state(pipeline, GST_STATE_NULL);
    latest.stop();
    gst_object_unref(camera); gst_object_unref(sink); gst_object_unref(pipeline);
  }
  std::cout << "Display, separate eyes, shift, missing AR, picture-in-picture and overlay variants passed\n";
}
