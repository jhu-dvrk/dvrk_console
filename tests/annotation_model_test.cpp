#include "annotation_model.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>

void check(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
}
int main() {
  using namespace dvrk_console;
  AnnotationPoint point{};
  auto view = AnnotationViewport::fit(800, 800, 1920, 1080);
  check(!view.point(400, 10, point), "Letterbox accepted a stroke");
  check(view.point(400, 400, point) && point.x == 0.5 && point.y == 0.5,
        "Preview center must map to image center");
  view = AnnotationViewport::fit(400, 200, 640, 480);
  check(!view.point(0, 100, point), "Pillarbox accepted a stroke");
  check(view.point(200, 100, point) && point.x == 0.5 && point.y == 0.5,
        "Resizing moved the image center");

  AnnotationModel model;
  model.strokes.push_back({1, 1, 0, 0, 0.1, {{0.5, 0.5}}});
  auto *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 100, 100);
  auto *cr = cairo_create(surface);
  auto pixel = [&](unsigned eye, int x, int y) {
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR); cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER); model.render(cr, eye, 100, 100);
    cairo_surface_flush(surface);
    return reinterpret_cast<uint32_t*>(cairo_image_surface_get_data(surface))[y * 100 + x];
  };
  check(pixel(1, 50, 50) == 0xffff0000, "Single tap must render opaque red");
  check(pixel(1, 0, 0) == 0, "Drawing background must remain transparent");
  check(pixel(2, 50, 50) == 0, "Left drawing leaked into right eye");
  model.visible = false;
  check(pixel(1, 50, 50) == 0, "Hidden drawing was still published");
  model.visible = true;
  model.strokes.push_back({3, 0, 1, 0, 0.1, {{0.5, 0.5}, {0.8, 0.5}}});
  check(pixel(2, 70, 50) == 0xff00ff00, "Both-eye stroke absent from right eye");
  model.undo(1);
  check(pixel(1, 50, 50) == 0xffff0000 && pixel(2, 50, 50) == 0xff00ff00,
        "Undo on one eye changed the other eye");
  model.clear(3);
  check(pixel(1, 50, 50) == 0 && pixel(2, 50, 50) == 0, "Clear left stale drawing pixels");
  cairo_destroy(cr); cairo_surface_destroy(surface);
  std::cout << "Annotation mapping, alpha, eye selection, undo and clear passed\n";
}
