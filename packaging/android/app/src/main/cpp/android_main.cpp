// Gridline — Android UI entry point (phase 2): SDL2 + OpenGL ES 3 + Dear ImGui, with engine audio.
//
// Boot flow: MainActivity (org.libsdl.app.SDLActivity subclass) loads libSDL2.so, then this
// library (libmain.so), and runs the app on a dedicated native thread started by SDLActivity
// (org.libsdl.app.SDLActivity) — SDL calls main() below. Touch works as a mouse via
// SDL_HINT_TOUCH_MOUSE_EVENTS, so every mouse-driven ImGui control responds to a finger tap
// or drag. The back button is not trapped: Android sends SDL_QUIT and the app exits cleanly.
//
// Phase 2: the AudioEngine (the same realtime engine the desktop app drives) renders the
// embedded demo song — "Gridline", all built-in instruments, no samples — through SDL's
// Android audio backend (AAudio on API 26+, OpenSL ES otherwise) into AAudio; PLAY/STOP run
// the transport in Song mode. RECORD_AUDIO is deliberately NOT declared yet: there is no
// capture path in this skeleton, and an unused permission only invites Play review questions.
// Output latency is reported as the negotiated buffer (SDL does not expose the device-side
// AAudio latency). Audio focus and background/foreground lifecycle are phase 5.
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

#include "GridlineVersion.h"
#include "core/Song.h"
#include "core/Demo.h"
#include "engine/AudioEngine.h"
#include "engine/Program.h"

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
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(scale);   // spacing/padding/thickness; fonts scale via FontGlobalScale
  style.WindowRounding = 14.0f;
  style.WindowBorderSize = 0.0f;

  ImGui_ImplSDL2_InitForOpenGL(window, gl);
  ImGui_ImplOpenGL3_Init("#version 300 es");

  // ---- phase 2: the engine and its Android audio device ----
  // Live engine state — the demo song ("Gridline", embedded in gridline_core; all built-in
  // instruments), so PLAY makes sound with nothing but the binary.
  gl::Song song = gl::makeDemoTrack();
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
      engine.setMode(gl::PlayMode::Song);  // play the playlist arrangement
      engine.setProgram(builder.build(song));
      const char* driver = SDL_GetCurrentAudioDriver();
      std::snprintf(audioStatus, sizeof audioStatus, "%s · %d Hz · %d-frame buffer (%.1f ms)",
                    driver ? driver : "audio", have.freq, have.samples,
                    1000.0 * static_cast<double>(have.samples) / static_cast<double>(have.freq));
      SDL_Log("audio device opened: %s", audioStatus);
      SDL_PauseAudioDevice(audioDevice, 0);  // unpause; the engine stays silent until PLAY
    }
  }
  const bool audioOk = audioDevice != 0;
  auto rebuild = [&]() {  // tempo edit committed: rebuild; the engine continues from the current position
    if (!audioOk) return;
    engine.setProgram(builder.build(song));
  };

  float tempoUi = static_cast<float>(song.tempo);
  float tempoCommitted = tempoUi;
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

    const ImVec4 accent{0.36f, 0.66f, 0.90f, 1.0f};
    const ImVec4 dim{0.60f, 0.60f, 0.62f, 1.0f};
    const ImVec4 warn{0.91f, 0.52f, 0.36f, 1.0f};
    const bool playing = engine.playing();

    // ---- central card ----
    const float cardW = std::min(display.x * 0.82f, 760.0f * scale);
    ImGui::SetNextWindowPos(ImVec2((display.x - cardW) * 0.5f, display.y * 0.30f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(cardW, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::Begin("##gridline", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);

    ImGui::PushStyleColor(ImGuiCol_Text, accent);
    ImGui::TextUnformatted("G R I D L I N E");
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, dim);
    ImGui::TextUnformatted("the workstation built around the grid");
    ImGui::TextUnformatted("android  ·  phase 2 — engine audio");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("gridline_core %s  ·  build %s", GRIDLINE_VERSION, GRIDLINE_BUILD_ID);
    ImGui::Text("demo song: Gridline · %.1f BPM · PPQ %d · %d/%d", song.tempo, song.ppq, song.timeSigNum,
                song.timeSigDen);
    ImGui::PushStyleColor(ImGuiCol_Text, audioOk ? dim : warn);
    ImGui::TextUnformatted(audioStatus);
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Transport row: PLAY/STOP runs the demo through SDL's AAudio/OpenSL ES path.
    ImGui::PushStyleColor(ImGuiCol_Button, playing ? accent : ImVec4(0.20f, 0.20f, 0.22f, 1.0f));
    if (ImGui::Button(playing ? "STOP" : "PLAY", ImVec2(-1.0f, 56.0f))) engine.togglePlay();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Tempo: edited live in the slider, committed to the engine on release (a tempo change is
    // a new tempo map; the audio thread keeps its position).
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##tempo", &tempoUi, 60.0f, 220.0f, "Tempo %.1f BPM");
    if (ImGui::IsItemDeactivatedAfterEdit() && tempoCommitted != tempoUi) {
      tempoCommitted = tempoUi;
      song.tempo = static_cast<double>(tempoUi);
      rebuild();
    }
    ImGui::Spacing();

    if (ImGui::Button("Reset song", ImVec2(-1.0f, 0.0f))) {
      engine.stopToStart();
      song = gl::makeDemoTrack();
      tempoUi = static_cast<float>(song.tempo);
      tempoCommitted = tempoUi;
      rebuild();
    }
    ImGui::Spacing();
    if (ImGui::Button("Quit", ImVec2(-1.0f, 44.0f))) running = false;
    ImGui::End();

    // ---- bottom-right stats ----
    ImGui::SetNextWindowPos(ImVec2(display.x - 10.0f, display.y - 10.0f), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    ImGui::Begin("##stats", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushStyleColor(ImGuiCol_Text, dim);
    ImGui::Text("%.0f FPS", ImGui::GetIO().Framerate);
    ImGui::PopStyleColor();
    ImGui::End();

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
