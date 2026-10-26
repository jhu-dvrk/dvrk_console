import os

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction, TimerAction
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from dvrk_data.stereo_pipeline_selection import select_processor, output_socket, color_config


DEFAULT_SOURCE_CONFIG = "stereo_source.json"
DEFAULT_ALIGNMENT_CONFIG = "stereo_alignment.json"
DEFAULT_DISPLAY_CONFIG = "stereo_display.json"


def resolve_config_path(base_dir, value, default_name):
    value = value.strip() or default_name
    value = os.path.expanduser(value)
    if os.path.isabs(value):
        return value
    return os.path.join(base_dir, value)


def launch_setup(context, *args, **kwargs):
    system = LaunchConfiguration("system").perform(context).strip()
    config_parent = LaunchConfiguration("config_parent").perform(context).strip()
    config_dir = LaunchConfiguration("config_dir").perform(context).strip()

    if system:
        if not config_parent:
            raise RuntimeError(
                'Launch argument "config_parent" is required when "system" is set.'
            )
        base_dir = os.path.join(os.path.expanduser(config_parent), system)
    else:
        base_dir = os.path.expanduser(config_dir) if config_dir else os.getcwd()

    base_dir = os.path.abspath(base_dir)
    source_config_path = resolve_config_path(
        base_dir,
        LaunchConfiguration("source_config").perform(context),
        DEFAULT_SOURCE_CONFIG,
    )
    executable, processor_config_path = select_processor(
        base_dir,
        LaunchConfiguration("processor").perform(context),
        LaunchConfiguration("alignment_config").perform(context),
        LaunchConfiguration("rectify_config").perform(context),
    )
    processor_config_path = str(processor_config_path)
    colors = color_config(base_dir, LaunchConfiguration("color_config").perform(context))
    display_config_path = resolve_config_path(
        base_dir,
        LaunchConfiguration("display_config").perform(context),
        DEFAULT_DISPLAY_CONFIG,
    )

    missing = [
        path
        for path in (source_config_path, processor_config_path, display_config_path)
        if not os.path.exists(path)
    ]
    if missing:
        raise RuntimeError("Missing configuration file(s): " + ", ".join(missing))

    source_node = Node(
        package="dvrk_data",
        executable="stereo_source",
        name="stereo_source",
        output="screen",
        arguments=["-c", source_config_path],
    )

    processor_arguments = ["-c", processor_config_path, "-i",
                           output_socket(source_config_path, "@dvrk:stereo_source:stereo")]
    if colors:
        processor_arguments += ["--color-adjustment", str(colors)]
    processor_node = Node(
        package="dvrk_data",
        executable=executable,
        name=executable,
        output="screen",
        arguments=processor_arguments,
    )
    default_socket = "@dvrk:" + executable + ":stereo"
    processor_output = output_socket(processor_config_path, default_socket)
    hint = executable if processor_output == default_socket else processor_output

    display_node = Node(
        package="dvrk_console",
        executable="stereo_display",
        name="stereo_display",
        output="screen",
        arguments=["-c", display_config_path, "--gst-hint", hint],
    )

    return [
        source_node,
        TimerAction(period=2.0, actions=[processor_node]),
        TimerAction(period=4.0, actions=[display_node]),
    ]


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            "config_dir",
            default_value=os.getcwd(),
            description="Directory containing config files. Defaults to the current working directory.",
        ),
        DeclareLaunchArgument(
            "system",
            default_value="",
            description="Optional system directory name under config_parent, e.g. jhu-daVinci.",
        ),
        DeclareLaunchArgument(
            "config_parent",
            default_value="",
            description="Parent directory containing system config directories. Required when system is set.",
        ),
        DeclareLaunchArgument(
            "source_config",
            default_value=DEFAULT_SOURCE_CONFIG,
            description="dvrk_data stereo_source config filename or absolute path.",
        ),
        DeclareLaunchArgument(
            "processor",
            default_value="auto",
            choices=["auto", "alignment", "rectify"],
            description="Geometry processor; auto requires exactly one geometry config.",
        ),
        DeclareLaunchArgument(
            "rectify_config",
            default_value="stereo_rectify.json",
            description="dvrk_data stereo_rectify config filename or absolute path.",
        ),
        DeclareLaunchArgument(
            "color_config",
            default_value="",
            description="Optional shared color adjustment config filename or absolute path.",
        ),
        DeclareLaunchArgument(
            "alignment_config",
            default_value=DEFAULT_ALIGNMENT_CONFIG,
            description="dvrk_data stereo_alignment config filename or absolute path.",
        ),
        DeclareLaunchArgument(
            "display_config",
            default_value=DEFAULT_DISPLAY_CONFIG,
            description="dvrk_console stereo_display config filename or absolute path.",
        ),
        OpaqueFunction(function=launch_setup),
    ])
