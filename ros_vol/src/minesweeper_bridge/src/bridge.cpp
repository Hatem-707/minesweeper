// Translates the control panel's ZeroMQ contract (protocol/include/
// minesweeper/messages.hpp) to ROS 2 topics and back. Translation only: no
// mapping or sensor fusion happens here. The robot is driven by RC, so the
// only command is ResetPose. See ROS_CONTRACT.md.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <minesweeper/messages.hpp>
#include <optional>
#include <string>
#include <zmq.hpp>

#include <geometry_msgs/msg/pose_stamped.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/u_int8.hpp>

using namespace std::chrono_literals;

namespace {
// Ages use the host CLOCK_MONOTONIC, not ROS time: the control panel compares
// them against its own freshness limits.
std::uint32_t
age_ms(std::int64_t since_ns, std::int64_t now_ns)
{
  if (since_ns == 0) {
    return msg::never;
  }
  const auto age = (now_ns - since_ns) / 1'000'000;
  return age < 0 ? 0 : std::uint32_t(std::min<std::int64_t>(age, msg::never - 1));
}

double
yaw_of(const geometry_msgs::msg::Quaternion& q)
{
  return std::atan2(2 * (q.w * q.z + q.x * q.y),
                    1 - 2 * (q.y * q.y + q.z * q.z));
}

void
bind_ipc(zmq::socket_t& socket, const std::string& endpoint)
{
  constexpr std::string_view scheme = "ipc://";
  if (endpoint.starts_with(scheme)) {
    const std::filesystem::path path = endpoint.substr(scheme.size());
    if (path.has_parent_path()) {
      std::filesystem::create_directories(path.parent_path());
    }
  }
  socket.bind(endpoint);
}
}

class Bridge : public rclcpp::Node
{
  zmq::context_t context;
  zmq::socket_t commands{ context, zmq::socket_type::pull };
  zmq::socket_t updates{ context, zmq::socket_type::pub };

  rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr reset_pose;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odometry;
  rclcpp::Subscription<std_msgs::msg::UInt8>::SharedPtr arm;
  rclcpp::Subscription<std_msgs::msg::UInt8>::SharedPtr metal;
  rclcpp::TimerBase::SharedPtr command_timer;
  rclcpp::TimerBase::SharedPtr telemetry_timer;

  // Latest robot state; *_ns are CLOCK_MONOTONIC receipt times, 0 = never.
  msg::Telemetry latest{};
  std::int64_t odometry_ns = 0;
  std::int64_t arm_ns = 0;
  std::int64_t metal_ns = 0;
  bool metal_now = false;
  bool metal_latched = false;

  std::uint32_t telemetry_seq = 0;

public:
  Bridge()
    : Node("minesweeper_bridge")
  {
    const auto command_endpoint = declare_parameter<std::string>(
      "command_endpoint", msg::command_endpoint);
    const auto telemetry_endpoint = declare_parameter<std::string>(
      "telemetry_endpoint", msg::telemetry_endpoint);
    commands.set(zmq::sockopt::rcvhwm, 64);
    commands.set(zmq::sockopt::linger, 0);
    updates.set(zmq::sockopt::sndhwm, 16);
    updates.set(zmq::sockopt::linger, 0);
    bind_ipc(commands, command_endpoint);
    bind_ipc(updates, telemetry_endpoint);

    reset_pose = create_publisher<geometry_msgs::msg::PoseStamped>(
      "/reset_pose", rclcpp::QoS(1).reliable().durability_volatile());

    odometry = create_subscription<nav_msgs::msg::Odometry>(
      "/odometry/filtered",
      rclcpp::QoS(1).best_effort(),
      [this](const nav_msgs::msg::Odometry& odom) { on_odometry(odom); });
    // Best effort matches both reliable and best-effort publishers. Any
    // nonzero value counts as 1.
    arm = create_subscription<std_msgs::msg::UInt8>(
      "/contact_switch",
      rclcpp::QoS(10).best_effort(),
      [this](const std_msgs::msg::UInt8& upright) {
        latest.arm_upright = upright.data != 0;
        arm_ns = msg::monotonic_ns();
      });
    metal = create_subscription<std_msgs::msg::UInt8>(
      "/metal_detector",
      rclcpp::QoS(20).best_effort(),
      [this](const std_msgs::msg::UInt8& detected) {
        metal_now = detected.data != 0;
        metal_latched = metal_latched || metal_now;
        metal_ns = msg::monotonic_ns();
      });

    command_timer = create_wall_timer(5ms, [this] { poll_commands(); });
    telemetry_timer = create_wall_timer(
      std::chrono::milliseconds(50), [this] { send_telemetry(); });
    RCLCPP_INFO(get_logger(),
                "Bridging %s and %s",
                command_endpoint.c_str(),
                telemetry_endpoint.c_str());
  }

private:
  void on_odometry(const nav_msgs::msg::Odometry& odom)
  {
    if (odom.header.frame_id != "map") {
      RCLCPP_WARN_ONCE(get_logger(),
                       "Odometry frame is '%s', expected 'map'",
                       odom.header.frame_id.c_str());
    }
    latest.x = odom.pose.pose.position.x;
    latest.y = odom.pose.pose.position.y;
    latest.heading_rad = yaw_of(odom.pose.pose.orientation);
    odometry_ns = msg::monotonic_ns();
  }

  void poll_commands()
  {
    zmq::message_t frame;
    while (commands.recv(frame, zmq::recv_flags::dontwait)) {
      handle_command(frame);
    }
  }

  void handle_command(const zmq::message_t& frame)
  {
    const auto now = msg::monotonic_ns();
    switch (msg::peek_type(frame).value_or(msg::MessageType{})) {
      case msg::MessageType::ResetPose:
        if (const auto reset = valid<msg::ResetPose>(frame, now)) {
          publish_reset(*reset);
        }
        break;
      default:
        RCLCPP_WARN_THROTTLE(
          get_logger(), *get_clock(), 1000, "Dropped unknown command");
    }
  }

  template<typename T>
  std::optional<T> valid(const zmq::message_t& frame, std::int64_t now)
  {
    auto message = msg::decode<T>(frame);
    if (!message) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 1000, "Dropped malformed command");
      return std::nullopt;
    }
    if (msg::is_expired(message->header, now)) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 1000, "Dropped expired command");
      return std::nullopt;
    }
    return message;
  }

  void publish_reset(const msg::ResetPose& reset)
  {
    const double yaw = std::uint8_t(reset.heading) * M_PI / 2;
    geometry_msgs::msg::PoseStamped pose;
    pose.header.stamp = now();
    pose.header.frame_id = "map";
    pose.pose.position.x = reset.x;
    pose.pose.position.y = reset.y;
    pose.pose.orientation.z = std::sin(yaw / 2);
    pose.pose.orientation.w = std::cos(yaw / 2);
    reset_pose->publish(pose);
    RCLCPP_INFO(get_logger(),
                "Reset pose to (%.2f, %.2f), heading %.0f deg",
                double(reset.x),
                double(reset.y),
                yaw * 180 / M_PI);
  }

  void send_telemetry()
  {
    const auto now = msg::monotonic_ns();
    auto message = latest;
    message.localization_age_ms = age_ms(odometry_ns, now);
    message.hardware_age_ms =
      arm_ns == 0 || metal_ns == 0 ? msg::never
                                   : age_ms(std::min(arm_ns, metal_ns), now);
    message.metal_detected = metal_latched;
    // A pulse between ticks was reported above; keep the current value.
    metal_latched = metal_now;
    (void)updates.send(msg::encode(message, telemetry_seq),
                       zmq::send_flags::dontwait);
  }
};

int
main(int argc, char** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<Bridge>());
  rclcpp::shutdown();
}
