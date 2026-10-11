#include "camera.hpp"
#include <raylib.h>

GstCamera::GstCamera(int width, int height)
  : texture{}
  , buffer(CameraBuffer())
  , gst_data(buffer)
  , worker([this]() { gst_data.run(worker.get_stop_token()); })
{
  Image placeholder = GenImageColor(width, height, WHITE);
  texture = LoadTextureFromImage(placeholder);
  UnloadImage(placeholder);
}

const Texture2D&
GstCamera::get_texture()
{
  auto guard = buffer.unlock();
  if (guard.data.updated) {
    const int width = guard.data.dimesions.first / 4;
    const int height = guard.data.dimesions.second;
    if (texture.width != width || texture.height != height) {
      UnloadTexture(texture);
      // The buffer owns these tightly packed RGBA pixels; only upload them.
      const Image frame{ guard.data.pixel_ptr.get(),
                         width,
                         height,
                         1,
                         PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
      texture = LoadTextureFromImage(frame);
    } else {
      UpdateTexture(texture, guard.data.pixel_ptr.get());
    }
    guard.data.updated = false;
  }
  return texture;
}

GstCamera::~GstCamera()
{
  gst_data.close_pipeline();
  UnloadTexture(texture);
}