# Gridline on Android (experimental)

Gridline does not ship an Android app yet. An experimental port is under way in
[`packaging/android/`](../packaging/android/README.md): the C++ engine (`gridline_core`)
compiles for Android (NDK, arm64-v8a and x86_64) as part of the normal CMake build
(`CMAKE_SYSTEM_NAME=Android`), and the phase-1 app boots SDL2 with an OpenGL ES 3 / Dear ImGui
shell that renders live engine state; the debug APK is confirmed working on a tablet. See the
README in `packaging/android/` for status, build instructions and the phase plan.

Desktop features that will not come to Android in their desktop form: external plugin
hosting with plugin editor windows (X11/Win32/Cocoa), detachable windows, ALSA/JACK MIDI.

On a Chromebook or an Android-based laptop (Googlebook), install the regular Linux `.deb` in
the Linux development environment instead — see [chromeos.md](chromeos.md). That is the
supported way to run real Gridline today on an Android-powered device.
