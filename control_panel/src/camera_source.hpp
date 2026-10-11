#pragma once
#include <cstdint>
#include <gst/app/app.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <memory>
#include <stop_token>
#include <string>
#include <utility>

#include "spin_lock.hpp"

inline constexpr const char* default_camera_uri = "rtsp://127.0.0.1:8554/test";

struct CameraBuffer
{
  bool updated{ false };
  std::pair<int, int> dimesions; // Row bytes (RGBA), rows.
  std::int64_t frame_ns = 0;     // CLOCK_MONOTONIC when the frame arrived.
  std::unique_ptr<uint8_t[]> pixel_ptr;
};

struct GstData
{
  GstElement *pipeline, *source, *convert, *sink;
  GstBus* bus;
  GstMessage* msg;
  GstStateChangeReturn ret;
  SpinLock<CameraBuffer>& camera_buffer;
  GstData(SpinLock<CameraBuffer>& d_buf,
          const std::string& uri = default_camera_uri);
  void close_pipeline();
  ~GstData();
  void run(std::stop_token stop);
};
