// Gridline — Android UI entry point: SDL2 + OpenGL ES 3 + Dear ImGui, with engine audio.
//
// Boot flow: MainActivity (org.libsdl.app.SDLActivity subclass) loads libSDL2.so, then this
// library (libmain.so), and runs the app on a dedicated native thread started by SDLActivity
// (org.libsdl.app.SDLActivity) — SDL calls main() below. Touch works as a mouse via
// SDL_HINT_TOUCH_MOUSE_EVENTS, so every mouse-driven ImGui control responds to a finger tap
// or drag. The back button is not trapped: Android sends SDL_QUIT and the app exits cleanly.
//
// Phase 2 engine audio + phase 4 channel rack. The AudioEngine (the same realtime engine the
// desktop app drives) renders the embedded demo song — "Gridline", all built-in instruments,
// no samples — through SDL's Android audio backend (AAudio on API 26+, OpenSL ES otherwise).
// android_rack.cpp draws the channel rack (phase-4 milestone 1): live mute/solo/gain/pan,
// a 16-step note grid per channel for the selected pattern, PAT/SONG transport, undo/redo.
// RECORD_AUDIO is deliberately NOT declared yet: there is no capture path in this skeleton,
// and an unused permission only invites Play review questions. Output latency is reported as
// the negotiated buffer (SDL does not expose the device-side AAudio latency). Audio focus
// and background/foreground lifecycle are phase 5.
#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <thread>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#if __has_include(<GLES3/gl3.h>)
#include <GLES3/gl3.h>
#endif

#include "core/Song.h"
#include "core/Demo.h"
#include "core/Document.h"
#include "engine/AudioEngine.h"
#include "engine/Program.h"
#include "android_rack.h"

namespace {

// SDL float32 interleaved callback → the engine's own render. The desktop callback's crash
// hooks are left out: the crash sentinel is disabled on Android (see packaging/android/README.md).
void audioCallback(void* user, Uint8* stream, int len) {
  auto* eng = static_cast<gl::AudioEngine*>(user);
  const int frames = len / static_cast<int>(sizeof(float) * 2);
  eng->processInterleaved(reinterpret_cast<float*>(stream), frames);
}

void drawGridBackground(const ImVec2& size) {
  ImDrawList* dl = ImGui::GetBackgroundDrawList();
  const float step = 44.0f;
  const ImU32 minor = IM_COL32(255, 255, 255, 9);
  const ImU32 major = IM_COL32(255, 255, 255, 22);
  int i = 0;
  for (float x = 0.0f; x <= size.x; x += step, ++i)
    dl->AddLine(ImVec2(x, 0.0f), ImVec2(x, size.y), (i % 4 == 0) ? major : minor, (i % 4 == 0) ? 1.5f : 1.0f);
  i = 0;
  for (float y = 0.0f; y <= size.y; y += step, ++i)
    dl->AddLine(ImVec2(0.0f, y), ImVec2(size.x, y), (i % 4 == 0) ? major : minor, (i % 4 == 0) ? 1.5f : 1.0f);
}

// Android density model: 160 dpi == 1.0. A modern tablet is 1.5..3.0.
float contentScaleFor(SDL_Window* window) {
  float ddpi = 0.0f;
  const int display = SDL_GetWindowDisplayIndex(window);
  if (display >= 0 && SDL_GetDisplayDPI(display, &ddpi, nullptr, nullptr) == 0 && ddpi > 0.0f)
    return SDL_clamp(ddpi / 160.0f, 1.0f, 3.5f);
  return 2.0f;  // sane default when the driver won't report DPI
}

}  // namespace

int main(int /* argc */, char** /* argv */) {
  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");  // touch -> mouse events for the ImGui backends

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
    SDL_Log("SDL_Init failed: %s", SDL_GetError());
    return 1;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  SDL_Window* window = SDL_CreateWindow("Gridline", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_SHOWN);
  if (!window) {
    SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  SDL_GLContext gl = SDL_GL_CreateContext(window);
  if (!gl) {
    SDL_Log("SDL_GL_CreateContext failed: %s", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  SDL_GL_SetSwapInterval(1);  // vsync

  const float scale = contentScaleFor(window);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.FontGlobalScale = scale;
  io.IniFilename = nullptr;  // no desktop-style config files on Android
  io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;  // finger-drag scrolling in list views
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(scale);   // spacing/padding/thickness; fonts scale via FontGlobalScale
  style.WindowRounding = 14.0f;
  style.WindowBorderSize = 0.0f;

  ImGui_ImplSDL2_InitForOpenGL(window, gl);
  ImGui_ImplOpenGL3_Init("#version 300 es");

  // ---- phase 2 + phase 4: the engine and its Android audio device, driving the channel rack ----
  // The live rack edits an in-memory gl::Document (the demo song "Gridline", all built-in
  // instruments, no samples); PLAY runs the selected pattern / the playlist arrangement through
  // SDL's Android audio backend into AAudio. RECORD_AUDIO is deliberately NOT declared yet:
  // there is no capture path in this skeleton, and an unused permission only invites Play
  // review questions. Output latency is reported as the negotiated buffer (SDL does not expose
  // the device-side AAudio latency). Audio focus and background/foreground lifecycle are phase 5.
  gl::Document doc(gl::makeDemoTrack());
  gl::AudioEngine engine(48000.0);
  // The desktop auto-selects half the hardware threads (Settings audio.threads = 0); the
  // engine's own default is one, which underruns a full demo arrangement on tablet cores.
  engine.setThreads(std::clamp(static_cast<int>(std::thread::hardware_concurrency()) / 2, 1, 8));
  gl::ProgramBuilder builder;
  char audioStatus[256] = "no audio device";
  SDL_AudioDeviceID audioDevice = 0;

  // Open the device first (paused) to learn the negotiated rate and buffer, then build the
  // program for that rate — the same order the desktop app uses.
  {
    SDL_AudioSpec want{}, have{};
    want.freq = 48000;
    want.format = AUDIO_F32SYS;
    want.channels = 2;
    want.samples = 2048;  // large buffer: tolerates slow tablet cores; costs ~43 ms round-trip. Try 1024/512 if tap response feels laggy.
    want.callback = audioCallback;
    want.userdata = &engine;
    audioDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &have,
                                      SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if (!audioDevice) {
      std::snprintf(audioStatus, sizeof audioStatus, "no audio output: %s", SDL_GetError());
      SDL_Log("SDL_OpenAudioDevice failed: %s", SDL_GetError());
    } else {
      engine.setSampleRate(static_cast<double>(have.freq));
      builder.setSampleRate(engine.sampleRate());
      builder.setAsyncSamples(true);  // renders land while the demo is built; no samples needed here
      engine.setMode(gl::PlayMode::Pattern);  // the rack's editing context; toggle PAT/SONG up top
      engine.setProgram(builder.build(doc.song()));
      const char* driver = SDL_GetCurrentAudioDriver();
      std::snprintf(audioStatus, sizeof audioStatus, "%s · %d Hz · %d-frame buffer (%.1f ms)",
                    driver ? driver : "audio", have.freq, have.samples,
                    1000.0 * static_cast<double>(have.samples) / static_cast<double>(have.freq));
      SDL_Log("audio device opened: %s", audioStatus);
      SDL_PauseAudioDevice(audioDevice, 0);  // unpause; the engine stays silent until PLAY
    }
  }
  const bool audioOk = audioDevice != 0;
  gl::android::RackState rackState;
  gl::android::rackInit(doc, engine, rackState);  // Pattern mode, first pattern selected

  bool running = true;

  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT) running = false;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    drawGridBackground(display);

    if (gl::android::rackDraw(rackState, doc, engine, builder, audioStatus, audioOk, scale, display)) running = false;

    ImGui::Render();
    int w = 0, h = 0;
    SDL_GL_GetDrawableSize(window, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.055f, 0.055f, 0.062f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);
  }

  if (audioDevice) {
    SDL_PauseAudioDevice(audioDevice, 1);
    SDL_CloseAudioDevice(audioDevice);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
