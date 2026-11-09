// Gridline — Android UI entry point (phase 1): SDL2 + OpenGL ES 3 + Dear ImGui.
//
// Boot flow: MainActivity (org.libsdl.app.SDLActivity subclass) loads libSDL2.so, then this
// library (libmain.so), and runs the app on a dedicated native thread started by SDLActivity
// (org.libsdl.app.SDLActivity) — SDL calls main() below. Touch works as a mouse via
// SDL_HINT_TOUCH_MOUSE_EVENTS, so every mouse-driven ImGui control responds to a finger tap
// or drag. The back button is not trapped: Android sends SDL_QUIT and the app exits cleanly.
//
// Phase-1 skeleton: one screen, live engine state (a gl::Song), no audio yet (AAudio is
// phase 2). No processes, no file access, no network — this only proves the render + input
// loop on a device.
#include <SDL.h>

#include <algorithm>
#include <cstdio>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#if __has_include(<GLES3/gl3.h>)
#include <GLES3/gl3.h>
#endif

#include "GridlineVersion.h"
#include "core/Song.h"

namespace {

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

  // Live engine state — the point of the skeleton: the UI edits a real gl::Song.
  gl::Song song = gl::makeEmptySong();
  float tempoUi = static_cast<float>(song.tempo);
  bool playing = false;
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
    ImGui::TextUnformatted("android  ·  phase 1 skeleton");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("gridline_core %s  ·  build %s", GRIDLINE_VERSION, GRIDLINE_BUILD_ID);
    ImGui::Text("default project: %.1f BPM  ·  PPQ %d  ·  %d/%d", song.tempo, song.ppq, song.timeSigNum,
                song.timeSigDen);
    ImGui::Spacing();

    // Transport row: Play/Stop toggle + a live tempo edit on the engine model.
    ImGui::PushStyleColor(ImGuiCol_Button, playing ? accent : ImVec4(0.20f, 0.20f, 0.22f, 1.0f));
    if (ImGui::Button(playing ? "STOP" : "PLAY", ImVec2(-1.0f, 56.0f))) playing = !playing;
    ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderFloat("##tempo", &tempoUi, 60.0f, 220.0f, "Tempo %.1f BPM")) song.tempo = static_cast<double>(tempoUi);
    ImGui::Spacing();

    if (ImGui::Button("Reset project", ImVec2(-1.0f, 0.0f))) {
      song = gl::makeEmptySong();
      tempoUi = static_cast<float>(song.tempo);
      playing = false;
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

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
