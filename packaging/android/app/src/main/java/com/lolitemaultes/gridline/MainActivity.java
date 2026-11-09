package com.lolitemaultes.gridline;

import org.libsdl.app.SDLActivity;

/**
 * Gridline on Android. All rendering and input live in native code (libmain.so): SDL2 +
 * OpenGL ES 3 + Dear ImGui. This class only boots SDL, which loads libSDL2.so and then
 * libmain.so (see getLibraries() in SDLActivity) and runs the native event loop on a
 * dedicated thread.
 */
public class MainActivity extends SDLActivity {
    // No overrides yet: the phase-1 skeleton needs nothing (fullscreen defaults, portrait
    // and landscape both fine). Later phases override getLibraries() only if Gridline
    // ships extra native libraries, and add SDL hints via AndroidManifest meta-data.
}
