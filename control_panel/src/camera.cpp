#include "camera.hpp"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <fmt/base.h>
#include <glib-object.h>
#include <gst/gstelement.h>
#include <gst/video/video-info.h>
#include <memory>
#include <raylib.h>
#include <utility>

constexpr std::string_view uri = "rtsp://127.0.0.1:8554/test\0";

void
on_pad_added(GstElement*, GstPad* new_pad, GstElement* convert)
{
  GstPad* sink_pad = NULL;
  GstPadLinkReturn ret;
  GstCaps* new_pad_caps = NULL;
  GstStructure* new_pad_struct = NULL;
  const gchar* new_pad_type = NULL;

  g_print("A new pad added");

  sink_pad = gst_element_get_static_pad(convert, "sink");
  new_pad_caps = gst_pad_get_current_caps(new_pad);
  new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
  new_pad_type = gst_structure_get_name(new_pad_struct);

  if (!g_str_has_prefix(new_pad_type, "video/x-raw")) {
    g_print("skipping pad: %s \n", new_pad_type);
    goto exit;
  }

  if (GST_PAD_IS_LINKED(sink_pad)) {
    g_print("already linked");
    goto exit;
  }

  ret = gst_pad_link(new_pad, sink_pad);

  if (GST_PAD_LINK_FAILED(ret)) {
    g_print("Type is '%s' but link failed.\n", new_pad_type);
  } else {
    g_print("Link succeeded (type '%s').\n", new_pad_type);
  }

exit:
  /* Unreference the new pad's caps, if we got them */
  if (new_pad_caps != NULL)
    gst_caps_unref(new_pad_caps);

  /* Unreference the sink pad */
  if (sink_pad != NULL)
    gst_object_unref(sink_pad);
}

void
on_new_sample(GstElement* appsink, CameraBuffer* d_buf)
{
  GstSample* sample;
  sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink));
  GstBuffer* buf = gst_sample_get_buffer(sample);
  GstCaps* caps = gst_sample_get_caps(sample);
  GstVideoInfo info;
  gst_video_info_from_caps(&info, caps);
  GstMapInfo map;
  if (gst_buffer_map(buf, &map, GST_MAP_READ)) {

    guint8* pixels = map.data;
    int size = map.size;
    int width = GST_VIDEO_INFO_WIDTH(&info);
    int height = GST_VIDEO_INFO_HEIGHT(&info);
    int stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
    int px_stride = GST_VIDEO_INFO_COMP_PSTRIDE(&info, 0);

    auto data = std::make_unique<uint8_t[]>(size);
    for (int i = 0; i < height; i++) {
      memcpy(data.get() + i * width * px_stride,
             pixels + i * stride,
             width * px_stride);
    }

    while (d_buf->reading.exchange(true, std::memory_order::acquire)) {
      // spin — someone else holds it
    }
    d_buf->updated = true;
    d_buf->dimesions = { width * px_stride, height };
    std::swap(d_buf->data, data);
    d_buf->reading.store(false, std::memory_order::release);

    gst_buffer_unmap(buf, &map);
  }
  gst_sample_unref(sample);
}

GstData::GstData(CameraBuffer* d_buf)
  : d_buf(d_buf)
{
  gst_init(NULL, NULL);

  source = gst_element_factory_make("uridecodebin", "uri-bin");
  convert = gst_element_factory_make("videoconvert", "video-convert");
  sink = gst_element_factory_make("appsink", "sink");

  pipeline = gst_pipeline_new("test-buffer-read");

  if (!source || !convert || !sink || !pipeline) {
    throw std::runtime_error("Gst initialization error");
  }

  gst_bin_add_many(GST_BIN(pipeline), source, convert, sink, NULL);
  if (!gst_element_link(convert, sink)) {
    gst_object_unref(pipeline);
    throw std::runtime_error("Gst linking error");
  }

  g_object_set(source, "uri", uri.data(), NULL);
  g_object_set(source, "buffer-duration", 0, NULL);
  g_signal_connect(source, "pad-added", G_CALLBACK(on_pad_added), convert);

  g_object_set(
    sink,
    "caps",
    gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "RGBA", NULL),
    NULL);
  g_object_set(sink, "sync", FALSE, "drop", TRUE, "qos", TRUE, NULL);
  // g_object_set(sink, "emit-signals", TRUE, NULL);
  // g_signal_connect(sink, "new-sample", G_CALLBACK(on_new_sample), d_buf);
};

GstData::~GstData()
{
  gst_object_unref(bus);
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
}

void
GstData::run()
{
  ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);

  if (ret == GST_STATE_CHANGE_FAILURE) {
    gst_object_unref(pipeline);
    throw std::runtime_error(
      "Unable to set the pipeline to the playing state.");
  }

  bus = gst_element_get_bus(pipeline);
  // msg = gst_bus_timed_pop_filtered(
  //   bus,
  //   GST_CLOCK_TIME_NONE,
  //   (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
  // if (msg != NULL) {
  //   GError* err;
  //   gchar* debug_info;

  //   switch (GST_MESSAGE_TYPE(msg)) {
  //     case GST_MESSAGE_ERROR:
  //       gst_message_parse_error(msg, &err, &debug_info);
  //       g_printerr("Error received from element %s: %s\n",
  //                  GST_OBJECT_NAME(msg->src),
  //                  err->message);
  //       g_printerr("Debugging information: %s\n",
  //                  debug_info ? debug_info : "none");
  //       g_clear_error(&err);
  //       g_free(debug_info);
  //       break;

  //     case GST_MESSAGE_EOS:
  //       g_print("End-Of-Stream reached.\n");
  //       break;
  //     default:
  //       /* We should not reach here */
  //       g_printerr("Unexpected message received.\n");
  //   }
  //   gst_message_unref(msg);
  // }

  while (! should_close) {
    on_new_sample(sink, d_buf);
  }
}

void GstData::close_pipeline(){
  ret = gst_element_set_state(pipeline, GST_STATE_NULL);

  if (ret == GST_STATE_CHANGE_FAILURE) {
    gst_object_unref(pipeline);
    throw std::runtime_error(
      "Unable to set the pipeline to the playing state.");
  }
}

GstCamera::GstCamera(int width, int height)
  : texture(LoadTextureFromImage(GenImageColor(width, height, WHITE)))
  , buffer()
  , gst_data(&buffer)
  , worker([this]() { gst_data.run(); })
{
}

std::unique_ptr<uint8_t[]>
center_feed(uint8_t original[], std::pair<int, int> dim, int edge)
{
  auto& [px_width, height] = dim;
  auto new_data = std::make_unique<uint8_t[]>(edge * edge * 4);
  if (px_width > height * 4) {
    int columns_diff = (edge - height) * edge * 4;
    memset(new_data.get(), 0, columns_diff / 2);
    memcpy(new_data.get() + columns_diff / 2, original, px_width * height);
    memset(new_data.get() + columns_diff / 2 + px_width * height,
           0,
           columns_diff / 2);
  } else {
    int row_diff = (edge - (px_width / 4)) * 4;
    for (int i = 0; i < edge; i++) {

      memset(new_data.get() + i * edge, 0, row_diff / 2);
      memcpy(new_data.get() + i * edge + row_diff / 2,
             original + i * px_width,
             px_width);
      memset(new_data.get() + i * edge + row_diff / 2, 0, row_diff / 2);
    }
  }
  return new_data;
}

const Texture2D&
GstCamera::get_texture()
{
  while (buffer.reading.exchange(true, std::memory_order::acquire)) {
    // spin — someone else holds it
  }
  if (buffer.updated) {
    if (buffer.dimesions.first == texture.width * 4 &&
        buffer.dimesions.second == texture.height) {
      UpdateTexture(texture, buffer.data.get());
    } else {
      int edge = std::max(buffer.dimesions.first / 4, buffer.dimesions.second);
      if (texture.height != edge || texture.width != edge) {
        UnloadTexture(texture);
        texture = LoadTextureFromImage(GenImageColor(edge, edge, WHITE));
      }
      auto ptr = center_feed(buffer.data.get(), buffer.dimesions, edge);
      UpdateTexture(texture, ptr.get());
    }
    buffer.updated = false;
  }
  return texture;
}

void
GstCamera::release_texture()
{
  buffer.reading.store(false, std::memory_order::release);
}

GstCamera::~GstCamera(){
  gst_data.should_close = true;
  gst_data.close_pipeline();
}