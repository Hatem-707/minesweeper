#pragma once
#include <cstdint>
#include <gst/app/app.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <memory>
#include <raylib.h>
#include <thread>
#include <utility>

#include "spin_lock.hpp"

struct CameraBuffer
{
  bool updated{ false };
  std::pair<int, int> dimesions;
  std::unique_ptr<uint8_t[]> data;
};

struct GstData
{
  GstElement *pipeline, *source, *convert, *sink;
  GstBus* bus;
  GstMessage* msg;
  GstStateChangeReturn ret;
  SpinLock<CameraBuffer>& camera_buffer;
  GstData(SpinLock<CameraBuffer>& d_buf);
  bool should_close{ false };
  void close_pipeline();
  ~GstData();
  void run();
};

class GstCamera
{
  Texture2D texture;
  SpinLock<CameraBuffer> buffer;
  GstData gst_data;
  std::jthread worker;

public:
  GstCamera(int width, int height);
  ~GstCamera();
  const Texture2D& get_texture();
};