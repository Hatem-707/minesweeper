#pragma once
#include <atomic>
#include <cstdint>
#include <gst/app/app.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <memory>
#include <raylib.h>
#include <thread>
#include <utility>

struct CameraBuffer
{
  alignas(64) std::atomic<bool> updated{ false };
  alignas(64) std::atomic<bool> reading{ false };
  std::pair<int, int> dimesions;
  std::unique_ptr<uint8_t[]> data;
};

struct GstData
{
  GstElement *pipeline, *source, *convert, *sink;
  GstBus* bus;
  GstMessage* msg;
  GstStateChangeReturn ret;
  GstData(CameraBuffer*);
  ~GstData();
  void run();
};

class GstCamera
{
  Texture2D texture;
  CameraBuffer buffer;
  GstData gst_data;
  std::jthread worker;

public:
  GstCamera(int width, int height);
  const Texture2D& get_texture();
  void release_texture();
};