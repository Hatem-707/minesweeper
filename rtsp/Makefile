# Define the compiler
CC = gcc

# Use the 'shell' function to let pkg-config find the flags
CFLAGS = $(shell pkg-config --cflags gstreamer-1.0 gstreamer-rtsp-server-1.0 )
LIBS   = $(shell pkg-config --libs gstreamer-1.0 gstreamer-rtsp-server-1.0)

# Target to build the executable
rtsp: rtsp.c
	$(CC) $< -o $@ $(CFLAGS) $(LIBS)
