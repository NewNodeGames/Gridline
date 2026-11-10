# Gridline on Android (experimental)

Gridline does not ship an Android app yet. An experimental port is under way in
[`packaging/android/`](../packaging/android/README.md): the C++ engine (`gridline_core`)
compiles for Android (NDK, arm64-v8a and x86_64) as part of the normal CMake build
(`CMAKE_SYSTEM_NAME=Android`), and the phase-2 app boots SDL2 with an OpenGL ES 3 / Dear
ImGui shell whose PLAY button plays a small built-in starter song through SDL's Android
audio backend (AAudio / OpenSL ES); the phase-2 debug APK was confirmed booting on a tablet,
though audio quality was reported bad and is deferred — the app now boots into the light
starter after the demo proved too heavy for tablet CPUs (the same content is heavy on
Winlator; the full demo is opt-in in the transport bar). The channel rack (phase-4 milestone
1) playback of the starter song is verified smooth on a tablet (2026-11-10); the rack UI
itself is not yet device-verified; milestone 2 adds a piano roll and delete-channel;
milestone 3 (CI build, not device-tested) adds roll depth — mouse-wheel octave jump,
a 1/8/1/16/1/32 snap selector, drag-to-resize notes — and fixes the DEL confirm, which never
appeared because it was rendered in a different ImGui window than its opener. See the README in `packaging/android/` for
status, build instructions and the phase plan.

Desktop features that will not come to Android in their desktop form: external plugin
hosting with plugin editor windows (X11/Win32/Cocoa), detachable windows, ALSA/JACK MIDI.

On a Chromebook or an Android-based laptop (Googlebook), install the regular Linux `.deb` in
the Linux development environment instead — see [chromeos.md](chromeos.md). That is the
supported way to run real Gridline today on an Android-powered device.
