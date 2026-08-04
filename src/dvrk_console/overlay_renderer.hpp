#ifndef DVRK_DISPLAY_OVERLAY_RENDERER_HPP
#define DVRK_DISPLAY_OVERLAY_RENDERER_HPP

#include "overlay_state.hpp"

#include <memory>

typedef struct _GstElement GstElement;

namespace dvrk_console {

// Connects the caps and draw callbacks to one cairooverlay element.  Keeping
// the callback signatures here avoids exposing Cairo and GStreamer details to
// the application-level pipeline wiring.
void attach_overlay_callbacks(
    GstElement *overlay,
    const std::shared_ptr<OverlayState> &overlay_state);

}  // namespace dvrk_console

#endif
