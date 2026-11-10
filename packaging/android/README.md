# Gridline for Android

Status: **phase 2 — table stakes audio** (SDL2's Android audio backend drives the real
`AudioEngine`: PLAY/STOP plays the embedded demo song). The app boots SDL2, creates an ES 3
context, renders ImGui frames and opens an AAudio-or-OpenSL-ES output device. Touch works as
a mouse. Phase 1 is verified on a real tablet by the project owner (debug APK built by CI).
Phase 0 (below) is done.

What phase 2 added on top of phase 1:

- The realtime `AudioEngine` (the same one the desktop app links) renders `gl::makeDemoTrack()` —
  the embedded demo song "Gridline" (174 BPM liquid drum & bass; all built-in instruments, no
  samples) — in Song mode through `SDL_OpenAudioDevice(...)` with `SDL_AUDIODRIVERS` = AAudio
  (API 26+) or OpenSL ES. PLAY/STOP run the transport; the status line shows the negotiated
  driver, rate and buffer (the SDL buffer is the only latency component SDL reports — the
  device-side AAudio latency is not exposed).
- The engine runs with the desktop's automatic thread count (half the hardware threads) and
  the output buffer is requested at 2048 frames — raised from 256 after the first tablet
  report ("sound works but is very sluggish": one-core rendering starved the device; the large
  buffer tolerates a slow core at the cost of ~43 ms round-trip). If tap response feels laggy,
  drop `want.samples` in `android_main.cpp` to 1024 or 512.
- Tempo is edited in the slider and **committed on release** (`IsItemDeactivatedAfterEdit`):
  a tempo change is a new tempo map in a rebuilt program; the audio thread keeps its position.
- No `RECORD_AUDIO` in the manifest: the output path needs no permission, and declaring one
  nothing uses only invites Play review questions. It arrives with the capture path.
- Reset reloads the demo song and rebuilds the program. Audio focus and
  background/foreground lifecycle remain phase 5.

What phase 1 added: the phase-0 plain `Activity` + JNI probe was replaced by
`MainActivity : SDLActivity`. `libmain.so` (this is Gridline's process) links SDL2,
`gridline_core` and Dear ImGui's OpenGL ES 3 renderer. SDL 2.30.9's Android Java bindings are
vendored under `app/src/main/java/org/libsdl/app/` (unmodified, see its README).

## What this is

A standard Gradle/Android project whose APK contains two native modules:

- `libSDL2.so` — SDL 2.30.9, built from upstream (same source and SHA-256 as the desktop
  `GRIDLINE_BUNDLED_SDL2` option). SDLActivity loads it first.
- `libmain.so` — `android_main.cpp` (SDL2 + OpenGL ES 3 + Dear ImGui), linking
  `gridline_core` (DSP, project model, plugin registry, collaboration protocol — the same
  static library the desktop app links). SDLActivity loads it second.

Desktop Gridline (SDL2 + desktop OpenGL + X11/Wayland plugin windows) does **not** build for
Android. Cross-compilation is detected in the top-level `CMakeLists.txt`
(`CMAKE_SYSTEM_NAME=Android` → `GRIDLINE_ANDROID`): it compiles the engine only and skips the
desktop application, CLI, tests, plugin bundles and CPack packaging.

Deliberately out of scope for now (desktop-only, by design):

- VST3 / VST2 / CLAP / LV2 / AU **plugin hosting and plugin editor windows** (X11/Win32/Cocoa)
- Detached/multi windows, X11/Wayland driver selection
- ALSA/JACK/sndio MIDI backends (Android will need AMidi)
- ASIO, sndio, desktop crash symbolisation

## Layout

```
packaging/android/
├── settings.gradle / build.gradle / gradle.properties
├── app/
│   ├── build.gradle                 app module (compileSdk 35, minSdk 28, targetSdk 35, ABIs arm64-v8a + x86_64)
│   └── src/main/
│       ├── AndroidManifest.xml      .MainActivity, glEsVersion 0x00030000, no permissions yet
│       ├── java/com/lolitemaultes/gridline/MainActivity.java   extends SDLActivity (no overrides)
│       ├── java/org/libsdl/app/                                vendored SDL 2.30.9 Android bindings (see README.gridline.md)
│       ├── cpp/CMakeLists.txt       FetchContent SDL2 + add_subdirectory of the repo root + imgui ES 3
│       ├── cpp/android_main.cpp     the phase-2 screen (grid + card + transport; engine audio)
│       └── res/                     launcher icons from assets/icons/gridline-app-*.png
```

## Requirements

- Android SDK with **platform 35**, **build-tools**, and **NDK 27.2.12479018** (r27c)
- JDK 17, Gradle 8.9 (or open `packaging/android/` in Android Studio and let it sync)
- `sdkmanager "ndk;27.2.12479018" "cmake;3.22.1" "platforms;android-35" "build-tools;35.0.0"`

## Verification

**Phase 0 (done)** — `gridline_core` compiles for Android on both ABIs; the APK assembles
(`:app:assembleRelease`): JNI smoke test ran, ELF LOAD segments at **16 KB**, APK passes
`zipalign -P 16` — Play Console's Android 15+ requirement.

**Phase 1** — `libmain.so` **and** `libSDL2.so` compile and link for `arm64-v8a` (NDK r27c,
Release) and `x86_64`. ELF LOAD segments are **16 KB** for both (applied to the repo's shared
libs, the wrapper's `main` module and the SDL2 module). The vendored SDL2 Java sources +
`MainActivity` compile against `android-35`, and the manifest/resources pass `aapt2 link`
(package, targetSdk 35, launchable activity, `glEsVersion 0x00030000`).

**Phase 2** — the engine is now built into the app: `gl::AudioEngine` + `ProgramBuilder`
(both inside `gridline_core`) render the embedded demo song through SDL's Android audio
backend; `android_main.cpp` opens the output device (F32 stereo, 48 kHz requested, buffer
negotiated), and PLAY/STOP drive the transport in Song mode. See the control list and the
CI-built debug APK to hear it — it is **not yet device-verified** at the time of writing.

- **On a device** — the debug APK built by CI (`:app:assembleDebug`, artifact
  `gridline-android-debug`) was sideloaded and confirmed working on a tablet (2026-11-09): the
  screen renders and the controls respond; back button quits. The debug APK is large (~83 MB)
  because the native libraries ship unstripped debug info; the release/Play packaging path
  strips it at packaging time.
- Local note: phase 1 was built on a 2 GB sandbox where the kernel OOM-kills the `-g`
  (RelWithDebInfo) native build; both ABIs are built in CI (16 GB runners). A normal desktop
  (8 GB+ or Android Studio) builds both without issue.
- Known engine limitations on Android (documented for the phases ahead): the crash-sentinel
  process and the plugin **sandbox helper** are disabled (an APK has no separate executables —
  `posix_spawn` is only used elsewhere at API 28+), POSIX `shm_open` is unavailable (API 30+),
  and MIDI stays unbound until the AMidi phase. Collaboration networking (libjuice) is compiled
  in but untested on Android; that is a phase-5 milestone.

## Build & install

```sh
cd packaging/android
gradle :app:assembleDebug            # APK: app/build/outputs/apk/debug/app-debug.apk
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Or create the wrapper once so `./gradlew` works: `gradle wrapper --gradle-version 8.9`.

Building both ABIs with debug symbols needs ~4 GB RAM free; the CI job
(`.github/workflows/android.yml`, 16 GB) builds `:app:assembleDebug` without issue.

16 KB page-size alignment (required by Google Play for Android 15+ on ARM64): the top-level
`CMakeLists.txt` adds `-Wl,-z,max-page-size=16384` for Android builds, and so do the
`main` and `SDL2` modules in `app/src/main/cpp/CMakeLists.txt`. `useLegacyPackaging` is not
set, so `.so` files ship uncompressed and 16 KB aligned.

## Phase plan

| Phase | Content | Notes |
|-------|---------|-------|
| 0 (done) | Gradle skeleton, NDK cross-compile of `gridline_core`, JNI smoke test, CI stub | APK assembles |
| 1 | `SDLActivity` + OpenGL ES 3 + Dear ImGui (`imgui_impl_opengl3`, GLSL ES 300), single live screen driven by a `gl::Song` | done — verified on a tablet (debug APK from CI) |
| 2 | Audio: SDL2's Android backend (AAudio/OpenSL ES) driving `AudioEngine::processInterleaved` from the SDL callback; embedded demo song plays in Song mode; buffer-based latency readout | output path works; **`RECORD_AUDIO` deferred** until a capture path exists; device re-check pending |
| 3 | Load/save projects via the Storage Access Framework; bundle `assets/` (sounds) in the APK or download on first run | file interop |
| 4 | Tablet-first UI pass: channel rack, piano roll, playlist, mixer with finger-sized targets | the big one |
| 5 | MIDI via AMidi; background/foreground lifecycle, audio focus, process death | |
| later | Collaboration in the field (libjuice over Android networks), Play Store packaging | keep `GRIDLINE_COLLAB=ON` |

## Controls (phase 2)

Tap/drag = mouse (via `SDL_TOUCH_MOUSE_EVENTS`). The **Back** button is not trapped — it quits
the app, like a plain back press. **PLAY/STOP** starts/stops the demo song ("Gridline") through
the tablet's speakers or headphones. The tempo slider edits the tempo **on release** (a tempo
change is a new tempo map). **Reset song** stops the transport and reloads the demo. **Quit**
exits. The status line under the version names the driver (`aaudio` or `openslES`), the
negotiated rate and buffer (2048 requested) — that buffer is the only latency figure SDL reports; AAudio's
device-side latency is not exposed to apps.

## CI

`.github/workflows/android.yml` assembles the debug APK on pushes that touch the engine or
this directory, and uploads it as an artifact.
