#include "video_sources.hpp"
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <stdexcept>
#include <iostream>

int main(int argc, char **argv) {
    gst_init(&argc, &argv);
    using dvrk_video::Layout;
    const std::string socket = "@dvrk:simulator:custom_camera";
    auto check = [](bool ok) { if (!ok) throw std::runtime_error("Incorrect preview eye selection"); };
    check(stereo_eye_sources(socket).empty());
    const auto mono = stereo_eye_sources(socket, Layout::Mono);
    check(mono.size() == 1 && mono[0].eye == -1);
    for (const auto layout : {Layout::SideBySideLR, Layout::SideBySideRL}) {
        const auto eyes = stereo_eye_sources(socket, layout);
        check(eyes.size() == 2);
        check(eyes[0].eye == (layout == Layout::SideBySideRL ? 1 : 0));
        check(eyes[1].eye == 1 - eyes[0].eye);
        check(eyes[0].label.find("left") != std::string::npos);
        check(eyes[1].label.find("right") != std::string::npos);
        check(video_source_key(eyes[0]) != video_source_key(eyes[1]));
        for (int eye = 0; eye < 2; ++eye) {
            auto *pipeline = gst_parse_launch(
                "appsrc name=input is-live=true format=time ! videocrop name=crop "
                "! appsink name=output sync=false", nullptr);
            auto *input = gst_bin_get_by_name(GST_BIN(pipeline), "input");
            auto *output = gst_bin_get_by_name(GST_BIN(pipeline), "output");
            auto *crop = gst_bin_get_by_name(GST_BIN(pipeline), "crop");
            g_object_set_data(G_OBJECT(crop), "eye", GINT_TO_POINTER(eyes[eye].eye));
            auto *pad = gst_element_get_static_pad(crop, "sink");
            gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, configure_stereo_eye_crop, crop, nullptr);
            gst_object_unref(pad);
            auto *caps = gst_caps_from_string(
                "video/x-raw,format=RGBA,width=32,height=4,framerate=30/1,dvrk-video-layout=stereo-LR");
            gst_app_src_set_caps(GST_APP_SRC(input), caps);
            gst_caps_unref(caps);
            gst_element_set_state(pipeline, GST_STATE_PLAYING);
            auto *buffer = gst_buffer_new_allocate(nullptr, 32 * 4 * 4, nullptr);
            GstMapInfo pixels;
            check(gst_buffer_map(buffer, &pixels, GST_MAP_WRITE));
            for (int y = 0; y < 4; ++y) for (int x = 0; x < 32; ++x) {
                const bool left_eye = (x < 16) == (layout == Layout::SideBySideLR);
                const int offset = (y * 32 + x) * 4;
                pixels.data[offset] = left_eye ? 255 : 0;
                pixels.data[offset + 1] = 0;
                pixels.data[offset + 2] = left_eye ? 0 : 255;
                pixels.data[offset + 3] = 255;
            }
            gst_buffer_unmap(buffer, &pixels);
            GST_BUFFER_PTS(buffer) = 0;
            check(gst_app_src_push_buffer(GST_APP_SRC(input), buffer) == GST_FLOW_OK);
            auto *sample = gst_app_sink_try_pull_sample(GST_APP_SINK(output), GST_SECOND);
            check(sample != nullptr);
            GstVideoInfo video;
            check(gst_video_info_from_caps(&video, gst_sample_get_caps(sample)) && video.width == 16);
            check(gst_buffer_map(gst_sample_get_buffer(sample), &pixels, GST_MAP_READ));
            check(pixels.data[0] == (eye == 0 ? 255 : 0));
            check(pixels.data[2] == (eye == 1 ? 255 : 0));
            gst_buffer_unmap(gst_sample_get_buffer(sample), &pixels);
            gst_sample_unref(sample);
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(crop);
            gst_object_unref(input);
            gst_object_unref(output);
            gst_object_unref(pipeline);
        }
    }
    check(stereo_eye_sources("@dvrk:stereo_display:overlay").size() == 2);
    std::cout << "Simulator mono/LR/RL preview selection and legacy streams passed\n";
}
