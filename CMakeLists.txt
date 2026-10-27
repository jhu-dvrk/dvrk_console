cmake_minimum_required(VERSION 3.16)
project(dvrk_console VERSION 1.0.0)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(crtk_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(dvrk_data REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-video-1.0 gstreamer-app-1.0)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)
pkg_check_modules(X11 REQUIRED x11 xi)

add_library(dvrk_console_overlay
  src/dvrk_console/overlay.cpp
  src/dvrk_console/overlay_utils.cpp
  src/dvrk_console/overlay_components.cpp
)

add_library(dvrk_console_ui
  src/dvrk_console/display_output_panel.cpp
)
target_include_directories(dvrk_console_ui PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dvrk_console
  ${GTKMM_INCLUDE_DIRS}
)
target_compile_options(dvrk_console_ui PRIVATE
  ${GTKMM_CFLAGS_OTHER}
)
target_link_libraries(dvrk_console_ui dvrk_data::dvrk_data)
target_link_libraries(dvrk_console_ui ${GTKMM_LIBRARIES})

add_library(dvrk_console_runtime
  src/dvrk_console/stereo_pipeline.cpp
  src/dvrk_console/glib_ros_executor.cpp
  src/dvrk_console/gst_pipeline_runtime.cpp
)
target_include_directories(dvrk_console_runtime PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dvrk_console
  ${GST_INCLUDE_DIRS}
)
target_compile_options(dvrk_console_runtime PRIVATE
  ${GST_CFLAGS_OTHER}
)
target_link_libraries(dvrk_console_runtime
  rclcpp::rclcpp
  dvrk_data::dvrk_data
)
target_link_libraries(dvrk_console_runtime ${GST_LIBRARIES})
target_include_directories(dvrk_console_overlay PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dvrk_console
  ${GST_INCLUDE_DIRS}
  ${JSONCPP_INCLUDE_DIRS}
  ${CAIRO_INCLUDE_DIRS}
  ${GTKMM_INCLUDE_DIRS}
)
target_compile_options(dvrk_console_overlay PRIVATE
  ${GST_CFLAGS_OTHER}
  ${JSONCPP_CFLAGS_OTHER}
  ${CAIRO_CFLAGS_OTHER}
  ${GTKMM_CFLAGS_OTHER}
)
target_link_libraries(dvrk_console_overlay
  rclcpp::rclcpp
  geometry_msgs::geometry_msgs
  sensor_msgs::sensor_msgs
  std_msgs::std_msgs
)
target_link_libraries(dvrk_console_overlay
  ${GST_LIBRARIES}
  ${JSONCPP_LIBRARIES}
  ${CAIRO_LIBRARIES}
  ${GTKMM_LIBRARIES}
)

add_executable(stereo_display
  src/dvrk_console/main_stereo_display.cpp
)
add_executable(control_panel
  src/dvrk_console/main_control_panel.cpp
  src/dvrk_console/annotation_panel.cpp
)

target_link_libraries(stereo_display
  rclcpp::rclcpp
  diagnostic_msgs::diagnostic_msgs
  geometry_msgs::geometry_msgs
  sensor_msgs::sensor_msgs
  std_msgs::std_msgs
  dvrk_data::dvrk_data
)
target_link_libraries(control_panel
  rclcpp::rclcpp
  ${crtk_msgs_TARGETS}
  std_msgs::std_msgs
  dvrk_data::dvrk_data
)

foreach(exe stereo_display control_panel)
  target_include_directories(${exe} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dvrk_console
    ${GST_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${GTKMM_INCLUDE_DIRS}
  )
  target_compile_options(${exe} PRIVATE
    ${GST_CFLAGS_OTHER}
    ${JSONCPP_CFLAGS_OTHER}
    ${CAIRO_CFLAGS_OTHER}
    ${GTKMM_CFLAGS_OTHER}
  )
  target_link_libraries(${exe}
    ${GST_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${GTKMM_LIBRARIES}
  )
endforeach()

target_link_libraries(stereo_display
  dvrk_console_overlay dvrk_console_ui dvrk_console_runtime)
target_link_libraries(control_panel dvrk_console_runtime)

target_include_directories(control_panel PRIVATE ${X11_INCLUDE_DIRS})
target_compile_options(control_panel PRIVATE ${X11_CFLAGS_OTHER})
target_link_libraries(control_panel ${X11_LIBRARIES})

install(TARGETS
  stereo_display
  control_panel
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(PROGRAMS
  scripts/stereo_display_calibration
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY share/config
  DESTINATION share/${PROJECT_NAME}
)

install(DIRECTORY launch/
  DESTINATION share/${PROJECT_NAME}/launch
)

if(BUILD_TESTING)
  enable_testing()
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  add_test(
    NAME stereo_display_calibration
    COMMAND ${CMAKE_COMMAND} -E env
      "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/../dvrk_data:$ENV{PYTHONPATH}"
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/stereo_display_calibration_test.py
  )
  add_executable(stereo_display_input_test tests/stereo_display_input_test.cpp)
  target_include_directories(stereo_display_input_test PRIVATE src/dvrk_console)
  target_link_libraries(stereo_display_input_test dvrk_data::dvrk_data)
  add_test(NAME stereo_display_input COMMAND stereo_display_input_test)
  add_test(NAME stereo_video_launch COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/stereo_video_launch_test.py)
  add_executable(stereo_pipeline_test tests/stereo_pipeline_test.cpp)
  target_include_directories(stereo_pipeline_test PRIVATE src/dvrk_console ${GST_INCLUDE_DIRS})
  target_link_libraries(stereo_pipeline_test dvrk_console_runtime dvrk_data::dvrk_data ${GST_LIBRARIES})
  add_test(NAME stereo_pipeline COMMAND stereo_pipeline_test)
  set_tests_properties(stereo_pipeline PROPERTIES TIMEOUT 20)
  add_executable(frame_timing_test tests/frame_timing_test.cpp)
  target_include_directories(frame_timing_test PRIVATE src/dvrk_console ${GST_INCLUDE_DIRS})
  target_link_libraries(frame_timing_test dvrk_data::dvrk_data diagnostic_msgs::diagnostic_msgs ${GST_LIBRARIES})
  add_test(NAME frame_timing COMMAND frame_timing_test)
  add_executable(annotation_model_test tests/annotation_model_test.cpp)
  target_include_directories(annotation_model_test PRIVATE src/dvrk_console ${CAIRO_INCLUDE_DIRS})
  target_link_libraries(annotation_model_test ${CAIRO_LIBRARIES})
  add_test(NAME annotation_model COMMAND annotation_model_test)
  add_executable(widget_disparity_test tests/widget_disparity_test.cpp)
  target_include_directories(widget_disparity_test PRIVATE src/dvrk_console)
  target_link_libraries(widget_disparity_test dvrk_data::dvrk_data rclcpp::rclcpp std_msgs::std_msgs)
  add_test(NAME widget_disparity COMMAND widget_disparity_test)
  set_tests_properties(widget_disparity PROPERTIES TIMEOUT 15)
  add_executable(optional_ar_input_test tests/optional_ar_input_test.cpp)
  target_include_directories(optional_ar_input_test PRIVATE src/dvrk_console ${GST_INCLUDE_DIRS} ${GTKMM_INCLUDE_DIRS})
  target_link_libraries(optional_ar_input_test dvrk_console_runtime dvrk_data::dvrk_data rclcpp::rclcpp ${GST_LIBRARIES} ${GTKMM_LIBRARIES})
  add_test(NAME optional_ar_input COMMAND optional_ar_input_test)
  set_tests_properties(optional_ar_input PROPERTIES TIMEOUT 25)
  add_executable(annotation_panel_test tests/annotation_panel_test.cpp src/dvrk_console/annotation_panel.cpp)
  target_include_directories(annotation_panel_test PRIVATE src/dvrk_console ${GST_INCLUDE_DIRS} ${GTKMM_INCLUDE_DIRS})
  target_link_libraries(annotation_panel_test dvrk_console_runtime dvrk_data::dvrk_data rclcpp::rclcpp std_msgs::std_msgs ${GST_LIBRARIES} ${GTKMM_LIBRARIES})
  add_test(NAME annotation_panel COMMAND annotation_panel_test)
  set_tests_properties(annotation_panel PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77)
endif()

ament_package()
