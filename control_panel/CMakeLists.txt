cmake_minimum_required(VERSION 3.10)
project(control_panel CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
configure_file(styles.toml "${CMAKE_CURRENT_BINARY_DIR}/styles.toml" COPYONLY)
foreach(FONT_ASSET Roboto-Regular.ttf LICENSE-Roboto.txt)
  configure_file("assets/fonts/${FONT_ASSET}"
    "${CMAKE_CURRENT_BINARY_DIR}/assets/fonts/${FONT_ASSET}" COPYONLY)
endforeach()
foreach(ICON_ASSET turn-right.png LICENSE-Material-Icons.txt)
  configure_file("assets/icons/${ICON_ASSET}"
    "${CMAKE_CURRENT_BINARY_DIR}/assets/icons/${ICON_ASSET}" COPYONLY)
endforeach()

# Uncomment to download raylib
# include(FetchContent)
# set(FETCHCONTENT_QUIET FALSE)
# # don't build the supplied examples
# set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
# # don't build the supplied example games
# set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
# set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE) # enable wayland on linux
# # disable x11 on linux (switch these settings around as you please)
# set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)

# FetchContent_Declare(
#     raylib
#     GIT_REPOSITORY "https://github.com/raysan5/raylib.git"
#     GIT_TAG "master"
#     GIT_PROGRESS TRUE
# )

# FetchContent_MakeAvailable(raylib)

find_package(raylib REQUIRED)
find_package(fmt REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(ZeroMQ REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(gtk3 REQUIRED IMPORTED_TARGET gtk+-3.0)
pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
pkg_search_module(gstreamer-app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
pkg_search_module(
gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4
)

# Adding our source files
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")
set(PROJECT_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/include/")
# IPC contract shared with the ROS bridge and the vision process.
set(PROTOCOL_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/../protocol/include")

# Declaring our executable
add_executable(${PROJECT_NAME})
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
target_sources(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_INCLUDE} ${PROTOCOL_INCLUDE})
set(CONTROL_PANEL_LIBRARIES
                      raylib
                      fmt
                      PkgConfig::gtk3
                      PkgConfig::gstreamer
                      PkgConfig::gstreamer-app
                      PkgConfig::gstreamer-video
                      libzmq
)
target_link_libraries(${PROJECT_NAME} PRIVATE ${CONTROL_PANEL_LIBRARIES})

include(CTest)
if(BUILD_TESTING)
  find_package(GTest CONFIG REQUIRED)
  add_executable(controller_tests
    tests/drive_tests.cpp
    tests/pickup_tests.cpp
    tests/messages_tests.cpp
    tests/wire_tests.cpp
    src/pickup.cpp
    src/drive.cpp
    src/link.cpp
    src/wire.cpp
  )
  target_include_directories(controller_tests PRIVATE src ${PROTOCOL_INCLUDE})
  target_compile_options(controller_tests PRIVATE -Wall -Wextra -Wpedantic)
  target_link_libraries(controller_tests PRIVATE GTest::gtest_main libzmq)
  include(GoogleTest)
  gtest_discover_tests(controller_tests)
endif()
