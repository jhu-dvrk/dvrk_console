#ifndef DVRK_CONSOLE_ANNOTATION_MODEL_HPP
#define DVRK_CONSOLE_ANNOTATION_MODEL_HPP

#include <cairo.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace dvrk_console {

struct AnnotationPoint { double x, y; };

// Coordinates and pen width are fractions of the full eye image. Letterboxing
// is shared by drawing and hit testing, so resizing never moves a mark.
struct AnnotationViewport {
  double x, y, width, height;
  static AnnotationViewport fit(double widget_w, double widget_h,
                                double image_w, double image_h) {
    if (image_w <= 0 || image_h <= 0) return {0, 0, 0, 0};
    const double scale = std::min(widget_w / image_w, widget_h / image_h);
    return {(widget_w - image_w * scale) / 2,
            (widget_h - image_h * scale) / 2, image_w * scale, image_h * scale};
  }
  bool point(double px, double py, AnnotationPoint &result) const {
    if (width <= 0 || height <= 0 || px < x || py < y ||
        px >= x + width || py >= y + height) return false;
    result = {(px - x) / width, (py - y) / height};
    return true;
  }
};

struct AnnotationStroke {
  unsigned eyes;
  double red, green, blue, width;
  std::vector<AnnotationPoint> points;
  unsigned source_eye = 1;
  // Retain stereo placement if Undo/Clear later removes the source eye.
  bool stereo = false;
};

struct AnnotationModel {
  std::vector<AnnotationStroke> strokes;
  bool visible = true;
  int disparity_px = 0;
  void clear(unsigned eyes) {
    for (auto &stroke : strokes) stroke.eyes &= ~eyes;
    strokes.erase(std::remove_if(strokes.begin(), strokes.end(),
                  [](const auto &s) { return s.eyes == 0; }), strokes.end());
  }
  void undo(unsigned eyes) {
    for (auto it = strokes.rbegin(); it != strokes.rend(); ++it) {
      if (it->eyes & eyes) { it->eyes &= ~eyes; break; }
    }
    clear(0);
  }
  void render(cairo_t *cr, unsigned eye, int width, int height) const {
    if (!visible) return;
    cairo_save(cr);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_clip(cr);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    for (const auto &stroke : strokes) {
      if (!(stroke.eyes & eye) || stroke.points.empty()) continue;
      const double shift = stroke.stereo && eye != stroke.source_eye
          ? (stroke.source_eye == 1 ? -static_cast<double>(disparity_px)
                                    : static_cast<double>(disparity_px)) : 0.0;
      cairo_new_path(cr);
      cairo_set_source_rgba(cr, stroke.red, stroke.green, stroke.blue, 1);
      const double pen = stroke.width * width;
      cairo_set_line_width(cr, pen);
      const auto &first = stroke.points.front();
      if (stroke.points.size() == 1) {
        cairo_arc(cr, first.x * width + shift, first.y * height, pen / 2, 0, 2 * std::acos(-1));
        cairo_fill(cr);
      } else {
        cairo_move_to(cr, first.x * width + shift, first.y * height);
        for (const auto &point : stroke.points)
          cairo_line_to(cr, point.x * width + shift, point.y * height);
        cairo_stroke(cr);
      }
    }
    cairo_restore(cr);
  }
};

} // namespace dvrk_console
#endif
