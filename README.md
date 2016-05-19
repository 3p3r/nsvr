# NSVR

Network Synchronized Video Renderer. A [GStreamer](https://gstreamer.freedesktop.org/) based library to playback a video, frame-synchronized across network and across computers.

## Build requirements

This library should be linked against GStreamer v1.4+ and requires a standard C++11 compiler with a decent support of following standard libraries:

 - `<mutex>`
 - `<atomic>`
 - `<string>`
 - `<vector>`
 - `<memory>`
 - `<sstream>`

## Build instructions

Main build procedure is handled by [cmake](https://cmake.org/). You need to have GStreamer developer SDK installed on your system before-hand.

### Windows

Before proceeding with cmake, download GStreamer development SDK [for Windows](https://gstreamer.freedesktop.org/data/pkg/windows/) and install its *complete* version on your system. Verify `GSTREAMER_1_0_ROOT_<arch>` is defined in your environment variables, pointing to the root of where you installed the SDK.

### Android

Cmake cannot be used while building this library for Android until GStreamer starts officially supporting cmake as a build option. Special patching procedure (*todo*) is required in order to build this library alongside the rest of the GStreamer SDK.

### Mac OSX

Both cmake and GStreamer can be obtained via [Homebrew](http://brew.sh/). Beyond that this library is not tested nor designed to run on Apple platforms yet.
