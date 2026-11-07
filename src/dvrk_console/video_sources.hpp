#ifndef DVRK_CONSOLE_VIDEO_SOURCES_HPP
#define DVRK_CONSOLE_VIDEO_SOURCES_HPP

#include <dvrk_data/video_layout.hpp>
#include <string>
#include <vector>

struct VideoSource {
    std::string label;
    std::string socket_path;
    int eye = -1;  // physical half: 0 = first, 1 = second; -1 = mono
};

inline std::string video_source_label(const std::string &name) {
    const auto last = name.rfind(':');
    if (last == std::string::npos) return name;
    const auto mid = name.rfind(':', last - 1);
    if (mid == std::string::npos) return name.substr(last + 1);
    return name.substr(last + 1) + " (" + name.substr(mid + 1, last - mid - 1) + ")";
}

inline std::string video_source_key(const VideoSource &source) {
    return source.socket_path + "#" + std::to_string(source.eye);
}

inline std::vector<VideoSource> stereo_eye_sources(
    const std::string &name, dvrk_video::Layout layout = dvrk_video::Layout::Unknown) {
    using dvrk_video::Layout;
    const auto label = video_source_label(name);
    if (layout == Layout::Mono) return {{label, name, -1}};
    if (layout == Layout::Unknown) {
        // Compatibility with existing untagged native stereo streams.
        if (name != "@dvrk:stereo_source:stereo" && name != "@dvrk:stereo_alignment:stereo" &&
            name != "@dvrk:stereo_rectify:stereo" && name != "@dvrk:stereo_display:overlay")
            return {};
    }
    const int left = layout == Layout::SideBySideRL ? 1 : 0;
    return {{label + " :: left", name, left}, {label + " :: right", name, 1 - left}};
}

inline GstPadProbeReturn configure_stereo_eye_crop(GstPad *, GstPadProbeInfo *info, gpointer data) {
    auto *crop = GST_ELEMENT(data);
    GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
    if (GST_EVENT_TYPE(event) != GST_EVENT_CAPS) return GST_PAD_PROBE_OK;
    GstCaps *caps = nullptr;
    gst_event_parse_caps(event, &caps);
    const GstStructure *structure = gst_caps_get_structure(caps, 0);
    int width = 0;
    if (!gst_structure_get_int(structure, "width", &width) || width < 2 || width % 2)
        return GST_PAD_PROBE_DROP;
    const int eye = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(crop), "eye"));
    g_object_set(crop, "left", eye == 1 ? width / 2 : 0,
                "right", eye == 0 ? width / 2 : 0, nullptr);
    return GST_PAD_PROBE_OK;
}

#endif
