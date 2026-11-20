// The plain rock groove a kit is fitted to records with: 128 BPM, closed hats on the eighths, the
// kick on one and three (and the and of three), the snare on two and four, and every fourth bar a
// tom fill down the kit into a crash and kick, 16 bars (30 s), on the GM map. The strokes vary in
// velocity as a drummer's do, the same on every run. Shared by stem_gate_tests and
// bonkr-groove-render, which plays it through the built plug-in.
#pragma once

#include <algorithm>
#include <memory>
#include <vector>

#include "../src/engine.hpp"

namespace stem {

constexpr double kRate = 44100.0;  // the corpus's rate
constexpr double kBpm = 128.0;
constexpr int kBars = 16;
constexpr double kLeadS = 0.1, kTailS = 1.5;
constexpr uint32_t kBlock = 256;

struct Note { double at_s; int key; double velocity; double length_s; };

inline std::vector<Note> groove() {
  const double eighth = 30.0 / kBpm, sixteenth = eighth / 2.0;
  std::vector<Note> n;
  const auto hit = [&](double at, int key, double velocity) { n.push_back({kLeadS + at, key, velocity, 0.08}); };
  for (int bar = 0; bar < kBars; ++bar) {
    const double t0 = bar * 8 * eighth;
    const bool fill = bar % 4 == 3;
    for (int e = 0; e < 8; ++e) {
      const double t = t0 + e * eighth;
      const double wobble = 0.04 * ((bar * 7 + e * 3) % 5 - 2);
      if (fill && e >= 4) break;
      hit(t, 42, (e % 2 == 0 ? 0.62 : 0.45) + wobble);
      if (e == 0 || e == 4 || e == 5) hit(t, 36, (e == 5 ? 0.7 : 0.86) + wobble);
      if (e == 2 || e == 6) hit(t, 38, 0.86 + wobble);
    }
    if (fill) {
      static constexpr int kToms[8] = {50, 50, 48, 48, 47, 45, 43, 41};
      for (int i = 0; i < 8; ++i) hit(t0 + 4 * eighth + i * sixteenth, kToms[i], 0.78 + 0.02 * i);
    }
    if (bar % 4 == 0) hit(t0, 49, bar == 0 ? 0.8 : 0.92);
  }
  std::sort(n.begin(), n.end(), [](const Note& a, const Note& b) { return a.at_s < b.at_s; });
  return n;
}

inline double seconds() { return kLeadS + kBars * 8 * 30.0 / kBpm + kTailS; }

struct Audio { std::vector<float> left, right; };

// The groove through the engine with `s`.
inline Audio play(const bonkr::Settings& s) {
  auto engine = std::make_unique<bonkr::Engine>();
  engine->prepare(kRate);
  engine->set_tempo(kBpm);
  const size_t frames = static_cast<size_t>(seconds() * kRate);
  Audio out;
  out.left.assign(frames, 0.0f); out.right.assign(frames, 0.0f);
  struct Event { size_t at; int key; bool on; double velocity; };
  std::vector<Event> events;
  for (const Note& note : groove()) {
    events.push_back({static_cast<size_t>(note.at_s * kRate), note.key, true, note.velocity});
    events.push_back({static_cast<size_t>((note.at_s + note.length_s) * kRate), note.key, false, 0.0});
  }
  std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b) { return a.at < b.at; });
  size_t next = 0;
  for (size_t done = 0; done < frames;) {
    while (next < events.size() && events[next].at <= done) {
      if (events[next].on) engine->note_on(s, 0, events[next].key, -1, events[next].velocity);
      else engine->note_off(s, 0, events[next].key, -1);
      ++next;
    }
    size_t until = std::min(frames, done + kBlock);
    if (next < events.size()) until = std::min(until, std::max(done + 1, events[next].at));
    engine->render(s, out.left.data() + done, out.right.data() + done, static_cast<uint32_t>(until - done));
    done = until;
  }
  return out;
}

}  // namespace stem
