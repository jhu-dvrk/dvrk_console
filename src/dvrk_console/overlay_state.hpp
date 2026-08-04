#ifndef DVRK_DISPLAY_OVERLAY_STATE_HPP
#define DVRK_DISPLAY_OVERLAY_STATE_HPP

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace dvrk_console {

enum class TeleopSide {
  Left,
  Right
};

struct ButtonState {
  bool present = false;
  bool active = false;
  std::chrono::steady_clock::time_point expiration;

  int get_status() const {
    if (active) return 1;
    if (std::chrono::steady_clock::now() < expiration) return 2;
    return 0;
  }

  bool is_active() const { return get_status() != 0; }
};

struct TeleopIndicator {
  TeleopSide side = TeleopSide::Left;
  std::string arm_name;
  int psm_number = 0;
  bool is_camera_teleop = false;
  bool following_active = false;
  double scale = 1.0;
  std::string current_state;
};

struct ArmOverlayInfo {
  bool measured_cp_valid = true;
  std::string tool_type;
};

struct OverlayState {
  ButtonState camera;
  ButtonState clutch;
  ButtonState focus_minus;
  ButtonState focus_plus;
  ButtonState coag;
  ButtonState bicoag;
  ButtonState operator_present;

  int frame_width = 0;
  int frame_height = 0;
  bool overlay_enabled = true;
  bool show_eye_labels = false;
  bool show_grid = false;
  double overlay_alpha = 0.7;
  int display_horizontal_offset_px = 0;
  std::unordered_map<std::string, std::pair<int, int>>
      overlay_frame_size_by_name;
  double camera_roll = 0.0;
  std::unordered_map<std::string, TeleopIndicator> teleop_indicators;
  std::unordered_map<std::string, ArmOverlayInfo> arm_info;
  std::mutex mutex;
};

}  // namespace dvrk_console

#endif
