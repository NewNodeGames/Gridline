# Gridline for Android — native build. Invoked by Gradle (externalNativeBuild).
#
# Targets:
#   SDL2 (SHARED)  — libSDL2.so, built from upstream release-2.30.9 (same source + SHA256 as
#                    the desktop GRIDLINE_BUNDLED_SDL2 option). SDLActivity loads libSDL2.so,
#                    then libmain.so.
#   main (SHARED)  — libmain.so: Gridline's Android entry point (android_main.cpp) + the
#                    Dear ImGui OpenGL ES 3 renderer + gridline_core (the whole engine).
cmake_minimum_required(VERSION 3.22.1)
project(gridline_android LANGUAGES C CXX)

get_filename_component(GRIDLINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.." ABSOLUTE)

# ---- SDL2 2.30.9 (upstream, unmodified) ----
include(FetchContent)
set(SDL_SHARED ON  CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF   CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL   ON CACHE BOOL "" FORCE)
set(SDL_DISABLE_UNINSTALL  ON CACHE BOOL "" FORCE)
set(SDL2_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(SDL2
  URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz
  URL_HASH SHA256=24b574f71c87a763f50704bbb630cbe38298d544a1f890f099a4696b1d6beba4)
FetchContent_MakeAvailable(SDL2)

# SDL2 is a shared module of this APK: same 16 KB page-alignment requirement as our own code.
if(TARGET SDL2)
  target_link_options(SDL2 PRIVATE "-Wl,-z,max-page-size=16384")
endif()

# ---- gridline_core (the whole engine; see the top-level CMakeLists.txt) ----
add_subdirectory(${GRIDLINE_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/gridline)

# ---- Dear ImGui (vendored in third_party/imgui, OpenGL ES 3 renderer) ----
set(IMGUI_DIR ${GRIDLINE_ROOT}/third_party/imgui)
add_library(gridline_imgui_es STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_impl_sdl2.cpp
  ${IMGUI_DIR}/imgui_impl_opengl3.cpp
  ${GRIDLINE_ROOT}/src/app/imgui_hooks/ImGuiItemHook.cpp)
# Same build flags as the desktop imgui_core target.
target_compile_definitions(gridline_imgui_es PUBLIC IMGUI_ENABLE_TEST_ENGINE "ImDrawIdx=unsigned int")
target_compile_definitions(gridline_imgui_es PRIVATE IMGUI_IMPL_OPENGL_ES3)
target_include_directories(gridline_imgui_es PUBLIC ${IMGUI_DIR})
# imgui_impl_opengl3.cpp with IMGUI_IMPL_OPENGL_ES3 includes <GLES3/gl3.h> (NDK sysroot);
# the gl* symbols resolve by linking GLESv3, which we do on the final module below.
target_link_libraries(gridline_imgui_es PUBLIC SDL2::SDL2)
set_target_properties(gridline_imgui_es PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---- libmain.so — the application ----
add_library(main SHARED android_main.cpp android_rack.cpp)
target_compile_features(main PRIVATE cxx_std_17)
target_link_libraries(main PRIVATE gridline_imgui_es gridline_core
                                  SDL2::SDL2 EGL GLESv3 android log)
# Google Play requires 16 KB page-aligned native libraries on Android 15+ (ARM64).
target_link_options(main PRIVATE "-Wl,-z,max-page-size=16384")
