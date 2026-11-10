// Gridline for Android — the light starter song.
//
// The embedded demo ("Gridline") is far too heavy for tablet CPUs at realtime (14 pitched
// synth voices + 20 automation lanes; see packaging/android/README.md). Until then the app
// boots into this small project instead: a kick, a hat and a sub-bass analog preset copied
// verbatim from the demo (so it sounds right), one two-bar pattern, no automation, no send
// effects. Edits are in-memory like everywhere else in the skeleton; Reset restores this.
#pragma once

#include "core/Song.h"

namespace gl::android {

// Builds the starter song. Falls back to gl::makeEmptySong() if the embedded JSON fails to
// parse (it is parsed with the same songFromJson() the demo uses, so a parse failure means
// a build-time bug, not bad user data).
gl::Song makeLightStarterSong();

}  // namespace gl::android
