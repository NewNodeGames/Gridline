// Gridline for Android — see android_starter.h. The JSON is a literal on purpose: the app
// already embeds one larger project the same way (the demo, via cmake/EmbedDemo.cmake), and
// parsing at boot through songFromJson keeps the schema in exactly one place.
#include "android_starter.h"

#include <cstdio>
#include <string>

#include "core/Demo.h"
#include "core/Json.h"
#include "core/Project.h"

namespace gl::android {
namespace {

constexpr char kStarterJson[] = R"json({
  "format": "gridline-project",
  "version": 8,
  "info": {
    "title": "Starter",
    "author": "Gridline",
    "genre": "",
    "url": "",
    "comments": "A small starter project: kick, hat and a sub bass. Built for modest CPUs."
  },
  "ppq": 96,
  "tempo": 120,
  "timeSigNum": 4,
  "timeSigDen": 4,
  "swing": 0,
  "masterPitch": 0,
  "markers": [],
  "automations": [],
  "playlist": [],
  "links": [],
  "modLinks": [],
  "nextId": 1100,
  "patterns": [
    {
      "id": 1001,
      "name": "Starter",
      "stepsPerBar": 16,
      "bars": 2,
      "color": 5935064,
      "steps": {},
      "notes": {
        "1002": [
          {"start": 0,   "length": 24, "pitch": 60, "velocity": 112},
          {"start": 96,  "length": 24, "pitch": 60, "velocity": 112},
          {"start": 192, "length": 24, "pitch": 60, "velocity": 112},
          {"start": 288, "length": 24, "pitch": 60, "velocity": 112},
          {"start": 384, "length": 24, "pitch": 60, "velocity": 112},
          {"start": 480, "length": 24, "pitch": 60, "velocity": 112},
          {"start": 576, "length": 24, "pitch": 60, "velocity": 112},
          {"start": 672, "length": 24, "pitch": 60, "velocity": 112}
        ],
        "1003": [
          {"start": 0,   "length": 24, "pitch": 60, "velocity": 88},
          {"start": 48,  "length": 24, "pitch": 60, "velocity": 62},
          {"start": 96,  "length": 24, "pitch": 60, "velocity": 62},
          {"start": 144, "length": 24, "pitch": 60, "velocity": 62},
          {"start": 192, "length": 24, "pitch": 60, "velocity": 88},
          {"start": 240, "length": 24, "pitch": 60, "velocity": 62},
          {"start": 288, "length": 24, "pitch": 60, "velocity": 62},
          {"start": 336, "length": 24, "pitch": 60, "velocity": 62},
          {"start": 384, "length": 24, "pitch": 60, "velocity": 88},
          {"start": 432, "length": 24, "pitch": 60, "velocity": 62},
          {"start": 480, "length": 24, "pitch": 60, "velocity": 62},
          {"start": 528, "length": 24, "pitch": 60, "velocity": 62},
          {"start": 576, "length": 24, "pitch": 60, "velocity": 88},
          {"start": 624, "length": 24, "pitch": 60, "velocity": 62},
          {"start": 672, "length": 24, "pitch": 60, "velocity": 62},
          {"start": 720, "length": 24, "pitch": 60, "velocity": 62}
        ],
        "1004": [
          {"start": 0,   "length": 72, "pitch": 28, "velocity": 104},
          {"start": 144, "length": 48, "pitch": 28, "velocity": 94},
          {"start": 192, "length": 72, "pitch": 31, "velocity": 104},
          {"start": 336, "length": 48, "pitch": 26, "velocity": 94},
          {"start": 384, "length": 72, "pitch": 28, "velocity": 104},
          {"start": 528, "length": 48, "pitch": 28, "velocity": 94},
          {"start": 576, "length": 72, "pitch": 33, "velocity": 104},
          {"start": 720, "length": 48, "pitch": 35, "velocity": 94}
        ]
      }
    }
  ],
  "channels": [
    {
      "id": 1002,
      "name": "Kick",
      "color": 14711359,
      "gain": 0.8,
      "pan": 0.0,
      "muted": false,
      "pitch": 0,
      "plugin": {
        "type": "gl.kick",
        "params": [260.0, 52.0, 0.035, 0.3, 0.22, 0.12, 0.9, 0.0, 0.0, 3500.0, 0.003, 0.0,
                   9000.0, 0.0, 0.0, 0.08, 0.0]
      }
    },
    {
      "id": 1003,
      "name": "Hat",
      "color": 13150524,
      "gain": 0.55,
      "pan": 0.15,
      "muted": false,
      "pitch": 0,
      "plugin": {
        "type": "gl.hat",
        "params": [0.035, 9000.0, 0.0, 0.6, 0.1, 205.0, 0.6, 1.0, 0.0008, 0.0, 0.0]
      }
    },
    {
      "id": 1004,
      "name": "Sub Bass",
      "color": 5935064,
      "gain": 0.5,
      "pan": 0.0,
      "muted": false,
      "pitch": 0,
      "plugin": {
        "type": "gl.analog",
        "params": [0.8, 0.0, 0.0, 0.0, 16.0, 2.0, 0.045, 1.0, 0.1, 0.0, 1.0, 0.25, 0.7, 1.0,
                   0.0, 0.0, 0.0, 1.0, 0.5, 0.0, 0.5, 0.0, 2.0, 0.0, 7.0, 0.6, 0.5, 0.0,
                   0.5, 0.0, 3.0, -12.0, 0.0, 0.6, 0.5, 0.0, 0.5, 0.0, 0.0, 1.0, 0.0, 0.0,
                   240.0, 0.0, 0.0, 0.0, 0.0, 0.004, 0.3, 1.0, 0.09, 0.003, 0.4, 0.3, 0.3,
                   0.001, 0.25, 0.0, 0.2, 0.0, 0.0, 0.0, 5.0, 0.0, 8.0, 1.0, 0.001, 0.0,
                   0.0, 0.0, 0.5, 0.0, 8.0, 0.0, 0.001, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                   0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 2.0, 0.6, 0.0, 0.0,
                   0.6, 0.5, 0.5, 0.0, 7.0, 0.4, 0.25, 0.0, 0.6, 0.5, 0.25]
      }
    }
  ],
  "tracks": []
})json";

}  // namespace

gl::Song makeLightStarterSong() {
  gl::Json j;
  size_t pos = 0;
  if (!gl::jsonParse(std::string(kStarterJson), j, &pos)) {
    std::fprintf(stderr, "[starter] embedded starter JSON is invalid at byte %zu\n", pos);
    return gl::makeEmptySong();
  }
  gl::Song s;
  std::string err;
  if (!gl::songFromJson(j, s, &err)) {
    std::fprintf(stderr, "[starter] embedded starter song did not load: %s\n", err.c_str());
    return gl::makeEmptySong();
  }
  // Master bus only — no insert tracks. Channel mixerTrack values from the file are missing
  // on purpose (they land on the master), and the analog preset is the heaviest part; keep
  // the bus graph to one node.
  gl::ensureMixer(s, 0);
  return s;
}

}  // namespace gl::android
