#ifndef DVRK_CONSOLE_STEREO_PIPELINE_HPP
#define DVRK_CONSOLE_STEREO_PIPELINE_HPP
#include <dvrk_data/config.hpp>
#include <dvrk_data/latest_video.hpp>
#include <string>
#include <vector>
namespace dvrk_console {
bool is_valid_sink(const std::string &sink);
std::string build_pipeline_string(const dvrk_data::AppConfig &config, bool include_overlay,
    std::vector<dvrk_video::LatestVideoSpec> *mixers = nullptr);
}
#endif
