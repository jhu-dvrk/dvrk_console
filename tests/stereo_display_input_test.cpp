#include "stereo_display_input.hpp"
#include <iostream>
#include <stdexcept>

int main() {
  using dvrk_console::stereo_display_input;
  auto check = [](bool ok) {
    if (!ok) throw std::runtime_error("Display input precedence or validation failed");
  };
  check(stereo_display_input("", "stereo_alignment") == "@dvrk:stereo_alignment:stereo");
  check(stereo_display_input("", "stereo_rectify") == "@dvrk:stereo_rectify:stereo");
  check(stereo_display_input("", "@dvrk:stereo_rectify:custom") == "@dvrk:stereo_rectify:custom");
  check(stereo_display_input("@dvrk:custom:stereo", "stereo_rectify") == "@dvrk:custom:stereo");
  check(stereo_display_input("videotestsrc is-live=true", "stereo_rectify") == "videotestsrc is-live=true");
  for (const auto *invalid : {"", "stereo rectify", "--grid", "@dvrk::stereo",
                               "@dvrk:stereo_rectify:", "@dvrk:stereo_rectify", "stereo_rectify ! fakesink"}) {
    bool rejected = false;
    try { stereo_display_input("", invalid); }
    catch (const std::invalid_argument &) { rejected = true; }
    check(rejected);
  }
  std::cout << "Default roles, custom sockets, explicit-config precedence and invalid hints passed\n";
}
