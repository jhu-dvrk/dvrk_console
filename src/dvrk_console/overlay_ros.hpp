#ifndef DVRK_DISPLAY_OVERLAY_ROS_HPP
#define DVRK_DISPLAY_OVERLAY_ROS_HPP

#include "overlay_state.hpp"

#include <geometry_msgs/msg/pose_stamped.hpp>
#include <sensor_msgs/msg/joy.hpp>
#include <sensor_msgs/msg/joint_state.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/float64.hpp>
#include <std_msgs/msg/string.hpp>

#include <memory>
#include <string>

namespace dvrk_console {

bool parse_teleop_name(
    const std::string &teleop_name,
    std::string &mtm_name,
    TeleopSide &side,
    int &psm_number,
    std::string *arm_name = nullptr,
    bool *is_camera_teleop = nullptr);

void on_teleop_selected(
    const std_msgs::msg::String::SharedPtr msg,
    const std::shared_ptr<OverlayState> &overlay_state);
void on_teleop_unselected(
    const std_msgs::msg::String::SharedPtr msg,
    const std::shared_ptr<OverlayState> &overlay_state);
void on_teleop_following(
    const std::string &teleop_name,
    const std_msgs::msg::Bool::SharedPtr msg,
    const std::shared_ptr<OverlayState> &overlay_state);
void on_teleop_scale(
    const std::string &teleop_name,
    const std_msgs::msg::Float64::SharedPtr msg,
    const std::shared_ptr<OverlayState> &overlay_state);
void on_teleop_current_state(
    const std::string &teleop_name,
    const std_msgs::msg::String::SharedPtr msg,
    const std::shared_ptr<OverlayState> &overlay_state);
void on_teleop_measured_cp(
    const std::string &psm_name,
    const geometry_msgs::msg::PoseStamped::SharedPtr msg,
    const std::shared_ptr<OverlayState> &overlay_state);
void on_teleop_tool_type(
    const std::string &psm_name,
    const std_msgs::msg::String::SharedPtr msg,
    const std::shared_ptr<OverlayState> &overlay_state);

void on_camera_joy(
    const sensor_msgs::msg::Joy::SharedPtr msg,
    const std::shared_ptr<OverlayState> &overlay_state);
void on_clutch_joy(
    const sensor_msgs::msg::Joy::SharedPtr msg,
    const std::shared_ptr<OverlayState> &overlay_state);
void on_focus_minus_joy(
    const sensor_msgs::msg::Joy::SharedPtr msg,
    const std::shared_ptr<OverlayState> &overlay_state);
void on_focus_plus_joy(
    const sensor_msgs::msg::Joy::SharedPtr msg,
    const std::shared_ptr<OverlayState> &overlay_state);
void on_coag_joy(
    const sensor_msgs::msg::Joy::SharedPtr msg,
    const std::shared_ptr<OverlayState> &overlay_state);
void on_bicoag_joy(
    const sensor_msgs::msg::Joy::SharedPtr msg,
    const std::shared_ptr<OverlayState> &overlay_state);
void on_operator_present(
    const sensor_msgs::msg::Joy::SharedPtr msg,
    const std::shared_ptr<OverlayState> &overlay_state);
void on_ecm_measured_js(
    const sensor_msgs::msg::JointState::SharedPtr msg,
    const std::shared_ptr<OverlayState> &overlay_state);

}  // namespace dvrk_console

#endif
