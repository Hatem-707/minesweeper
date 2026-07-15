#include <glib.h>
#include "gst/rtsp-server/rtsp-server-object.h"
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define DEFAULT_RTSP_PORT "8554"
#define DEFAULT_URI_PATH "/test"
#define DEFAULT_DEVICE "/dev/video0"

static char *port = (char *)DEFAULT_RTSP_PORT;
static char *path = (char *)DEFAULT_URI_PATH;
static char *device = (char *)DEFAULT_DEVICE;

static GOptionEntry entries[] = {
    {"port", 'p', 0, G_OPTION_ARG_STRING, &port,
     "Port to listen on (default: " DEFAULT_RTSP_PORT ")", "PORT"},
    {"uri", 'u', 0, G_OPTION_ARG_STRING, &path,
     "URI path of the stream (default: " DEFAULT_URI_PATH " )", "URI"},
    {"device", 'd', 0, G_OPTION_ARG_STRING, &device,
     "Device name to stream (default: " DEFAULT_DEVICE " )", "DEVICE"}};

int main(int argc, char *argv[]) {
  GstRTSPServer *server;
  GstRTSPMediaFactory *factory;
  GstRTSPMountPoints *mounts;
  GMainLoop *loop;
  GstRTSPMedia *media;
  GOptionContext *optctx;
  GError *error = NULL;

  optctx = g_option_context_new("");
  g_option_context_add_main_entries(optctx, entries, NULL);
  g_option_context_add_group(optctx, gst_init_get_option_group());
  if (!g_option_context_parse(optctx, &argc, &argv, &error)) {
    g_printerr("Error parsing options: %s\n", error->message);
    g_option_context_free(optctx);
    g_clear_error(&error);
    return -1;
  }
  g_option_context_free(optctx);

  gst_init(&argc, &argv);

  server = gst_rtsp_server_new();
  g_object_set(server, "service", port, NULL);

  factory = gst_rtsp_media_factory_new();
  gst_rtsp_media_factory_set_shared(factory, TRUE);
  gst_rtsp_media_factory_set_latency(factory, 0);

  /* make a mainloop for the default context */
  loop = g_main_loop_new(NULL, FALSE);

  /* attach the server to the default maincontext */
  gst_rtsp_server_attach(server, NULL);

  char *launch = (char *)malloc(1024);
  sprintf(launch,
          "v4l2src device=%s ! jpegdec ! videoflip method=horizontal-flip ! "
          " x264enc tune=zerolatency speed-preset=ultrafast "
          "key-int-max=30 ! rtph264pay pt=96 name=pay0 config-interval=-1",
          device);

  gst_rtsp_media_factory_set_launch(factory, launch);

  mounts = gst_rtsp_server_get_mount_points(server);
  gst_rtsp_mount_points_add_factory(mounts, path, factory);
  g_object_unref(mounts);

  printf("serving on rtsp://127.0.0.1:%s%s \n", port, path);

  /* start serving */
  g_main_loop_run(loop);
}
