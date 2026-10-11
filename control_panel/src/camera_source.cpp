#include "camera_source.hpp"
#include "spin_lock.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fmt/base.h>
#include <glib-object.h>
#include <gst/app/gstappsink.h>
#include <gst/gstclock.h>
#include <gst/gstelement.h>
#include <gst/gstpad.h>
#include <gst/video/video-info.h>
#include <memory>
#include <stop_token>
#include <thread>
#include <utility>

void
on_pad_added(GstElement*, GstPad* new_pad, GstElement* convert)
{
  GstPad* sink_pad = NULL;
  GstPadLinkReturn ret;
  GstCaps* new_pad_caps = NULL;
  GstStructure* new_pad_struct = NULL;
  const gchar* new_pad_type = NULL;

  fmt::println("A new pad added");

  sink_pad = gst_element_get_static_pad(convert, "sink");
  new_pad_caps = gst_pad_get_current_caps(new_pad);
  new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
  new_pad_type = gst_structure_get_name(new_pad_struct);

  if (!g_str_has_prefix(new_pad_type, "video/x-raw")) {
    fmt::println("skipping pad: {} ", new_pad_type);
    goto exit;
  }

  if (GST_PAD_IS_LINKED(sink_pad)) {
    g_print("already linked");
    goto exit;
  }

  ret = gst_pad_link(new_pad, sink_pad);

  if (GST_PAD_LINK_FAILED(ret)) {
    fmt::println("Type is {} but link failed.", new_pad_type);
  } else {
    fmt::println("Link succeeded (type {}).", new_pad_type);
  }

exit:
  /* Unreference the new pad's caps, if we got them */
  if (new_pad_caps != NULL)
    gst_caps_unref(new_pad_caps);

  /* Unreference the sink pad */
  if (sink_pad != NULL)
    gst_object_unref(sink_pad);
}

bool
on_new_sample(GstElement* appsink, SpinLock<CameraBuffer>& camera_buffer)
{
  GstSample* sample;
  sample =
    gst_app_sink_try_pull_sample(GST_APP_SINK(appsink), 100 * GST_MSECOND);
  if (sample != NULL) {
    const std::int64_t received_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
        .count();
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
      bool should_allocate = true;

      {
        auto guard = camera_buffer.unlock();
        if (guard.data.dimesions.first == width * px_stride &&
            guard.data.dimesions.second == height) {
          should_allocate = false;
        }
      }

      if (should_allocate) [[unlikely]] {
        auto data_ptr = std::make_unique<uint8_t[]>(size);
        for (int i = 0; i < height; i++) {
          memcpy(data_ptr.get() + i * width * px_stride,
                 pixels + i * stride,
                 width * px_stride);
        }
        auto buffer_guard = camera_buffer.unlock();
        buffer_guard.data.updated = true;
        buffer_guard.data.frame_ns = received_ns;
        buffer_guard.data.dimesions = { width * px_stride, height };
        std::swap(buffer_guard.data.pixel_ptr, data_ptr);
      } else [[likely]] {
        auto buffer_guard = camera_buffer.unlock();
        auto& data_ptr = buffer_guard.data.pixel_ptr;
        for (int i = 0; i < height; i++) {
          memcpy(data_ptr.get() + i * width * px_stride,
                 pixels + i * stride,
                 width * px_stride);
        }
        buffer_guard.data.updated = true;
        buffer_guard.data.frame_ns = received_ns;
      }

      gst_buffer_unmap(buf, &map);
    }
    gst_sample_unref(sample);
  } else {
    // fmt::println("Null sample");
    return false;
  }
  return true;
}

GstData::GstData(SpinLock<CameraBuffer>& camera_buffer, const std::string& uri)
  : camera_buffer(camera_buffer)
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

  g_object_set(source, "uri", uri.c_str(), NULL);
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
GstData::run(std::stop_token stop)
{
  ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);

  if (ret == GST_STATE_CHANGE_FAILURE) {
    gst_object_unref(pipeline);
    fmt::println("error connecting");
    throw std::runtime_error(
      "Unable to set the pipeline to the playing state.");
  }

  bus = gst_element_get_bus(pipeline);

  while (!stop.stop_requested()) {
    if (!on_new_sample(sink, camera_buffer)) {
      msg = gst_bus_timed_pop_filtered(
        bus,
        100 * GST_MSECOND,
        (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
      if (msg != NULL) {
        GError* err;
        gchar* debug_info;

        switch (GST_MESSAGE_TYPE(msg)) {
          case GST_MESSAGE_ERROR:
            gst_message_parse_error(msg, &err, &debug_info);
            fmt::println("Error received from element {}: {}",
                         GST_OBJECT_NAME(msg->src),
                         err->message);
            fmt::println("Debugging information: {}",
                         debug_info ? debug_info : "none");
            g_clear_error(&err);
            g_free(debug_info);
            // most probable cause is a stream yet to launch
            std::this_thread::sleep_for(std::chrono::seconds(1));
            ret = gst_element_set_state(pipeline, GST_STATE_NULL);
            if (ret == GST_STATE_CHANGE_FAILURE) {
              gst_object_unref(pipeline);
              fmt::println("error connecting");
              throw std::runtime_error(
                "Unable to set the pipeline to the playing state.");
            }
            ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
            if (ret == GST_STATE_CHANGE_FAILURE) {
              gst_object_unref(pipeline);
              fmt::println("error connecting");
              throw std::runtime_error(
                "Unable to set the pipeline to the playing state.");
            }

            break;

          case GST_MESSAGE_EOS:
            fmt::println("End-Of-Stream reached.");
            break;
          default:
            /* We should not reach here */
            fmt::println("Unexpected message received.");
        }
        gst_message_unref(msg);
      }
    }
  }
}

void
GstData::close_pipeline()
{
  ret = gst_element_set_state(pipeline, GST_STATE_NULL);

  if (ret == GST_STATE_CHANGE_FAILURE) {
    gst_object_unref(pipeline);
    throw std::runtime_error(
      "Unable to set the pipeline to the playing state.");
  }
}

