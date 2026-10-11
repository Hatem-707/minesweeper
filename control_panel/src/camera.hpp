#pragma once
#include <raylib.h>
#include <thread>

#include "camera_source.hpp"
#include "spin_lock.hpp"

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
