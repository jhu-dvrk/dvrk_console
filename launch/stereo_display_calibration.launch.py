"""Start source, alignment, then interactive stereo display calibration.

Run from a directory containing stereo_source.json, stereo_alignment.json,
and stereo_display.json::

    ros2 launch dvrk_console stereo_display_calibration.launch.py
"""

import os

from ament_index_python.packages import get_package_prefix
from launch import LaunchDescription
from launch.actions import (DeclareLaunchArgument, ExecuteProcess, OpaqueFunction,
                            RegisterEventHandler, Shutdown, TimerAction)
from launch.event_handlers import OnProcessExit
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


DEFAULT_SOURCE_CONFIG = "stereo_source.json"
DEFAULT_ALIGNMENT_CONFIG = "stereo_alignment.json"
DEFAULT_DISPLAY_CONFIG = "stereo_display.json"


def _config_path(base_dir, value):
    value = os.path.expanduser(value.strip())
    return value if os.path.isabs(value) else os.path.join(base_dir, value)


def _launch_processes(context, *args, **kwargs):
    config_dir = os.path.abspath(os.path.expanduser(
        LaunchConfiguration("config_dir").perform(context)))
    source_path = _config_path(
        config_dir, LaunchConfiguration("source_config").perform(context))
    alignment_path = _config_path(
        config_dir, LaunchConfiguration("alignment_config").perform(context))
    display_path = _config_path(
        config_dir, LaunchConfiguration("display_config").perform(context))

    missing = [path for path in (source_path, alignment_path, display_path)
               if not os.path.isfile(path)]
    if missing:
        raise RuntimeError("Missing configuration file(s): " + ", ".join(missing))

    executable = os.path.join(
        get_package_prefix("dvrk_console"), "lib", "dvrk_console",
        "stereo_display_calibration")
    calibration = ExecuteProcess(
        cmd=[executable, "-c", display_path],
        output="screen",
    )
    return [
        Node(
            package="dvrk_data",
            executable="stereo_source",
            name="stereo_source",
            output="screen",
            arguments=["-c", source_path],
        ),
        TimerAction(period=2.0, actions=[
            Node(
                package="dvrk_data",
                executable="stereo_alignment",
                name="stereo_alignment",
                output="screen",
                arguments=["-c", alignment_path],
            ),
        ]),
        TimerAction(period=4.0, actions=[calibration]),
        RegisterEventHandler(OnProcessExit(
            target_action=calibration,
            on_exit=[Shutdown(reason="Display calibration finished")],
        )),
    ]


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            "config_dir", default_value=os.getcwd(),
            description="Directory containing the three configs; defaults to the current working directory.",
        ),
        DeclareLaunchArgument(
            "source_config", default_value=DEFAULT_SOURCE_CONFIG,
            description="Source config filename relative to config_dir, or absolute path.",
        ),
        DeclareLaunchArgument(
            "alignment_config", default_value=DEFAULT_ALIGNMENT_CONFIG,
            description="Alignment config filename relative to config_dir, or absolute path.",
        ),
        DeclareLaunchArgument(
            "display_config", default_value=DEFAULT_DISPLAY_CONFIG,
            description="Display config filename relative to config_dir, or absolute path.",
        ),
        OpaqueFunction(function=_launch_processes),
    ])
