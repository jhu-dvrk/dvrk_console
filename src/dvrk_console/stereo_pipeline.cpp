#include "stereo_pipeline.hpp"
#include <dvrk_data/dvrk_gst_socket.hpp>
#include <algorithm>
#include <cmath>

namespace dvrk_console {
// Helper: ensure a pixel count is even (round down)
static int make_even(int v) { return v & ~1; }

static bool is_side_by_side_sink(const std::string &sink) {
  return sink == "side_by_side" || sink == "glimage";
}

static bool is_separate_sink(const std::string &sink) {
  return sink == "separate" || sink == "glimages";
}

static bool is_headless_sink(const std::string &sink) {
  return sink == "headless";
}

bool is_valid_sink(const std::string &sink) {
  return is_side_by_side_sink(sink) || is_separate_sink(sink) || is_headless_sink(sink);
}

std::vector<dvrk_video::LatestVideoPosition> pip_widget_positions(
    const dvrk_video::LatestVideoSpec &spec, int disparity_px) {
  std::vector<dvrk_video::LatestVideoPosition> positions;
  if (spec.name != "__stereo_comp__" || spec.inputs.size() <= 2) return positions;
  const int eye_w = spec.width / 2;
  const size_t count = (spec.inputs.size() - 2) / 2;
  // Use wide arithmetic so even invalid startup configurations cannot overflow.
  const int64_t offset = -static_cast<int64_t>(disparity_px);
  const int64_t lo = offset / 2, hi = offset - lo;
  for (int side = 0; side < 2; ++side) {
    for (size_t i = 0; i < count; ++i) {
      const auto &input = spec.inputs[2 + side * count + i];
      const int64_t base = i * (input.width + dvrk_data::AppConfig::gap_px);
      const int64_t origin = side * eye_w;
      const int x = static_cast<int>(std::clamp(origin + base + (side ? hi : -lo),
          origin, origin + eye_w - input.width));
      positions.push_back({input.name, x, input.y});
    }
  }
  return positions;
}

std::string build_pipeline_string(const dvrk_data::AppConfig &stereo, bool include_overlay,
    std::vector<dvrk_video::LatestVideoSpec> *mixer_specs) {
  using dvrk_video::CameraContent;
  using dvrk_video::LatestVideoInput;
  using dvrk_video::LatestVideoMixers;
  using dvrk_video::LatestVideoSpec;
  const int eye_w = stereo.original_width, eye_h = stereo.original_height;
  const int stereo_w = 2 * eye_w;
  const bool side_by_side = std::any_of(stereo.sinks.begin(), stereo.sinks.end(), is_side_by_side_sink);
  const bool separate = std::any_of(stereo.sinks.begin(), stereo.sinks.end(), is_separate_sink);
  const bool display = side_by_side || separate;
  const bool socket_output = !stereo.stereo.gst_output.empty();
  const auto &extras = stereo.pip_gst_inputs;
  const int n_mono = static_cast<int>(extras.monos.size());
  const int n_stereo = static_cast<int>(extras.stereos.size());
  const int n_extra = n_mono + n_stereo;
  const bool has_extra = n_extra > 0 && extras.scale > 0.01;
  const bool has_ar = stereo.ar.enabled &&
      (!stereo.ar.left.gst_input.empty() || !stereo.ar.right.gst_input.empty());
  const bool has_stereo_ar = stereo.ar.enabled && !stereo.ar.stereo.gst_input.empty();
  const std::string queue = " ! queue max-size-buffers=1 max-size-time=0 max-size-bytes=0 leaky=downstream";
  const auto rgba = [](int width, int height) {
    return " ! videoconvert ! video/x-raw,format=RGBA,width=" + std::to_string(width) +
        ",height=" + std::to_string(height);
  };
  const auto overlay = [](const std::string &name) {
    return " ! videoconvert ! cairooverlay name=" + name +
        " ! videoconvert ! video/x-raw,format=RGBA";
  };
  std::string branches;
  const auto mix = [&](LatestVideoSpec spec, const std::vector<std::string> &inputs) {
    for (size_t i = 0; i < inputs.size(); ++i)
      branches += " " + inputs[i] + " ! " + LatestVideoMixers::sink(spec.inputs[i].name) + " ";
    spec.rgba = true;
    if (mixer_specs) mixer_specs->push_back(spec);
    return LatestVideoMixers::source(spec.name) + rgba(spec.width, spec.height);
  };
  const auto join_eyes = [&](const std::string &name, const std::string &left, const std::string &right) {
    LatestVideoSpec spec;
    spec.name = name; spec.width = stereo_w; spec.height = eye_h;
    spec.inputs = {{name + "_left", 0, 0, eye_w, eye_h, CameraContent::Left},
                   {name + "_right", eye_w, 0, eye_w, eye_h, CameraContent::Right}};
    return mix(spec, {left, right});
  };
  const auto crop_eye = [&](const std::string &tee, bool left) {
    return tee + "." + queue + " ! videocrop left=" + std::to_string(left ? 0 : eye_w) +
        " right=" + std::to_string(left ? eye_w : 0) + " top=0 bottom=0" + rgba(eye_w, eye_h);
  };

  std::string video = stereo.stereo.gst_input +
      " ! queue name=__stereo_src_q__ max-size-buffers=1 max-size-time=0 max-size-bytes=0 leaky=downstream" +
      rgba(stereo_w, eye_h);
  if (has_ar) {
    branches += " " + video + " ! tee name=__clean_stereo__ ";
    const auto ar_eye = [&](bool left) {
      std::string camera = crop_eye("__clean_stereo__", left);
      const auto &input = left ? stereo.ar.left.gst_input : stereo.ar.right.gst_input;
      if (input.empty()) return camera;
      const std::string name = left ? "left_ar_mix" : "right_ar_mix";
      std::string artwork = LatestVideoMixers::source(left ? "left_ar_input" : "right_ar_input") +
          rgba(eye_w, eye_h);
      if (stereo.ar.use_color_key)
        artwork += " ! alpha method=custom target-r=" + std::to_string(stereo.ar.color_key_r) +
            " target-g=" + std::to_string(stereo.ar.color_key_g) +
            " target-b=" + std::to_string(stereo.ar.color_key_b);
      LatestVideoSpec spec;
      spec.name = name; spec.width = eye_w; spec.height = eye_h;
      spec.inputs = {{name + "_camera", 0, 0, eye_w, eye_h, CameraContent::Stereo},
                     {name + "_art", 0, 0, eye_w, eye_h, CameraContent::None, false, 0.5, true}};
      return mix(spec, {camera, artwork});
    };
    const auto left = ar_eye(true);
    const auto right = ar_eye(false);
    video = join_eyes("mix", left, right);
  }

  if (stereo.display_horizontal_shift_px != 0) {
    const int shift = stereo.display_horizontal_shift_px;
    const int left_shift = shift >= 0 ? (shift + 1) / 2 : shift / 2;
    const int right_shift = left_shift - shift;
    branches += " " + video + " ! tee name=__display_shift_in__ ";
    const auto shifted_eye = [&](bool left) {
      const int pixels = left ? left_shift : right_shift;
      return crop_eye("__display_shift_in__", left) +
          " ! videobox fill=black left=" + std::to_string(-pixels) +
          " right=" + std::to_string(pixels) + rgba(eye_w, eye_h);
    };
    video = join_eyes("__display_shift_mix__", shifted_eye(true), shifted_eye(false));
  }

  if (has_stereo_ar) {
    LatestVideoSpec spec;
    spec.name = "stereo_ar_mix"; spec.width = stereo_w; spec.height = eye_h;
    spec.inputs = {{"stereo_ar_camera", 0, 0, stereo_w, eye_h, CameraContent::Stereo},
                   {"stereo_ar_art", 0, 0, stereo_w, eye_h, CameraContent::None, false, 0.5, true}};
    video = mix(spec, {video, LatestVideoMixers::source("stereo_ar_input") + rgba(stereo_w, eye_h)});
  }

  if (display && socket_output) {
    branches += " " + video + " ! tee name=__stereo_out__ ";
    video = "__stereo_out__." + queue;
  }
  std::string outputs;
  if (display) {
    std::string presentation = video;
    if (include_overlay) presentation += overlay("stereo_overlay");
    if (has_extra) {
      const int gap = dvrk_data::AppConfig::gap_px;
      const int stereo_h = std::max(2, std::min(eye_h - 2,
          make_even(static_cast<int>(std::round(eye_h * (1.0 - extras.scale))))));
      const int mono_h = std::max(2, eye_h - stereo_h - gap);
      const int slot_w = std::max(2, make_even((eye_w - (n_extra - 1) * gap) / n_extra));
      branches += " " + presentation + " ! tee name=__stereo_split__ ";
      LatestVideoSpec spec;
      spec.name = "__stereo_comp__"; spec.width = stereo_w; spec.height = eye_h;
      spec.inputs = {{"pip_camera_left", 0, 0, eye_w, stereo_h, CameraContent::Left, true},
                     {"pip_camera_right", eye_w, 0, eye_w, stereo_h, CameraContent::Right, true}};
      std::vector<std::string> inputs{crop_eye("__stereo_split__", true), crop_eye("__stereo_split__", false)};
      for (int i = 0; i < n_mono; ++i)
        branches += " " + extras.monos[i] + queue + " ! videoconvert ! video/x-raw,format=RGBA"
            " ! tee name=__extra_mono" + std::to_string(i) + "__ ";
      for (int i = 0; i < n_stereo; ++i) {
        branches += " " + extras.stereos[i].left + queue + " ! videoconvert ! video/x-raw,format=RGBA"
            " ! tee name=__extra_stereo_left" + std::to_string(i) + "__ ";
        branches += " " + extras.stereos[i].right + queue + " ! videoconvert ! video/x-raw,format=RGBA"
            " ! tee name=__extra_stereo_right" + std::to_string(i) + "__ ";
      }
      for (int side = 0; side < 2; ++side) {
        for (int i = 0; i < n_extra; ++i) {
          const int base_x = i * (slot_w + gap);
          const int x = side * eye_w + base_x;
          const auto name = "pip_extra_" + std::to_string(side) + "_" + std::to_string(i);
          spec.inputs.push_back({name, x, stereo_h + gap, slot_w, mono_h, CameraContent::None, true, 0.0});
          const auto tee = i < n_mono ? "__extra_mono" + std::to_string(i) + "__"
              : std::string(side ? "__extra_stereo_right" : "__extra_stereo_left") + std::to_string(i - n_mono) + "__";
          inputs.push_back(tee + "." + queue);
        }
      }
      const auto positions = pip_widget_positions(spec, stereo.widget_horizontal_disparity_px);
      for (size_t i = 0; i < positions.size(); ++i) spec.inputs[i + 2].x = positions[i].x;
      presentation = mix(spec, inputs);
    }
    outputs += " " + presentation + " ! tee name=__stereo_display_out__ ";
    if (side_by_side)
      outputs += " __stereo_display_out__." + queue +
          " ! glupload ! glcolorconvert ! gtkglsink sync=false force-aspect-ratio=false name=__stereo_sink__ ";
    if (separate) {
      outputs += " " + crop_eye("__stereo_display_out__", true) +
          " ! glupload ! glcolorconvert ! gtkglsink name=__left_eye_sink__ sync=false force-aspect-ratio=false ";
      outputs += " " + crop_eye("__stereo_display_out__", false) +
          " ! glupload ! glcolorconvert ! gtkglsink name=__right_eye_sink__ sync=false force-aspect-ratio=false ";
    }
  }
  if (socket_output) {
    std::string output = display ? "__stereo_out__." + queue : video;
    if (include_overlay) output += overlay("stereo_overlay_unixfd");
    outputs += " " + output + " ! videoconvert ! video/x-raw,format=I420"
        " ! queue name=__stereo_output_q__ max-size-buffers=1 max-size-time=0 max-size-bytes=0 leaky=downstream ! " +
        dvrk_gst::build_sink(dvrk_gst::resolve(stereo.stereo.gst_output));
  }
  if (!display && !socket_output) {
    outputs += " " + video;
    if (include_overlay) outputs += overlay("stereo_overlay");
    outputs += " ! fakesink name=__timing_sink__ sync=false";
  }
  return branches + " " + outputs;
}

}
