// Gridline for Android — the channel rack (phase 4, milestone 1).
//
// A tablet-first, purpose-built view (not a port of the desktop rack): the channel list on
// the left (select, mute, gain — pan in the detail panel), the selected channel on the right
// with a 16-step piano grid for the selected pattern (toggle notes in place), transport and
// pattern selection in the bars above and below. Everything edits the in-memory gl::Document;
// the engine rebuilds its program when the document revision changes (same contract as the
// desktop). Undo/redo via Document. Step/note editing is 16th-grid only — no drag-paint, no
// velocity painting (next milestones), no files, no undo persistence across resets.
#pragma once
#include <utility>
#include <vector>

#include "imgui.h"

#include "core/Document.h"
#include "core/Song.h"
#include "engine/AudioEngine.h"
#include "engine/Program.h"

namespace gl::android {

struct RackState {
  ObjectId selectedPattern = kNoId;
  ObjectId selectedChannel = kNoId;
  int bar = 0;                  // visible bar in multi-bar patterns
  uint64_t builtRevision = ~uint64_t{0};  // force a program build on the first frame
  float tempoUi = 0.0f, tempoCommitted = 0.0f;
  // FL-style rack solo: mutes every other channel in one edit, restores the pre-solo
  // mute states when the armed channel is toggled again. No solo flag exists on the
  // channel model, and mixer-track solo would hit channels sharing a track.
  bool soloArmed = false;
  ObjectId soloOwner = kNoId;
  std::vector<std::pair<ObjectId, bool>> soloPrior;  // channel id -> muted before solo
};

// Picks the first pattern/channel, puts the engine in Pattern mode. Call once after the
// document is created (before the audio device is unpaused), so rackDraw does not race a
// null program.
void rackInit(gl::Document& doc, gl::AudioEngine& engine, RackState& st);

// Draws the whole screen for one frame: topology (top bar, channel list, detail grid,
// bottom transport bar). Rebuilds the engine program if `doc` changed. Returns true when the
// user pressed Quit.
bool rackDraw(RackState& st, gl::Document& doc, gl::AudioEngine& engine, gl::ProgramBuilder& builder,
              const char* audioStatus, bool audioOk, float scale, const ImVec2& display);

}  // namespace gl::android
