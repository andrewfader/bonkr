// The phrase every preset is judged on (docs/QUALITY.md): for a kit, a bar of groove on the GM
// map (kick, snare, closed and open hats), a tom fill and a crash on the downbeat; for one sound
// across the keyboard, a held C4 with the E and G above it struck after it, then three short
// strokes higher up. Then the preset's own tail. Shared by the
// bonkr-preset-audit tool and engine_tests so the bar is measured the same way in both.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include <string>

#include "../src/engine.hpp"
#include "audio_measure.hpp"

namespace bonkr::audit {

constexpr double kRate = 48000.0;
constexpr uint32_t kBlock = 256;
constexpr double kFirstNoteSeconds = 0.30;
constexpr double kPhraseEndSeconds = 3.1;
// The level every preset is matched on: the loudest 400 ms of the phrase (BS.1770 momentary
// loudness of the mono sum) lands here, within the slack below, with the limiter asked for no
// more than kMaxLimitDb of reduction over the whole phrase.
constexpr double kTargetLufs = -18.0;
constexpr double kLoudnessSlackDb = 2.0;
constexpr double kMaxLimitDb = Limiter::kClipDb;
constexpr double kImpulsiveLimitDb = 5.5;

struct Note { double at_s; int key; double velocity; double length_s; };
inline const std::vector<Note>& groove() {
  static const std::vector<Note> notes{
      {0.30, 36, 0.85, 0.1}, {0.30, 42, 0.6, 0.1}, {0.55, 42, 0.45, 0.1}, {0.80, 38, 0.85, 0.1}, {0.80, 42, 0.6, 0.1},
      {1.05, 42, 0.45, 0.1}, {1.30, 36, 0.8, 0.1}, {1.42, 36, 0.6, 0.1}, {1.30, 42, 0.6, 0.1}, {1.55, 46, 0.55, 0.2},
      {1.80, 38, 0.9, 0.1}, {1.80, 44, 0.5, 0.1}, {2.05, 50, 0.8, 0.1}, {2.18, 48, 0.8, 0.1}, {2.31, 45, 0.85, 0.1},
      {2.44, 43, 0.9, 0.1}, {2.60, 49, 0.95, 0.4}, {2.60, 36, 0.95, 0.1},
  };
  return notes;
}
inline const std::vector<Note>& melody() {
  static const std::vector<Note> notes{
      {0.30, 60, 0.75, 1.6}, {0.80, 64, 0.7, 1.1}, {1.30, 67, 0.7, 0.6},
      {2.10, 72, 0.8, 0.22}, {2.35, 74, 0.7, 0.22}, {2.60, 76, 0.9, 0.45},
  };
  return notes;
}
inline const std::vector<Note>& phrase(const Settings& s) { return s.chromatic() ? melody() : groove(); }

struct Rendered { std::vector<float> left, right, mono; size_t first_note = 0, phrase_end = 0; };

// Renders the phrase through `engine` with `s`, then the tail up to `max_tail_s`.
inline Rendered render(const Settings& s, Engine& engine, double max_tail_s = 10.0) {
  const double total_s = kPhraseEndSeconds + std::min(max_tail_s, engine.tail_samples(s) / kRate) + 0.2;
  const size_t frames = static_cast<size_t>(total_s * kRate);
  Rendered out;
  out.left.assign(frames, 0.0f); out.right.assign(frames, 0.0f);
  out.first_note = static_cast<size_t>(kFirstNoteSeconds * kRate);
  out.phrase_end = static_cast<size_t>(kPhraseEndSeconds * kRate);
  struct Pending { size_t at; int key; bool on; double velocity; };
  std::vector<Pending> events;
  for (const Note& n : phrase(s)) {
    events.push_back({static_cast<size_t>(n.at_s * kRate), n.key, true, n.velocity});
    events.push_back({static_cast<size_t>((n.at_s + n.length_s) * kRate), n.key, false, 0.0});
  }
  std::sort(events.begin(), events.end(), [](const Pending& a, const Pending& b) { return a.at < b.at; });
  size_t next = 0;
  for (size_t done = 0; done < frames; done += kBlock) {
    const uint32_t n = static_cast<uint32_t>(std::min<size_t>(kBlock, frames - done));
    while (next < events.size() && events[next].at < done + n) {
      if (events[next].on) engine.note_on(s, 0, events[next].key, -1, events[next].velocity);
      else engine.note_off(s, 0, events[next].key, -1);
      ++next;
    }
    engine.render(s, out.left.data() + done, out.right.data() + done, n);
  }
  out.mono.resize(frames);
  for (size_t i = 0; i < frames; ++i) out.mono[i] = 0.5f * (out.left[i] + out.right[i]);
  return out;
}

// The quality bar's measurements of one rendered preset (docs/QUALITY.md).
struct Bar {
  double peak_db = 0, loudness = 0, limit_db = 0, first_db = 0, dc_db = 0, idle_db = 0, bright = 0, width = 0, tail_db = 0;
  bool finite = true, clipped = false;
  std::string flags;
};
inline Bar measure_bar(const Settings& s, Engine& engine, const Rendered& r) {
  Bar b;
  for (float v : r.left) b.finite = b.finite && std::isfinite(v);
  for (float v : r.right) b.finite = b.finite && std::isfinite(v);
  double peak = 0.0;
  for (size_t i = 0; i < r.left.size(); ++i) peak = std::max({peak, static_cast<double>(std::abs(r.left[i])), static_cast<double>(std::abs(r.right[i]))});
  b.peak_db = 20.0 * std::log10(std::max(peak, 1e-12));
  b.loudness = measure::momentary_loudness_max(r.mono, kRate);
  b.limit_db = engine.limited_db();
  b.clipped = engine.clipped();
  const measure::Span phrase(r.mono, r.first_note, r.phrase_end - r.first_note);
  b.first_db = measure::db(measure::peak(measure::Span(r.mono, r.first_note, static_cast<size_t>(0.25 * kRate))));
  b.dc_db = measure::db(std::abs(measure::mean(phrase)));
  b.idle_db = measure::db(measure::rms(measure::Span(r.mono, 2400, r.first_note - 2400 - 512)));
  b.bright = measure::spectral_centroid(phrase, kRate, 12);
  b.width = 1.0 - measure::correlation(measure::Span(r.left, r.first_note, r.phrase_end - r.first_note), measure::Span(r.right, r.first_note, r.phrase_end - r.first_note));
  b.tail_db = measure::db(measure::peak(measure::Span(r.mono, r.mono.size() - 4800)));
  if (!b.finite) b.flags += " NONFINITE";
  if (b.peak_db > -1.5) b.flags += " HOT";
  if (b.loudness > kTargetLufs + kLoudnessSlackDb) b.flags += " LOUD";
  // An impulsive preset (a gunshot, a cannon) reaches the limiter's allowance before its
  // momentary loudness reaches the target: it is then as loud as it can be without squashing.
  if (b.loudness < kTargetLufs - kLoudnessSlackDb && b.limit_db < kImpulsiveLimitDb) b.flags += " QUIET";
  if (b.first_db < -60.0) b.flags += " SILENT";
  if (b.dc_db > -40.0) b.flags += " DC";
  if (b.idle_db > -55.0) b.flags += " NOISY";
  if (b.tail_db > -50.0 && engine.tail_samples(s) / kRate <= 10.0) b.flags += " TAIL";
  if (b.clipped || b.limit_db > kMaxLimitDb) b.flags += " CLIP";
  return b;
}

}  // namespace bonkr::audit
