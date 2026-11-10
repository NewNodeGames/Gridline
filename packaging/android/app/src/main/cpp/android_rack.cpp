// Gridline — Android channel rack (phase 4, milestone 1). See android_rack.h for the contract.
//
// Layout (fullscreen, landscape tablet):
//   top bar     GRIDLINE · pattern prev/next + picker · PAT/SONG mode · audio status · Quit
//   left        channel list (select · mute · solo), scrollable
//   right       selected channel detail: name/plugin, gain + pan, 16-step grid of the
//               selected pattern (toggle notes), bar switcher for multi-bar patterns
//   bottom bar  transport: PLAY/STOP · position · tempo · undo/redo · Reset (starter) · Demo (heavy)
//
// Editing contract: every change is a gl::Document edit (undoable, single step for solo).
// The audio thread's program is rebuilt whenever doc.revision() moves — the same contract
// the desktop app uses. There is no persistence: edits live in memory until the process exits.
#include "android_rack.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

#include "imgui.h"

#include "core/Demo.h"
#include "android_starter.h"

namespace gl::android {
namespace {

constexpr ImU32 kPanel = IM_COL32(22, 22, 26, 235);
constexpr ImU32 kRow = IM_COL32(255, 255, 255, 10);
constexpr ImU32 kRowSelected = IM_COL32(96, 156, 214, 64);
constexpr ImU32 kCellOff = IM_COL32(255, 255, 255, 13);
constexpr ImU32 kCellBeat = IM_COL32(255, 255, 255, 30);
constexpr ImU32 kCellOnText = IM_COL32(15, 15, 18, 255);

ImU32 channelColor(Rgb c, int alpha) {
  return IM_COL32((c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff, alpha);
}

const Pattern* findPattern(const Song& s, ObjectId id) {
  auto it = std::find_if(s.patterns.begin(), s.patterns.end(), [&](const Pattern& p) { return p.id == id; });
  return it != s.patterns.end() ? &*it : nullptr;
}
const Channel* findChannel(const Song& s, ObjectId id) {
  auto it = std::find_if(s.channels.begin(), s.channels.end(), [&](const Channel& c) { return c.id == id; });
  return it != s.channels.end() ? &*it : nullptr;
}

// Notes of `ch` overlapping [from, to) in pattern `p`.
std::vector<const Note*> notesOverlapping(const Pattern& p, ObjectId ch, Tick from, Tick to) {
  std::vector<const Note*> out;
  auto it = p.notes.find(ch);
  if (it == p.notes.end()) return out;
  for (const Note& n : it->second) {
    const Tick len = std::max<Tick>(n.length, 1);
    if (n.start < to && n.start + len > from) out.push_back(&n);
  }
  return out;
}

void toggleSolo(RackState& st, Document& doc, ObjectId id) {
  if (st.soloArmed && st.soloOwner == id) {  // toggle off: restore the pre-solo mute mix
    doc.editSong("Unsolo channel", [&](Song& s) {
      for (Channel& ch : s.channels) {
        auto it = std::find_if(st.soloPrior.begin(), st.soloPrior.end(),
                               [&](const auto& p) { return p.first == ch.id; });
        if (it != st.soloPrior.end()) ch.muted = it->second;
      }
    });
    st.soloArmed = false;
    st.soloOwner = kNoId;
    st.soloPrior.clear();
    return;
  }
  if (!st.soloArmed) {  // first arm: remember the mute mix to restore later
    st.soloPrior.clear();
    for (const Channel& ch : doc.song().channels) st.soloPrior.emplace_back(ch.id, ch.muted);
    st.soloArmed = true;
  }
  doc.editSong("Solo channel", [&](Song& s) {
    for (Channel& ch : s.channels) ch.muted = (ch.id != id);
  });
  st.soloOwner = id;
}

}  // namespace

void rackInit(gl::Document& doc, gl::AudioEngine& engine, RackState& st) {
  const Song& s = doc.song();
  st.selectedPattern = s.patterns.empty() ? kNoId : s.patterns.front().id;
  st.selectedChannel = s.channels.empty() ? kNoId : s.channels.front().id;
  st.bar = 0;
  st.soloArmed = false;
  st.soloOwner = kNoId;
  st.soloPrior.clear();
  st.tempoUi = static_cast<float>(s.tempo);
  st.tempoCommitted = st.tempoUi;
  engine.setMode(gl::PlayMode::Pattern);
  engine.setPattern(st.selectedPattern);
  st.builtRevision = doc.revision();
}

bool rackDraw(RackState& st, gl::Document& doc, gl::AudioEngine& engine, gl::ProgramBuilder& builder,
              const char* audioStatus, bool audioOk, float scale, const ImVec2& display) {
  // Keep the audio program in sync with the document (single-threaded contract with the engine).
  if (doc.revision() != st.builtRevision) {
    if (audioOk) engine.setProgram(builder.build(doc.song()));
    st.builtRevision = doc.revision();
  }
  engine.setPattern(st.selectedPattern);

  const Song& song = doc.song();
  const Pattern* pattern = findPattern(song, st.selectedPattern);
  const Channel* channel = findChannel(song, st.selectedChannel);
  if (st.bar >= (pattern ? pattern->bars : 1)) st.bar = 0;

  const float s = scale;
  const float topH = 58.0f * s;
  const float bottomH = 84.0f * s;
  const float leftW = 300.0f * s;
  const ImVec4 accent{0.36f, 0.66f, 0.90f, 1.0f};
  const ImVec4 dim{0.60f, 0.60f, 0.62f, 1.0f};
  const ImVec4 warn{0.91f, 0.52f, 0.36f, 1.0f};
  const ImVec2 bodyPos(0.0f, topH);
  const ImVec2 bodySize(display.x, display.y - topH - bottomH);
  bool quit = false;

  // ================= top bar =================
  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(display.x, topH), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.92f);
  ImGui::Begin("##topbar", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
  ImGui::PushStyleColor(ImGuiCol_Text, accent);
  ImGui::TextUnformatted("G R I D L I N E");
  ImGui::PopStyleColor();
  ImGui::SameLine(0.0f, 12.0f * s);
  ImGui::PushStyleColor(ImGuiCol_Text, dim);
  ImGui::TextUnformatted("channel rack");
  ImGui::PopStyleColor();
  ImGui::SameLine();

  // Pattern nav: prev/next + picker.
  if (!song.patterns.empty()) {
    auto patternIndex = [&]() {
      for (size_t i = 0; i < song.patterns.size(); ++i)
        if (song.patterns[i].id == st.selectedPattern) return static_cast<int>(i);
      return 0;
    };
    const int idx = patternIndex();
    ImGui::SameLine(display.x * 0.30f);
    if (ImGui::Button("<", ImVec2(44.0f * s, 40.0f * s)) && idx > 0) {
      st.selectedPattern = song.patterns[idx - 1].id;
      st.bar = 0;
    }
    ImGui::SameLine();
    char preview[96];
    std::snprintf(preview, sizeof preview, "%d  %s", idx + 1, song.patterns[idx].name.c_str());
    ImGui::SetNextItemWidth(240.0f * s);
    if (ImGui::BeginCombo("##pattern", preview)) {
      for (int i = 0; i < static_cast<int>(song.patterns.size()); ++i) {
        const bool sel = i == idx;
        char label[96];
        std::snprintf(label, sizeof label, "%d  %s", i + 1, song.patterns[i].name.c_str());
        if (ImGui::Selectable(label, sel)) {
          st.selectedPattern = song.patterns[i].id;
          st.bar = 0;
        }
        if (sel) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button(">", ImVec2(44.0f * s, 40.0f * s)) && idx + 1 < static_cast<int>(song.patterns.size())) {
      st.selectedPattern = song.patterns[idx + 1].id;
      st.bar = 0;
    }
  }

  // PAT/SONG mode toggle (active side is highlighted).
  ImGui::SameLine(display.x * 0.62f);
  const gl::PlayMode mode = engine.mode();
  const ImVec4 dimBtn(0.20f, 0.20f, 0.22f, 1.0f);
  ImGui::PushStyleColor(ImGuiCol_Button, mode == gl::PlayMode::Pattern ? accent : dimBtn);
  if (ImGui::Button("PAT", ImVec2(52.0f * s, 40.0f * s))) engine.setMode(gl::PlayMode::Pattern);
  ImGui::PopStyleColor();
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, mode == gl::PlayMode::Song ? accent : dimBtn);
  if (ImGui::Button("SONG", ImVec2(52.0f * s, 40.0f * s))) engine.setMode(gl::PlayMode::Song);
  ImGui::PopStyleColor();
  // Active-mode highlight is the button's own press feedback; the PAT/SONG choice is small
  // enough to read from what you hear.

  ImGui::SameLine(display.x * 0.78f);
  ImGui::PushStyleColor(ImGuiCol_Text, audioOk ? dim : warn);
  ImGui::TextUnformatted(audioStatus);
  ImGui::PopStyleColor();
  ImGui::SameLine();
  if (ImGui::Button("Quit", ImVec2(72.0f * s, 40.0f * s))) quit = true;
  ImGui::End();

  // ================= left: channel list =================
  ImGui::SetNextWindowPos(bodyPos, ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(leftW, bodySize.y), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.92f);
  ImGui::Begin("##channels", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
  ImGui::PushStyleColor(ImGuiCol_Text, dim);
  ImGui::Text("CHANNELS  ·  %d", static_cast<int>(song.channels.size()));
  ImGui::PopStyleColor();
  ImGui::BeginChild("##chanlist", ImVec2(0, 0), false);

  const float rowH = 40.0f * s;
  const float btn = 34.0f * s;
  for (size_t ci = 0; ci < doc.song().channels.size(); ++ci) {
    const Channel& ch = doc.song().channels[ci];
    const ObjectId chId = ch.id;  // an id copy survives edits that swap the song's fields
    ImGui::PushID(static_cast<int>(chId));
    const ImVec2 rowMin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const bool selected = ch.id == st.selectedChannel;
    const bool armed = st.soloArmed && st.soloOwner == ch.id;

    ImGui::InvisibleButton("##row", ImVec2(width, rowH));
    const bool rowClicked = ImGui::IsItemClicked();
    const bool rowHovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 rowMax(rowMin.x + width, rowMin.y + rowH);
    dl->AddRectFilled(rowMin, rowMax, selected ? kRowSelected : (rowHovered ? IM_COL32(255, 255, 255, 18) : kRow), 8.0f);
    dl->AddRectFilled(ImVec2(rowMin.x + 6.0f * s, rowMin.y + rowH * 0.22f),
                      ImVec2(rowMin.x + 6.0f * s + 10.0f * s, rowMin.y + rowH * 0.78f), channelColor(ch.color, 255),
                      3.0f * s);
    dl->AddText(ImVec2(rowMin.x + 6.0f * s + 10.0f * s + 12.0f * s, rowMin.y + rowH * 0.5f - ImGui::GetTextLineHeight() * 0.5f),
                ch.muted ? IM_COL32(150, 150, 155, 255) : IM_COL32(235, 235, 238, 255), ch.name.c_str());

    // Mute / solo: custom-drawn buttons layered on the row (right-aligned).
    const float mx = rowMax.x - 6.0f * s - btn * 2.0f - 6.0f * s;
    const float my = rowMin.y + (rowH - btn) * 0.5f;
    const ImVec2 mMin(mx, my), mMax(mx + btn, my + btn);
    const ImVec2 sMin(mMax.x + 6.0f * s, my), sMax(mMax.x + 6.0f * s + btn, my + btn);
    ImGui::SetCursorScreenPos(mMin);
    ImGui::InvisibleButton("##mute", ImVec2(btn, btn));
    const bool mClicked = ImGui::IsItemClicked();
    ImGui::SetCursorScreenPos(sMin);
    ImGui::InvisibleButton("##solo", ImVec2(btn, btn));
    const bool sClicked = ImGui::IsItemClicked();
    dl->AddRectFilled(mMin, mMax, ch.muted ? IM_COL32(226, 130, 60, 235) : kCellOff, 6.0f * s);
    dl->AddRectFilled(sMin, sMax, armed ? IM_COL32(226, 190, 60, 235) : kCellOff, 6.0f * s);
    const ImVec2 mMid((mMin.x + mMax.x) * 0.5f, (mMin.y + mMax.y) * 0.5f);
    const ImVec2 sMid((sMin.x + sMax.x) * 0.5f, (sMin.y + sMax.y) * 0.5f);
    const ImVec2 tHalf(ImGui::CalcTextSize("M").x * 0.5f, ImGui::GetTextLineHeight() * 0.5f);
    const ImU32 mText = ch.muted ? kCellOnText : IM_COL32(205, 205, 210, 255);
    const ImU32 sText = armed ? kCellOnText : IM_COL32(205, 205, 210, 255);
    dl->AddText(ImVec2(mMid.x - tHalf.x, mMid.y - tHalf.y), mText, "M");
    dl->AddText(ImVec2(sMid.x - tHalf.x, sMid.y - tHalf.y), sText, "S");

    if (mClicked) doc.editChannel(chId, "Toggle mute", [](Channel& c) { c.muted = !c.muted; });
    if (sClicked) toggleSolo(st, doc, chId);  // may swap the song's fields; ch is not read after this
    if (rowClicked && !mClicked && !sClicked) st.selectedChannel = chId;
    ImGui::SetCursorScreenPos(ImVec2(rowMin.x, rowMax.y + 2.0f * s));  // rows are not spaced by layout
    ImGui::Spacing();
    ImGui::PopID();
  }
  ImGui::EndChild();
  ImGui::End();

  // ================= right: selected channel =================
  ImGui::SetNextWindowPos(ImVec2(leftW, topH), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(display.x - leftW, bodySize.y), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.92f);
  ImGui::Begin("##detail", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

  if (channel && pattern) {
    const ImVec4 chipColor(static_cast<float>((channel->color >> 16) & 0xff) / 255.0f,
                           static_cast<float>((channel->color >> 8) & 0xff) / 255.0f,
                           static_cast<float>(channel->color & 0xff) / 255.0f, 1.0f);
    ImGui::TextColored(chipColor, "■");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.96f, 1.0f));
    ImGui::TextUnformatted(channel->name.c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, dim);
    ImGui::TextUnformatted(channel->plugin.type.c_str());
    ImGui::PopStyleColor();

    // Gain / pan sliders (live: every edit rebuilds the program; the engine continues in place).
    float gain = channel->gain, pan = channel->pan;
    ImGui::SetNextItemWidth(display.x * 0.24f);
    if (ImGui::SliderFloat("##gain", &gain, 0.0f, 1.25f, "Gain %.2f"))
      doc.editChannel(channel->id, "Channel gain", [gain](Channel& c) { c.gain = gain; }, /*mergeKey*/ 1);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(display.x * 0.20f);
    if (ImGui::SliderFloat("##pan", &pan, -1.0f, 1.0f, "Pan %+.2f"))
      doc.editChannel(channel->id, "Channel pan", [pan](Channel& c) { c.pan = pan; }, /*mergeKey*/ 2);
    ImGui::SameLine();
    const bool armed = st.soloArmed && st.soloOwner == channel->id;
    if (ImGui::Button(channel->muted ? "UNMUTE" : "MUTE", ImVec2(96.0f * s, 0.0f)))
      doc.editChannel(channel->id, "Toggle mute", [](Channel& c) { c.muted = !c.muted; });
    ImGui::SameLine();
    if (ImGui::Button(armed ? "UNOSOLO" : "SOLO", ImVec2(96.0f * s, 0.0f))) toggleSolo(st, doc, channel->id);
    // SOLO swaps the song's fields — re-fetch both pointers before anything below reads them.
    pattern = findPattern(doc.song(), st.selectedPattern);
    channel = findChannel(doc.song(), st.selectedChannel);

    ImGui::Spacing();
    int patternIdx = 0;
    for (int i = 0; i < static_cast<int>(song.patterns.size()); ++i)
      if (song.patterns[i].id == st.selectedPattern) patternIdx = i;
    ImGui::PushStyleColor(ImGuiCol_Text, dim);
    ImGui::Text("Pattern %d  ·  %s  ·  %d steps/bar  ·  %d bar%s", patternIdx + 1, pattern->name.c_str(),
                pattern->stepsPerBar, pattern->bars, pattern->bars == 1 ? "" : "s");
    ImGui::PopStyleColor();
    // (pattern index is shown in the top bar picker; here just the name and grid shape)

    // Bar switcher for multi-bar patterns.
    const Tick barTicks = song.barTicks();
    const Tick tps = song.ticksPerStep(*pattern);
    const int stepsPerBar = std::max(1, pattern->stepsPerBar);
    if (pattern->bars > 1) {
      for (int b = 0; b < pattern->bars; ++b) {
        char lbl[16];
        std::snprintf(lbl, sizeof lbl, "Bar %d", b + 1);
        if (b > 0) ImGui::SameLine();
        if (ImGui::Button(lbl, ImVec2(72.0f * s, 32.0f * s))) st.bar = b;
      }
    }

    ImGui::Spacing();

    // ---- the 16-step grid ----
    const int steps = stepsPerBar;
    const float gap = 4.0f * s;
    const float avail = ImGui::GetContentRegionAvail().x;
    const float cellW = std::max(28.0f * s, (avail - gap * (steps - 1)) / static_cast<float>(steps));
    const float cellH = 46.0f * s;
    const Tick cellTicks = tps;
    const Tick barBase = static_cast<Tick>(st.bar) * stepsPerBar * cellTicks;

    const ImVec2 gridMin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int i = 0; i < steps; ++i) {
      const Tick from = barBase + static_cast<Tick>(i) * cellTicks;
      const Tick to = from + cellTicks;
      const auto hits = notesOverlapping(*pattern, channel->id, from, to);
      const bool on = !hits.empty();
      const bool beat = (i % 4) == 0;
      int pitchShown = -1;
      bool samePitch = true;
      for (const Note* n : hits) {
        if (pitchShown < 0) pitchShown = n->pitch;
        else if (pitchShown != n->pitch) samePitch = false;
      }

      ImGui::PushID(1000 + i);
      const ImVec2 cMin(gridMin.x + i * (cellW + gap), gridMin.y);
      const ImVec2 cMax(cMin.x + cellW, gridMin.y + cellH);
      ImGui::SetCursorScreenPos(cMin);
      ImGui::InvisibleButton("##cell", ImVec2(cellW, cellH));
      const bool hovered = ImGui::IsItemHovered();
      if (ImGui::IsItemClicked()) {
        doc.editPattern(pattern->id, "Toggle step", [&](Pattern& p) {
          auto& row = p.notes[channel->id];
          bool any = false;
          for (const Note& n : row) {
            const Tick len = std::max<Tick>(n.length, 1);
            if (n.start < to && n.start + len > from) { any = true; break; }
          }
          if (any) {
            row.erase(std::remove_if(row.begin(), row.end(), [&](const Note& n) {
                        const Tick len = std::max<Tick>(n.length, 1);
                        return n.start < to && n.start + len > from;
                      }),
                      row.end());
          } else {
            row.push_back(Note(from, cellTicks, /*pitch*/ 60, /*velocity*/ 100));
            std::sort(row.begin(), row.end(), [](const Note& a, const Note& b) { return a.start < b.start; });
          }
        });
      }
      const ImU32 off = beat ? kCellBeat : kCellOff;
      dl->AddRectFilled(cMin, cMax, on ? channelColor(channel->color, 235) : (hovered ? IM_COL32(255, 255, 255, 26) : off),
                        6.0f * s);
      if (beat) dl->AddRect(cMin, cMax, IM_COL32(255, 255, 255, 40), 6.0f * s, 0, 1.0f);
      if (on) {
        char txt[8];
        if (samePitch && pitchShown >= 0) std::snprintf(txt, sizeof txt, "%d", pitchShown);
        else std::snprintf(txt, sizeof txt, "+");
        const ImVec2 ts = ImGui::CalcTextSize(txt);
        dl->AddText(ImVec2((cMin.x + cMax.x - ts.x) * 0.5f, (cMin.y + cMax.y - ts.y) * 0.5f), kCellOnText, txt);
      }
      // beat number under every 4th cell
      if (beat) {
        char n[4];
        std::snprintf(n, sizeof n, "%d", i / 4 + 1);
        dl->AddText(ImVec2(cMin.x + 2.0f * s, cMax.y + 2.0f * s), IM_COL32(120, 120, 126, 255), n);
      }
      ImGui::PopID();
    }
    // Playhead over the visible bar.
    if (engine.playing() && engine.mode() == gl::PlayMode::Pattern) {
      const double pos = engine.position();
      const Tick patLen = song.patternLength(*pattern);
      double inPat = patLen > 0 ? std::fmod(pos, static_cast<double>(patLen)) : 0.0;
      const double rel = (inPat - static_cast<double>(barBase)) / static_cast<double>(cellTicks);
      if (rel >= 0.0 && rel < steps) {
        const float x = gridMin.x + static_cast<float>(rel) * (cellW + gap);
        dl->AddRectFilled(ImVec2(x - 1.0f, gridMin.y - 4.0f * s), ImVec2(x + 1.0f, gridMin.y + cellH + 4.0f * s),
                          IM_COL32(240, 240, 245, 220));
      }
    }
    ImGui::SetCursorScreenPos(ImVec2(gridMin.x, gridMin.y + cellH + 18.0f * s));
    ImGui::Dummy(ImVec2(1.0f, 1.0f));  // beat numbers live in the gap; reserve it below the cells

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, dim);
    ImGui::TextUnformatted("Tap a cell to add/remove a note (C5). Existing notes in a cell are replaced as a group.");
    ImGui::TextUnformatted("Edits are live and in-memory only — nothing is saved to a file.");
    ImGui::PopStyleColor();
  } else {
    ImGui::TextUnformatted("no channel selected");
  }
  ImGui::End();

  // ================= bottom transport bar =================
  ImGui::SetNextWindowPos(ImVec2(0, display.y - bottomH), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(display.x, bottomH), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.92f);
  ImGui::Begin("##transport", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

  const bool playing = engine.playing();
  ImGui::PushStyleColor(ImGuiCol_Button, playing ? accent : ImVec4(0.20f, 0.20f, 0.22f, 1.0f));
  if (ImGui::Button(playing ? "STOP" : "PLAY", ImVec2(150.0f * s, 56.0f * s))) engine.togglePlay();
  ImGui::PopStyleColor();

  ImGui::SameLine();
  // Position readout: bar.beat.sixteenth within the pattern (pattern mode) or the song.
  {
    const double pos = engine.position();
    const Tick bt = song.barTicks();
    const Tick beatT = song.beatTicks();
    const Tick sixT = beatT / 4;
    const long long p = static_cast<long long>(std::max(0.0, pos));
    const long long bar = bt > 0 ? p / bt : 0, beat = bt > 0 ? (p % bt) / beatT : 0, six = sixT > 0 ? (p % beatT) / sixT : 0;
    char posText[32];
    std::snprintf(posText, sizeof posText, "%lld.%lld.%lld", bar + 1, beat + 1, six + 1);
    ImGui::PushStyleColor(ImGuiCol_Text, dim);
    ImGui::TextUnformatted(posText);
    ImGui::PopStyleColor();
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(display.x * 0.26f);
  ImGui::SliderFloat("##tempo", &st.tempoUi, 60.0f, 220.0f, "Tempo %.1f BPM");
  if (ImGui::IsItemDeactivatedAfterEdit() && st.tempoCommitted != st.tempoUi) {
    st.tempoCommitted = st.tempoUi;
    doc.editSong("Tempo", [&](Song& s) { s.tempo = static_cast<double>(st.tempoUi); });  // rebuilds via revision
  }

  ImGui::SameLine();
  if (ImGui::Button("Undo", ImVec2(76.0f * s, 44.0f * s))) doc.undo();
  ImGui::SameLine();
  if (ImGui::Button("Redo", ImVec2(76.0f * s, 44.0f * s))) doc.redo();
  ImGui::SameLine();
  if (ImGui::Button("Reset", ImVec2(96.0f * s, 44.0f * s))) ImGui::OpenPopup("##resetConfirm");
  ImGui::SameLine();
  if (ImGui::Button("Demo", ImVec2(76.0f * s, 44.0f * s))) ImGui::OpenPopup("##demoConfirm");

  // Swaps in a fresh built-in song. `song` (bound at the top of the frame) dangles after this;
  // nothing below the popups reads it.
  auto reloadWith = [&](const std::function<gl::Song()>& factory) {
    engine.stopToStart();
    doc = gl::Document(factory());
    engine.setMode(gl::PlayMode::Pattern);
    rackInit(doc, engine, st);
    st.builtRevision = ~uint64_t{0};  // force rebuild from the new song
  };

  // Both confirmations discard the live edits (they were never saved).
  if (ImGui::BeginPopupModal("##resetConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted("Reset to the starter song?");
    ImGui::TextUnformatted("Your live edits are discarded (they were never saved).");
    ImGui::Spacing();
    if (ImGui::Button("Reset", ImVec2(120.0f * s, 40.0f * s))) {
      reloadWith(gl::android::makeLightStarterSong);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Keep editing", ImVec2(120.0f * s, 40.0f * s))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
  if (ImGui::BeginPopupModal("##demoConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted("Load the full demo song?");
    ImGui::TextUnformatted("It is very heavy on this device; playback will likely stutter.");
    ImGui::TextUnformatted("Your live edits are discarded (they were never saved).");
    ImGui::Spacing();
    if (ImGui::Button("Load demo", ImVec2(120.0f * s, 40.0f * s))) {
      reloadWith(gl::makeDemoTrack);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Keep starter", ImVec2(120.0f * s, 40.0f * s))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
  ImGui::End();

  return quit;
}

}  // namespace gl::android
