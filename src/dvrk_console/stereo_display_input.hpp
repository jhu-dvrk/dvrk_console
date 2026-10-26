// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 Johns Hopkins University, LCSR
#pragma once

#include <dvrk_data/dvrk_gst_socket.hpp>
#include <stdexcept>
#include <string>

namespace dvrk_console {

// Hints select a default; a configured gst_input always takes precedence.
inline std::string stereo_display_input(const std::string &configured,
                                        const std::string &hint) {
  auto valid_component = [](const std::string &value) {
    return !value.empty() && value.front() != '-' && value.find_first_not_of(
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") == std::string::npos;
  };
  std::string socket;
  if (hint.rfind("@dvrk:", 0) == 0) {
    const auto split = hint.find(':', 6);
    if (split == std::string::npos || !valid_component(hint.substr(6, split - 6)) ||
        !valid_component(hint.substr(split + 1)))
      throw std::invalid_argument("--gst-hint expects a role or @dvrk:<role>:<name> socket");
    socket = hint;
  } else {
    if (!valid_component(hint))
      throw std::invalid_argument("--gst-hint expects a role such as stereo_rectify");
    socket = dvrk_gst::make(hint, "stereo");
  }
  return configured.empty() ? socket : configured;
}

} // namespace dvrk_console
