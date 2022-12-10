from launch import LaunchDescription
from launch_ros.actions import Node
import os
from ament_index_python import get_package_share_directory
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration


def generate_launch_description():
    use_sim_time = LaunchConfiguration("use_sim_time")

    declare_use_sim_time_cmd = DeclareLaunchArgument(
        "use_sim_time", default_value="False", description="Use simulation clock if True"
    )

    package_name = "tensorrt_yolov7"

    engine_path = os.path.join(get_package_share_directory(package_name), "engines", "something.engine")
    config = os.path.join(get_package_share_directory(package_name), "param", "tensorrt_yolov7.param.yaml")

    return LaunchDescription(
        [
            declare_use_sim_time_cmd,
            Node(
                package="race_path_planner",
                executable="race_path_planner_node_exe",
                parameters=[config, use_sim_time, {"engine_path": engine_path}],
                remappings=[
                    ("in/image", "/vimba_front_left_center/image"),
                    ("out/image", "/vimba_front_left_center/det_image"),
                    ("out/objects", "/tier4_detections"),
                ],
            ),
        ]
    )
