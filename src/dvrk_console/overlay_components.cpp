#include "overlay_components.hpp"
#include "overlay_utils.hpp"
#include <cmath>
#include <algorithm>

namespace sv {

namespace {

void append_pedal_path(cairo_t *cr, double cx, double cy, double radius,
                       double corner_radius) {
  const double half_width = radius * 0.6; // width reduced by 40%
  cairo_new_path(cr);
  // Top: perfect semicircle of radius half_width
  cairo_arc(cr, cx, cy, half_width, M_PI, 0.0);
  // Right side straight down, then bottom-right rounded corner
  cairo_line_to(cr, cx + half_width, cy + radius - corner_radius);
  cairo_arc(cr, cx + half_width - corner_radius, cy + radius - corner_radius,
            corner_radius, 0.0, 0.5 * M_PI);
  // Bottom edge, then bottom-left rounded corner
  cairo_line_to(cr, cx - half_width + corner_radius, cy + radius);
  cairo_arc(cr, cx - half_width + corner_radius, cy + radius - corner_radius,
            corner_radius, 0.5 * M_PI, M_PI);
  cairo_close_path(cr);
}

void append_focus_pedal_path(cairo_t *cr, double cx, double cy,
                             double radius) {
  // Stadium shape: top and bottom semicircles of radius half_width,
  // with a straight rectangular section in between. Total height = 2*radius.
  const double half_width = radius * 0.6; // width reduced by 40%
  const double rect_half_height = radius - half_width;
  cairo_new_path(cr);
  // Top semicircle (left -> right, arching upward)
  cairo_arc(cr, cx, cy - rect_half_height, half_width, M_PI, 0.0);
  // Right side down to bottom semicircle
  cairo_line_to(cr, cx + half_width, cy + rect_half_height);
  // Bottom semicircle (right -> left, arching downward)
  cairo_arc(cr, cx, cy + rect_half_height, half_width, 0.0, M_PI);
  cairo_close_path(cr);
}

void set_button_fill(cairo_t *cr, int status, double alpha,
                     const OverlayTheme &theme) {
  if (status == 2) {
    set_source_rgba(cr, theme.active_green, alpha);
  } else {
    set_source_rgba(cr, theme.active_grey, alpha);
  }
}

void draw_focus_label(cairo_t *cr, const char *label, int status, double cx,
                      double cy, double radius, double alpha,
                      const OverlayTheme &theme) {
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, radius * 0.9);

  cairo_text_extents_t extents;
  cairo_text_extents(cr, label, &extents);
  cairo_move_to(cr, cx - (extents.width * 0.5 + extents.x_bearing),
                cy - (extents.height * 0.5 + extents.y_bearing));
  set_source_rgba(cr, status != 0 ? theme.text_dark : theme.valid_grey,
                  alpha);
  cairo_show_text(cr, label);
}

} // namespace

void draw_status_circle(cairo_t *cr, int status, double cx, double cy,
                        double radius, double alpha,
                        const OverlayTheme &theme) {
  append_pedal_path(cr, cx, cy, radius, theme.corner_radius);

  if (status != 0) {
    set_button_fill(cr, status, alpha, theme);
    cairo_fill_preserve(cr);
  }

  set_source_rgba(cr, theme.valid_grey, alpha);
  cairo_set_line_width(cr, theme.line_width);
  cairo_stroke(cr);
}

void draw_focus_pedal(cairo_t *cr, int focus_minus_status,
                      int focus_plus_status, double cx, double cy,
                      double radius, double alpha,
                      const OverlayTheme &theme) {
  cairo_save(cr);
  append_focus_pedal_path(cr, cx, cy, radius);
  cairo_clip(cr);

  if (focus_plus_status != 0) {
    set_button_fill(cr, focus_plus_status, alpha, theme);
    cairo_rectangle(cr, cx - radius, cy - radius, 2.0 * radius, radius);
    cairo_fill(cr);
  }
  if (focus_minus_status != 0) {
    set_button_fill(cr, focus_minus_status, alpha, theme);
    cairo_rectangle(cr, cx - radius, cy, 2.0 * radius, radius);
    cairo_fill(cr);
  }
  cairo_restore(cr);

  append_focus_pedal_path(cr, cx, cy, radius);
  set_source_rgba(cr, theme.valid_grey, alpha);
  cairo_set_line_width(cr, theme.line_width);
  cairo_stroke(cr);

  const double half_width = radius * 0.6;
  cairo_move_to(cr, cx - half_width, cy);
  cairo_line_to(cr, cx + half_width, cy);
  cairo_stroke(cr);

  draw_focus_label(cr, "+", focus_plus_status, cx, cy - radius * 0.48,
                   radius, alpha, theme);
  draw_focus_label(cr, "-", focus_minus_status, cx, cy + radius * 0.48,
                   radius, alpha, theme);
}

void draw_numbered_circle(cairo_t *cr, bool active, bool valid, int number,
                          double cx, double cy, double radius, double alpha,
                          const OverlayTheme &theme) {
  cairo_new_path(cr);
  cairo_arc(cr, cx, cy, radius, 0.0, 2.0 * M_PI);
  if (active) {
    if (valid) {
      set_source_rgba(cr, theme.active_grey, alpha);
    } else {
      set_source_rgba(cr, theme.invalid_red, alpha);
    }
    cairo_fill_preserve(cr);
  }

  if (valid) {
    set_source_rgba(cr, theme.valid_grey, alpha);
  } else {
    set_source_rgba(cr, theme.invalid_red, alpha);
  }
  cairo_set_line_width(cr, theme.line_width);
  cairo_stroke(cr);

  const std::string label = std::to_string(number);
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, radius * 1.1);

  cairo_text_extents_t extents;
  cairo_text_extents(cr, label.c_str(), &extents);
  cairo_move_to(cr, cx - (extents.width / 2.0 + extents.x_bearing),
                cy - (extents.height / 2.0 + extents.y_bearing));
  set_source_rgba(cr, theme.text_light, alpha);
  cairo_show_text(cr, label.c_str());
}

void draw_scale_gage(cairo_t *cr, double scale, bool on_right, double cx,
                     double cy, double radius, double alpha,
                     const OverlayTheme &theme) {
  const double gage_height = radius * 2.0;
  const double gage_width = gage_height / 3.0;

  const double x = on_right ? (cx + radius + theme.h_spacing)
                            : (cx - radius - theme.h_spacing - gage_width);
  const double y = cy - radius;

  // Background
  draw_rounded_rectangle(cr, x, y, gage_width, gage_height, theme.corner_radius);
  set_source_rgba(cr, theme.valid_grey, alpha);
  cairo_set_line_width(cr, theme.line_width);
  cairo_stroke(cr);

  // Fill
  const double clamped_scale = std::clamp(scale, 0.0, 1.0);
  const double fill_height = gage_height * clamped_scale;
  if (fill_height > 0) {
    draw_rounded_rectangle(cr, x, y + gage_height - fill_height, gage_width,
                           fill_height, theme.corner_radius);
    set_source_rgba(cr, theme.active_grey, alpha);
    cairo_fill(cr);
  }
}

void draw_tool_type_label(cairo_t *cr, const std::string &tool_type,
                          bool left_side, double cx, double cy, double radius,
                          double alpha, double extra_offset,
                          const OverlayTheme &theme) {
  const std::string display_tool_type = format_tool_type_label(tool_type);
  if (display_tool_type.empty()) {
    return;
  }

  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, theme.text_height);

  cairo_text_extents_t extents;
  cairo_text_extents(cr, display_tool_type.c_str(), &extents);

  const double text_x = left_side
                            ? (cx + radius + theme.h_spacing + extra_offset)
                            : (cx - radius - theme.h_spacing - extents.width -
                               extents.x_bearing - extra_offset);
  // Position text top at cy + v_spacing*0.5
  const double text_y = cy + theme.v_spacing * 0.5 - extents.y_bearing;

  cairo_move_to(cr, text_x, text_y);
  set_source_rgba(cr, theme.text_light, alpha);
  cairo_show_text(cr, display_tool_type.c_str());
}

void draw_scale_label(cairo_t *cr, const std::string &state, bool left_side,
                      double cx, double cy, double radius, double alpha,
                      double extra_offset, const OverlayTheme &theme) {
  std::string label = "";
  if (state == "DISABLED") {
    label = "Disabled";
  } else if (state == "ALIGNING_MTM") {
    label = "Aligning...";
  } else if (state == "SETTING_ARMS_STATE") {
    label = "Checking arms";
  }

  if (label.empty()) {
    return;
  }

  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, theme.text_height);

  cairo_text_extents_t extents;
  cairo_text_extents(cr, label.c_str(), &extents);

  const double text_x = left_side
                            ? (cx + radius + theme.h_spacing + extra_offset)
                            : (cx - radius - theme.h_spacing - extents.width -
                               extents.x_bearing - extra_offset);
  // Position text bottom at cy - v_spacing*0.5
  const double text_y =
      cy - theme.v_spacing * 0.5 - (extents.height + extents.y_bearing);

  cairo_move_to(cr, text_x, text_y);
  set_source_rgba(cr, theme.text_light, alpha);
  cairo_show_text(cr, label.c_str());
}

void draw_camera_icon(cairo_t *cr, bool active, bool valid, double cx, double cy,
                      double radius, double alpha, double roll,
                      const OverlayTheme &theme) {
  const RgbaColor outline_color =
      valid ? theme.valid_grey : theme.invalid_red;

  const double body_height = radius * 1.6;
  const double body_width =
      body_height * (5.0 / 3.0); // 5:3 horizontal rectangle
  const double x_left = cx - body_width * 0.5;
  const double y_top = cy - body_height * 0.5;

  cairo_new_path(cr);
  cairo_move_to(cr, x_left + theme.corner_radius, y_top);
  cairo_line_to(cr, x_left + body_width - theme.corner_radius, y_top);
  cairo_arc(cr, x_left + body_width - theme.corner_radius,
            y_top + theme.corner_radius, theme.corner_radius, -0.5 * M_PI, 0.0);
  cairo_line_to(cr, x_left + body_width,
                y_top + body_height - theme.corner_radius);
  cairo_arc(cr, x_left + body_width - theme.corner_radius,
            y_top + body_height - theme.corner_radius, theme.corner_radius, 0.0,
            0.5 * M_PI);
  cairo_line_to(cr, x_left + theme.corner_radius, y_top + body_height);
  cairo_arc(cr, x_left + theme.corner_radius,
            y_top + body_height - theme.corner_radius, theme.corner_radius,
            0.5 * M_PI, M_PI);
  cairo_line_to(cr, x_left, y_top + theme.corner_radius);
  cairo_arc(cr, x_left + theme.corner_radius, y_top + theme.corner_radius,
            theme.corner_radius, M_PI, 1.5 * M_PI);
  cairo_close_path(cr);

  if (active) {
    set_source_rgba(cr, theme.active_grey, alpha);
    cairo_fill_preserve(cr);
  }

  set_source_rgba(cr, outline_color, alpha);
  cairo_set_line_width(cr, theme.line_width);
  cairo_stroke(cr);

  const double circle_radius = body_height * 0.45;
  cairo_new_path(cr);
  cairo_arc(cr, cx, cy, circle_radius, 0.0, 2.0 * M_PI);
  set_source_rgba(cr, outline_color, alpha);
  cairo_set_line_width(cr, theme.line_width);
  cairo_stroke(cr);

  // Draw horizon bar inside the circle
  cairo_save(cr);
  cairo_translate(cr, cx, cy);
  cairo_rotate(cr, -roll); // Negative because if camera rolls right, horizon tilts left
  cairo_move_to(cr, -circle_radius * 0.8, 0);
  cairo_line_to(cr, circle_radius * 0.8, 0);
  set_source_rgba(cr, outline_color, alpha);
  cairo_set_line_width(cr, theme.line_width);
  cairo_stroke(cr);
  cairo_restore(cr);
}

void draw_operator_present_icon(cairo_t *cr, int status, double cx, double cy,
                                double radius, double alpha,
                                const OverlayTheme &theme) {
  const RgbaColor outline_color = theme.valid_grey;

  const double body_height = radius * 1.6;
  const double body_width = body_height * (5.0 / 3.0);
  const double x_left = cx - body_width * 0.5;
  const double y_top = cy - body_height * 0.5;

  draw_rounded_rectangle(cr, x_left, y_top, body_width, body_height,
                         theme.corner_radius);

  if (status != 0) {
    if (status == 2) {
      set_source_rgba(cr, theme.active_green, alpha);
    } else {
      set_source_rgba(cr, theme.active_grey, alpha);
    }
    cairo_fill_preserve(cr);
  }

  set_source_rgba(cr, outline_color, alpha);
  cairo_set_line_width(cr, theme.line_width);
  cairo_stroke(cr);

  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, radius * 0.9);
  cairo_text_extents_t extents;
  cairo_text_extents(cr, "OP", &extents);
  cairo_move_to(cr, cx - (extents.width / 2.0 + extents.x_bearing),
                cy - (extents.height / 2.0 + extents.y_bearing));
  if (status != 0) {
    set_source_rgba(cr, theme.text_dark, alpha);
  } else {
    set_source_rgba(cr, outline_color, alpha);
  }
  cairo_show_text(cr, "OP");
}

} // namespace sv
