#include <chrono>
#include <moveit/move_group_interface/move_group_interface.h>
#include <moveit/planning_interface/planning_interface.h>
#include <moveit/robot_trajectory/robot_trajectory.h>
#include <moveit/trajectory_processing/time_optimal_trajectory_generation.h>

#include <moveit_msgs/msg/display_robot_state.hpp>
#include <moveit_msgs/msg/display_trajectory.hpp>

#include <moveit_msgs/msg/attached_collision_object.hpp>
#include <moveit_msgs/msg/collision_object.hpp>
#include <moveit_msgs/msg/robot_trajectory.hpp>

#include <moveit_visual_tools/moveit_visual_tools.h>
#include <move_group_server_interfaces/srv/move_to_state.hpp>
#include <move_group_server_interfaces/srv/move_to_pose.hpp>
#include <move_group_server_interfaces/srv/follow_path.hpp>
#include <move_group_server_interfaces/srv/follow_cartesian_path.hpp>
#include <std_srvs/srv/trigger.hpp>

#include <move_group_server_interfaces/srv/get_jacobian.hpp>
#include <Eigen/Dense>
#include <rclcpp/rclcpp.hpp>
#include <vector>

// All source files that use ROS logging should define a file-specific
// static const rclcpp::Logger named LOGGER, located at the top of the file
// and inside the namespace with the narrowest scope (if there is one)
static const rclcpp::Logger LOGGER = rclcpp::get_logger("move_group_server");

bool move_to_state(const std::shared_ptr<move_group_server_interfaces::srv::MoveToState::Request> request,
  const std::shared_ptr<move_group_server_interfaces::srv::MoveToState::Response> response,
  const std::shared_ptr<moveit::planning_interface::MoveGroupInterface> move_group,
  const std::shared_ptr<moveit_visual_tools::MoveItVisualTools> visual_tools,
  const moveit::core::JointModelGroup* joint_model_group)
{
  move_group->setStartStateToCurrentState();
  if (request->goal_state.joint_state.header.frame_id == "")
  {
    RCLCPP_WARN(LOGGER, "No frame id, malformed joint state");
    return false;
  }
  move_group->setJointValueTarget(request->goal_state.joint_state);
  move_group->setMaxVelocityScalingFactor(0.1);
  move_group->setMaxAccelerationScalingFactor(0.1);
  moveit::planning_interface::MoveGroupInterface::Plan my_plan;

  bool success = (move_group->plan(my_plan) == moveit::core::MoveItErrorCode::SUCCESS);
  RCLCPP_INFO(LOGGER, "Plan to state %s", success ? "SUCCESS" : "FAILED");
  if (success == false)
    return (response->state = success);

  visual_tools->deleteAllMarkers();
  visual_tools->publishTrajectoryLine(my_plan.trajectory_, joint_model_group);
  visual_tools->trigger();
  rclcpp::sleep_for(std::chrono::seconds(5));

  success = (move_group->execute(my_plan) == moveit::core::MoveItErrorCode::SUCCESS);
  RCLCPP_INFO(LOGGER, "Execute %s", success ? "SUCCESS" : "FAILED");
  if (!success)
  {
    int retry = 1;
    bool execute_success = false;
    while (!execute_success && retry < 4)
    {
      move_group->setStartStateToCurrentState();
      execute_success = (move_group->move() == moveit::core::MoveItErrorCode::SUCCESS);
      RCLCPP_INFO(LOGGER, "Replan %d and execute %s", retry, execute_success ? "SUCCESS" : "FAILED");
      success = execute_success;
      retry++;
    }
  }

  return (response->state = success);
}

bool move_to_pose(const std::shared_ptr<move_group_server_interfaces::srv::MoveToPose::Request> request,
  const std::shared_ptr<move_group_server_interfaces::srv::MoveToPose::Response> response,
  const std::shared_ptr<moveit::planning_interface::MoveGroupInterface> move_group,
  const std::shared_ptr<moveit_visual_tools::MoveItVisualTools> visual_tools,
  const moveit::core::JointModelGroup* joint_model_group)
{
  move_group->setStartStateToCurrentState();
  move_group->setPoseTarget(request->goal_state);
  
  move_group->setMaxVelocityScalingFactor(0.1);
  move_group->setMaxAccelerationScalingFactor(0.1);
  move_group->setPlanningTime(60.0);
  move_group->setNumPlanningAttempts(1000);
  move_group->setGoalTolerance(0.01);
  moveit::planning_interface::MoveGroupInterface::Plan my_plan;

  bool success = (move_group->plan(my_plan) == moveit::core::MoveItErrorCode::SUCCESS);
  RCLCPP_INFO(LOGGER, "Plan to pose %s", success ? "SUCCESS" : "FAILED");
  if (success == false)
    return (response->state = success);

  visual_tools->deleteAllMarkers();
  visual_tools->publishTrajectoryLine(my_plan.trajectory_, joint_model_group);
  visual_tools->trigger();
  rclcpp::sleep_for(std::chrono::seconds(10));

  success = (move_group->execute(my_plan) == moveit::core::MoveItErrorCode::SUCCESS);
  RCLCPP_INFO(LOGGER, "Execute %s", success ? "SUCCESS" : "FAILED");
  if (!success)
  {
    int retry = 1;
    bool execute_success = false;
    while (!execute_success && retry < 4)
    {
      move_group->setStartStateToCurrentState();
      execute_success = (move_group->move() == moveit::core::MoveItErrorCode::SUCCESS);
      RCLCPP_INFO(LOGGER, "Replan %d and execute %s", retry, execute_success ? "SUCCESS" : "FAILED");
      success = execute_success;
      retry++;
    }
  }

  return (response->state = success);
}

bool follow_path(const std::shared_ptr<move_group_server_interfaces::srv::FollowPath::Request> request,
  const std::shared_ptr<move_group_server_interfaces::srv::FollowPath::Response> response,
  const std::shared_ptr<moveit::planning_interface::MoveGroupInterface> move_group,
  const std::shared_ptr<moveit_visual_tools::MoveItVisualTools> visual_tools,
  const moveit::core::JointModelGroup* joint_model_group)
{
  RCLCPP_INFO_STREAM(LOGGER, "In follow path server");
  move_group->setStartStateToCurrentState();
  RCLCPP_INFO_STREAM(LOGGER, "Set Start State");
  if (request->robot_trajectory.joint_trajectory.header.frame_id == "")
  {
    RCLCPP_WARN(LOGGER, "No frame id, malformed joint state");
    return false;
  }

  RCLCPP_INFO_STREAM(LOGGER, "get current statw");
  auto currRobotState = move_group->getCurrentState(5.0);
  RCLCPP_INFO_STREAM(LOGGER, "set RT");
  robot_trajectory::RobotTrajectory rt(currRobotState->getRobotModel(), "ur_manipulator");
  rt.setRobotTrajectoryMsg(*currRobotState, request->robot_trajectory);

  RCLCPP_INFO_STREAM(LOGGER, "do TOTG");
  moveit_msgs::msg::RobotTrajectory trajectory_msg;
  trajectory_processing::TimeOptimalTrajectoryGeneration totg;
  bool success = totg.computeTimeStamps(rt, 0.1, 0.1);
  rt.getRobotTrajectoryMsg(trajectory_msg);
  RCLCPP_INFO_STREAM(LOGGER, "Parameterized trajectory length: " << trajectory_msg.joint_trajectory.points.size());
  moveit::planning_interface::MoveGroupInterface::Plan my_plan;
  rclcpp::sleep_for(std::chrono::seconds(1));
  my_plan.trajectory_ = trajectory_msg;

  if (success)
  {
    bool execute_success = (move_group->execute(my_plan) == moveit::core::MoveItErrorCode::SUCCESS);
    RCLCPP_INFO(LOGGER, "Trying to execute %s", execute_success ? "" : "FAILED");
    success = execute_success;
  }

  return (response->state = success);
}

bool follow_cartesian_path(
  const std::shared_ptr<move_group_server_interfaces::srv::FollowCartesianPath::Request> request,
  const std::shared_ptr<move_group_server_interfaces::srv::FollowCartesianPath::Response> response,
  const std::shared_ptr<moveit::planning_interface::MoveGroupInterface> move_group,
  const std::shared_ptr<moveit_visual_tools::MoveItVisualTools> visual_tools,
  const moveit::core::JointModelGroup* joint_model_group)
{
  RCLCPP_INFO_STREAM(LOGGER, "In follow Cartesian path server");

  // Set the start state to the current robot state
  move_group->setStartStateToCurrentState();
  RCLCPP_INFO_STREAM(LOGGER, "Set Start State");

  // Check if any poses were provided
  if (request->poses.empty()) {
    RCLCPP_WARN(LOGGER, "No poses provided for Cartesian path planning.");
    response->state = false;
    return false;
  }

  // Define waypoints for the Cartesian path
  std::vector<geometry_msgs::msg::Pose> waypoints;
  RCLCPP_INFO_STREAM(LOGGER, "Received " << request->poses.size() << " waypoints.");

  // Add the current pose of the end-effector as the starting pose
  waypoints.push_back(move_group->getCurrentPose().pose);

  // Add the target poses from the request to the waypoints
  for (const auto& pose : request->poses) {
    waypoints.push_back(pose);
  }

  // Compute the Cartesian path
  moveit_msgs::msg::RobotTrajectory cartesian_trajectory;
  double fraction = move_group->computeCartesianPath(waypoints, 0.01, 0.0, cartesian_trajectory);

  if (fraction < 1.0) {
    RCLCPP_WARN(LOGGER, "Unable to compute full Cartesian path. Fraction achieved: %f", fraction);
    response->state = false;
    return false;
  }

  RCLCPP_INFO_STREAM(LOGGER, "Cartesian path computed. Fraction achieved: " << fraction);

  // Time-optimal trajectory generation (TOTG) for smooth timing
  RCLCPP_INFO_STREAM(LOGGER, "Performing TOTG");
  auto currRobotState = move_group->getCurrentState(5.0);
  robot_trajectory::RobotTrajectory rt(currRobotState->getRobotModel(), "ur_manipulator");
  rt.setRobotTrajectoryMsg(*currRobotState, cartesian_trajectory);

  // Apply time-optimal trajectory generation (TOTG)
  trajectory_processing::TimeOptimalTrajectoryGeneration totg;
  bool success = totg.computeTimeStamps(rt, 0.1, 0.1);

  // Update the trajectory with time-optimized trajectory
  moveit_msgs::msg::RobotTrajectory trajectory_msg;
  rt.getRobotTrajectoryMsg(trajectory_msg);
  moveit::planning_interface::MoveGroupInterface::Plan my_plan;
  my_plan.trajectory_ = trajectory_msg;

  RCLCPP_INFO_STREAM(LOGGER, "Parameterized trajectory length: " << trajectory_msg.joint_trajectory.points.size());

  // Execute the Cartesian path
  if (success)
  {
    bool execute_success = (move_group->execute(my_plan) == moveit::core::MoveItErrorCode::SUCCESS);
    RCLCPP_INFO(LOGGER, "Trying to execute Cartesian Path %s: " , execute_success ? "SUCCESS" : "FAILED");
    success = execute_success;
  }

  response->state = success;
  return success;
}

Eigen::MatrixXd make_jacobian(const std::shared_ptr<moveit::planning_interface::MoveGroupInterface> move_group, 
                              const moveit::core::JointModelGroup* joint_model_group)
{
    moveit::core::RobotStatePtr current_state = move_group->getCurrentState();
    Eigen::Vector3d reference_point_position(0.0, 0.0, 0.0);
    Eigen::MatrixXd jacobian;

    current_state->getJacobian(joint_model_group,
          current_state->getLinkModel(joint_model_group->getLinkModelNames().back()),
            reference_point_position, jacobian);
 
    // RCLCPP_INFO_STREAM(LOGGER, "Current state: \n" <<jacobian << "\n");
    return jacobian;
}

void get_jacobian(const std::shared_ptr<move_group_server_interfaces::srv::GetJacobian::Request> request,
  std::shared_ptr<move_group_server_interfaces::srv::GetJacobian::Response> response,
  const std::shared_ptr<moveit::planning_interface::MoveGroupInterface> move_group,
  const moveit::core::JointModelGroup* joint_model_group)
{
  // RCLCPP_INFO_STREAM(LOGGER, "Jacobian request received");
  Eigen::MatrixXd jacobian = make_jacobian(move_group, joint_model_group);

  //convert the jacobian to a vector
  std::vector<float> vec(jacobian.data(), jacobian.data() + jacobian.rows() * jacobian.cols());
  response->jacobian = vec;
}

int main(int argc, char** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::NodeOptions node_options;
  node_options.automatically_declare_parameters_from_overrides(true);

  auto move_group_node = rclcpp::Node::make_shared("move_group_server", node_options);
  auto my_callback_group = move_group_node->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
  // rclcpp::SubscriptionOptions subs_options;
  // subs_options.callback_group = my_callback_group;

  // We spin up a MultiThreadedExecutor for the current state monitor to get information
  // about the robot's state so the services do not block
  rclcpp::executors::MultiThreadedExecutor executor;
  executor.add_node(move_group_node);
  std::thread t([&executor]()
    { executor.spin(); });

  RCLCPP_INFO(LOGGER, "Hello!");

  // MoveIt operates on sets of joints called "planning groups" and stores them in an object called
  // the ``JointModelGroup``. Throughout MoveIt, the terms "planning group" and "joint model group"
  // are used interchangeably.
  static const std::string PLANNING_GROUP = "ur_manipulator";

  auto move_group =
    std::make_shared<moveit::planning_interface::MoveGroupInterface>(move_group_node, PLANNING_GROUP);

  // Raw pointers are frequently used to refer to the planning group for improved performance.
  const moveit::core::JointModelGroup* joint_model_group =
    move_group->getCurrentState()->getJointModelGroup(PLANNING_GROUP);

  // Visualization
  // ^^^^^^^^^^^^^
  namespace rvt = rviz_visual_tools;
  auto visual_tools = std::make_shared<moveit_visual_tools::MoveItVisualTools>(move_group_node, "gripper_top_link",
    "move_group_server", move_group->getRobotModel());

  visual_tools->deleteAllMarkers();

  /* Remote control is an introspection tool that allows users to step through a high level script */
  /* via buttons and keyboard shortcuts in RViz */
  visual_tools->loadRemoteControl();

  // RViz provides many types of markers, in this demo we will use text, cylinders, and spheres
  Eigen::Isometry3d text_pose = Eigen::Isometry3d::Identity();
  text_pose.translation().z() = 1.0;
  visual_tools->publishText(text_pose, "Starting Sketch Controller", rvt::WHITE, rvt::XLARGE);

  // Batch publishing is used to reduce the number of messages being sent to RViz for large visualizations
  visual_tools->trigger();

  // We can print the name of the reference frame for this robot.
  RCLCPP_INFO(LOGGER, "Planning frame: %s", move_group->getPlanningFrame().c_str());

  // We can also print the name of the end-effector link for this group.
  RCLCPP_INFO(LOGGER, "End effector link: %s", move_group->getEndEffectorLink().c_str());

  // We can get a list of all the groups in the robot:
  RCLCPP_INFO(LOGGER, "Available Planning Groups:");
  std::copy(move_group->getJointModelGroupNames().begin(), move_group->getJointModelGroupNames().end(),
    std::ostream_iterator<std::string>(std::cout, ", "));
  
  //Setting up the services
  auto move_to_state_cb =
    [&](const std::shared_ptr<move_group_server_interfaces::srv::MoveToState::Request>& request,
      const std::shared_ptr<move_group_server_interfaces::srv::MoveToState::Response>& response) -> bool
    {
      return move_to_state(request, response, move_group, visual_tools, joint_model_group);
    };

  auto move_to_state_server =
    move_group_node->create_service<move_group_server_interfaces::srv::MoveToState>("move_group_server/move_to_state", move_to_state_cb,
      rmw_qos_profile_services_default, my_callback_group);

  auto move_to_pose_cb =
    [&](const std::shared_ptr<move_group_server_interfaces::srv::MoveToPose::Request>& request,
      const std::shared_ptr<move_group_server_interfaces::srv::MoveToPose::Response>& response) -> bool
    {
      return move_to_pose(request, response, move_group, visual_tools, joint_model_group);
    };

  auto move_to_pose_server =
    move_group_node->create_service<move_group_server_interfaces::srv::MoveToPose>("move_group_server/move_to_pose", move_to_pose_cb,
      rmw_qos_profile_services_default, my_callback_group);

  auto followpath_cb =
    [&](const std::shared_ptr<move_group_server_interfaces::srv::FollowPath::Request>& request,
      const std::shared_ptr<move_group_server_interfaces::srv::FollowPath::Response>& response) -> bool
    {
      return follow_path(request, response, move_group, visual_tools, joint_model_group);
    };

  auto followpath_server =
    move_group_node->create_service<move_group_server_interfaces::srv::FollowPath>("move_group_server/follow_path", followpath_cb,
      rmw_qos_profile_services_default, my_callback_group);

  auto follow_cartesian_path_cb =
    [&](const std::shared_ptr<move_group_server_interfaces::srv::FollowCartesianPath::Request>& request,
      const std::shared_ptr<move_group_server_interfaces::srv::FollowCartesianPath::Response>& response) -> bool
    {
      return follow_cartesian_path(request, response, move_group, visual_tools, joint_model_group);
    };

  auto follow_cartesian_path_server =
    move_group_node->create_service<move_group_server_interfaces::srv::FollowCartesianPath>("move_group_server/follow_cartesian_path",
      follow_cartesian_path_cb, rmw_qos_profile_services_default, my_callback_group);

  auto jacobian_cb =
    [&](const std::shared_ptr<move_group_server_interfaces::srv::GetJacobian::Request>& request,
      const std::shared_ptr<move_group_server_interfaces::srv::GetJacobian::Response>& response) -> void
    {
      return get_jacobian(request, response, move_group, joint_model_group);
    };

  auto jacobian_server = move_group_node->create_service<move_group_server_interfaces::srv::GetJacobian>("move_group_server/get_jacobian",
    jacobian_cb, rmw_qos_profile_services_default, my_callback_group);

  visual_tools->deleteAllMarkers();
  visual_tools->trigger();

  RCLCPP_INFO(LOGGER, "Move To State Server up");
  t.join();
  RCLCPP_INFO(LOGGER, "Join complete");
  rclcpp::shutdown(0);
  return 0;
}
