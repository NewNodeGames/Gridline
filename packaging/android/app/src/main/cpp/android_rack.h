// Gridline for Android — the channel rack (phase 4, milestone 3: roll depth).
//
// A tablet-first, purpose-built view (not a port of the desktop rack): the channel list on
// the left (select, mute, gain — pan in the detail panel), the selected channel on the right
// with either a 16-step piano grid (toggle notes in place) or a piano roll (per-pitch cells,
// tap to add / remove notes, pitch window auto-fitted to content with octave shift, mouse-wheel
// octave jump, snap selector 1/8–1/16–1/32, drag a note's right edge to resize it), plus
// delete-channel (DEL) for thinning songs out. Transport and
// pattern selection in the bars above and below. Everything edits the in-memory gl::Document;
// the engine rebuilds its program when the document revision changes (same contract as the
// desktop). Undo/redo via Document. No drag-paint, no velocity painting, no selection marquee
// (later milestones), no files, no undo persistence across resets.
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
  int detailMode = 0;   // 0 = step grid (16 steps/bar), 1 = piano roll (per-pitch cells)
  int rollLo = 48, rollHi = 71;  // persistent visible pitch window (expand-only, wheel / 8va buttons move it); 24 rows by default
  int rollGrid = 1;        // roll snap: 0 = 1/8, 1 = 1/16 (default), 2 = 1/32 — notes place/resize on snap ticks
  // Resize gesture: press and drag the right edge of a note (in its row) to change its length.
  // Valid between the press that grabs a note edge and the release that ends the drag.
  bool resizeGesture = false;
  int resizePitch = -1;
  Tick resizeStart = 0;
  ObjectId pendingDelete = kNoId;  // channel awaiting the DEL confirm popup
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
