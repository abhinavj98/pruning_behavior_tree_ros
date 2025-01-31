#!/usr/bin/env python3
import launch
from launch import LaunchDescription
from launch.actions import (
    IncludeLaunchDescription,
    DeclareLaunchArgument,
    TimerAction,
    SetLaunchConfiguration,
    EmitEvent,
    ExecuteProcess,
)
from launch.launch_description_sources import AnyLaunchDescriptionSource
from launch.conditions import IfCondition, UnlessCondition, LaunchConfigurationEquals
from launch.substitutions import LaunchConfiguration, PythonExpression, AndSubstitution
from ament_index_python.packages import get_package_share_directory
from launch_ros.actions import Node
import os


def generate_launch_description():
    ur_type = LaunchConfiguration("ur_type")
    robot_ip = LaunchConfiguration("robot_ip")
    use_fake_hardware = LaunchConfiguration("use_fake_hardware")
    headless_mode = LaunchConfiguration("headless_mode", default="true")

    ur_type_arg = DeclareLaunchArgument(
        "ur_type", default_value="ur5e", description="Robot description name (consistent with ur_control.launch.py)"
    )
    robot_ip_arg = DeclareLaunchArgument("robot_ip", default_value="169.254.177.220", description="Robot IP")

    use_fake_hardware_arg = DeclareLaunchArgument(
        "use_fake_hardware", default_value="false", description="If true, uses the fake controllers"
    )
    
    headless_mode_arg = DeclareLaunchArgument(
        "headless_mode", default_value="true", description="Run UR5 in headless mode"
    )

    package_dir = get_package_share_directory("pruning_bt")
    params_path = os.path.join(package_dir, "config")

    ur_pruner_startup_launch = IncludeLaunchDescription(
        AnyLaunchDescriptionSource(os.path.join(package_dir, "launch", "ur_pruner_startup.launch.py")),
        launch_arguments = {
            'ur_type': ur_type,
            'robot_ip': robot_ip,
            'use_fake_hardware': use_fake_hardware,
            'headless_mode': headless_mode,
        }.items()
    )

    realsense_launch = IncludeLaunchDescription(
        AnyLaunchDescriptionSource(
            os.path.join(get_package_share_directory("realsense2_camera"), "launch/rs_launch.py")
        ),
        launch_arguments=[
            ("enable_depth", "false"),
            ("pointcloud.enable", "false"),
            ("rgb_camera.color_profile", "424x240x30"),
        ],
        # condition=UnlessCondition(use_sim),  # TODO: add unless condition for other camera makers
    )

    move_group_server_launch = IncludeLaunchDescription(
        AnyLaunchDescriptionSource(
            os.path.join(get_package_share_directory("move_group_server"), "launch", "move_group_server.launch.py")
        ),
        launch_arguments={
            "use_sim": "false",
            "use_fake_hardware": use_fake_hardware,
            "headless_mode": headless_mode,
            "robot_ip": robot_ip,
            "ur_type": ur_type,
            "launch_rviz": "true",
        }.items(),
    )

    delay_launch_move_group_server = TimerAction(
        period=10.0,
        actions=[move_group_server_launch], # We delay the launch of move_group_server to ensure that moveit is ready
    )

    #Simulation for RL policy has the robot rotated
    # tf_node_world_sim = Node(
    #     package="tf2_ros",
    #     executable="static_transform_publisher",

    #     prefix=['bash -c \'sleep 7; $0 $@\''],
    #     arguments="0. 0. 0. 0. 0. -0.707 0.707 fake_base world".split(' '),
    # )

    # tf_node_gripper_endpoint = Node(
    #     package="tf2_ros",
    #     executable="static_transform_publisher",
    #     arguments="-0.0 0.035 0.275 0. 0. 0. gripper_link endpoint".split(' '),
    # )
    joy_node = Node(
        package="joy",
        executable="joy_node",
        name="joy_node",
    )

    io_manager = Node(
        package="pruning_bt",
        executable="io_manager",
        name="io_manager",
    )

    set_goal_as_endpoint_service = Node(   
            package="pruning_bt",
            executable="set_goal_as_endpoint_service",
            name="set_goal_as_endpoint_service",
        )

    # tf_node_mount = Node(
    #     package="tf2_ros",
    #     executable="static_transform_publisher",
    #     arguments="0 0.08 0.173 0 0 1 0 tool0 camera_mount_center".split(),
    #     condition=UnlessCondition(use_fake_hardware),
    # )

    tf_node_mount_to_cam = Node(
        package="tf2_ros",
        executable="static_transform_publisher",
        arguments="0.0193 0.009 0. 0. 0. 0. 1. mock_pruner__camera0 camera_link".split(),  # Z is camera thickness (23mm) minus glass (3.7mm)
        condition=UnlessCondition(use_fake_hardware),
    )

    pub_pcl_node = Node(
        package="pruning_bt",
        executable="pub_pcl",   
    )

    rviz_point_selection_node = Node(
        package="pruning_bt",
        executable="rviz_point_selection",
    )

    return LaunchDescription(
        [
            ur_type_arg,
            robot_ip_arg,
            use_fake_hardware_arg,
            headless_mode_arg,
            ur_pruner_startup_launch,
            realsense_launch,
            # tf_node_world_sim,   
            delay_launch_move_group_server,
            joy_node,
            io_manager,
            set_goal_as_endpoint_service,
            # teleop_node,
            # tf_node_tool0_camera,
            # tf_node_gripper_endpoint,
            # tf_node_mount,
            tf_node_mount_to_cam,
            pub_pcl_node,
            rviz_point_selection_node
        ]
    )
