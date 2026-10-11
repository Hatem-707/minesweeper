cmake_minimum_required(VERSION 3.16)
project(minesweeper_bridge CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(zmq REQUIRED IMPORTED_TARGET libzmq)
find_path(CPPZMQ_INCLUDE_DIR zmq.hpp REQUIRED)

# The IPC contract shared with the control panel. Inside Docker, mount the
# repository's protocol/ directory and point this at its include/ folder.
set(MINESWEEPER_PROTOCOL_INCLUDE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../protocol/include"
    CACHE PATH "Directory containing minesweeper/messages.hpp")
if(NOT EXISTS "${MINESWEEPER_PROTOCOL_INCLUDE}/minesweeper/messages.hpp")
  message(FATAL_ERROR
    "minesweeper/messages.hpp not found in ${MINESWEEPER_PROTOCOL_INCLUDE}; "
    "set -DMINESWEEPER_PROTOCOL_INCLUDE=<repo>/protocol/include")
endif()

add_executable(bridge src/bridge.cpp)
target_compile_options(bridge PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(bridge PRIVATE
  "${MINESWEEPER_PROTOCOL_INCLUDE}" "${CPPZMQ_INCLUDE_DIR}")
target_link_libraries(bridge PRIVATE
  rclcpp::rclcpp
  ${geometry_msgs_TARGETS}
  ${nav_msgs_TARGETS}
  ${std_msgs_TARGETS}
  PkgConfig::zmq)

install(TARGETS bridge DESTINATION lib/${PROJECT_NAME})

ament_package()
