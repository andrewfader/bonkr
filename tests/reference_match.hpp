// BONKR against recordings of the real instruments. references/features.tsv holds the features
// (tests/timbre.hpp) of every reference recording, extracted once by `bonkr-reference extract`;
// this strikes the same sound through the engine (dry, no room, the default settings, a firm
// stroke), extracts the same features and scores the difference.
//
// The score is in units of a just-plain-audible difference on each measure, summed: 6 dB of RMS
// difference across the octave-band profile, half an octave of brightness, a factor of two in
// the ring time, 6 dB of flatness. Real recordings of one instrument differ among themselves
// too (another drum, another room, another microphone), so a sound's spread is the mean score
// between its recordings, and the quality gate asks that BONKR sit no further from them, on
// average, than twice that spread, or 4 (whichever is larger); with one recording, no further
// than 5. The gate depends only on the recordings.
// references/baseline.tsv holds the scores BONKR last reached, which may not get worse.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "../src/engine.hpp"
#include "timbre.hpp"

namespace reference {

struct Recording { std::string sound, id; timbre::Features f; };

inline int sound_index(const std::string& name) {
  static const char* const kIds[] = {
#define BONKR_ID(id, name) #id,
      BONKR_SOUNDS(BONKR_ID)
#undef BONKR_ID
  };
  for (int i = 0; i < bonkr::dsp::kSoundCount; ++i) if (name == kIds[i]) return i;
  return -1;
}
inline std::string sound_id(int s) {
  static const char* const kIds[] = {
#define BONKR_ID(id, name) #id,
      BONKR_SOUNDS(BONKR_ID)
#undef BONKR_ID
  };
  return kIds[s];
}

inline std::vector<Recording> load_features(const std::string& path) {
  std::vector<Recording> out;
  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::stringstream s(line);
    Recording r;
    std::string field;
    std::getline(s, r.sound, '\t');
    std::getline(s, r.id, '\t');
    double* numbers[] = {&r.f.centroid, &r.f.decay_ms, &r.f.attack_ms, &r.f.flatness_db};
    for (double* d : numbers) { std::getline(s, field, '\t'); *d = std::stod(field); }
    for (double& p : r.f.profile) { std::getline(s, field, '\t'); p = std::stod(field); }
    out.push_back(r);
  }
  return out;
}
inline void write_features(std::FILE* out, const Recording& r) {
  std::fprintf(out, "%s\t%s\t%.4f\t%.1f\t%.2f\t%.2f", r.sound.c_str(), r.id.c_str(), r.f.centroid, r.f.decay_ms, r.f.attack_ms, r.f.flatness_db);
  for (double p : r.f.profile) std::fprintf(out, "\t%.2f", p);
  std::fprintf(out, "\n");
}

// How long to look at a sound: what rings or arrives late gets two seconds.
inline double window_s(int s) {
  using namespace bonkr::dsp;
  switch (family(s)) {
    case Family::Cymbal: case Family::Bar: case Family::Machine: case Family::Blast: return 2.0;
    default: break;
  }
  if (s == kTimpani || s == kBassDrum || s == kODaiko || s == kTR808Cymbal || s == kTR909Crash || s == kTR909Ride || s == kTR808HatOpen || s == kTR909HatOpen) return 2.0;
  return 0.6;
}

// One stroke of a sound through the engine: dry, master at 0 dB, a firm stroke, held through the
// window for the sounds that are held.
inline timbre::Features play(int sound, double rate = 48000.0) {
  using namespace bonkr;
  auto engine = std::make_unique<Engine>();
  engine->prepare(rate);
  Settings s = Settings::defaults();
  s.v[params::kLayout] = params::kLayoutChromatic;
  s.v[params::kSound] = sound;
  s.v[params::kMasterDb] = 0.0;
  s.v[params::kStereoWidth] = 0.0;
  const double window = window_s(sound);
  std::vector<float> l(static_cast<size_t>((window + 0.3) * rate)), r(l.size());
  engine->note_on(s, 0, params::kChromaticRoot, -1, 0.8);
  const size_t release = static_cast<size_t>(window * rate);
  for (size_t at = 0; at < l.size(); at += 256) {
    const uint32_t n = static_cast<uint32_t>(std::min<size_t>(256, l.size() - at));
    if (at <= release && release < at + n) engine->note_off(s, 0, params::kChromaticRoot, -1);
    engine->render(s, l.data() + at, r.data() + at, n);
  }
  std::vector<float> m(l.size());
  for (size_t i = 0; i < m.size(); ++i) m[i] = 0.5f * (l[i] + r[i]);
  return timbre::extract(m, rate, window);
}

struct Score {
  double profile_db = 0, centroid_oct = 0, decay_oct = 0, flatness_db = 0;
  double total() const { return profile_db / 6.0 + centroid_oct / 0.5 + decay_oct / 1.0 + flatness_db / 6.0; }
  Score& operator+=(const Score& o) { profile_db += o.profile_db; centroid_oct += o.centroid_oct; decay_oct += o.decay_oct; flatness_db += o.flatness_db; return *this; }
  Score scaled(double k) const { Score s = *this; s.profile_db *= k; s.centroid_oct *= k; s.decay_oct *= k; s.flatness_db *= k; return s; }
};

inline Score distance(const timbre::Features& a, const timbre::Features& b) {
  Score s;
  // The profiles compared after removing their mean offset (a level, not a shape).
  double mean = 0.0;
  for (int i = 0; i < timbre::kBands; ++i) mean += a.profile[static_cast<size_t>(i)] - b.profile[static_cast<size_t>(i)];
  mean /= timbre::kBands;
  double sq = 0.0;
  for (int i = 0; i < timbre::kBands; ++i) { const double d = a.profile[static_cast<size_t>(i)] - b.profile[static_cast<size_t>(i)] - mean; sq += d * d; }
  s.profile_db = std::sqrt(sq / timbre::kBands);
  s.centroid_oct = std::abs(a.centroid - b.centroid);
  s.decay_oct = std::abs(std::log2(std::max(a.decay_ms, 4.0) / std::max(b.decay_ms, 4.0)));
  s.flatness_db = std::abs(a.flatness_db - b.flatness_db);
  return s;
}

struct Result {
  int sound = -1; int recordings = 0; Score model, spread; timbre::Features features;
  double gate() const { return recordings >= 2 ? std::max(2.0 * spread.total(), 4.0) : 5.0; }
};

// Every referenced sound's score against its recordings, and the recordings' spread.
inline std::vector<Result> match(const std::vector<Recording>& refs, double rate = 48000.0) {
  std::map<int, std::vector<const Recording*>> by_sound;
  for (const Recording& r : refs) { const int s = sound_index(r.sound); if (s >= 0) by_sound[s].push_back(&r); }
  std::vector<Result> out;
  for (const auto& [sound, recs] : by_sound) {
    Result res;
    res.sound = sound;
    res.recordings = static_cast<int>(recs.size());
    res.features = play(sound, rate);
    for (const Recording* r : recs) res.model += distance(res.features, r->f);
    res.model = res.model.scaled(1.0 / static_cast<double>(recs.size()));
    int pairs = 0;
    for (size_t i = 0; i < recs.size(); ++i)
      for (size_t j = i + 1; j < recs.size(); ++j) { res.spread += distance(recs[i]->f, recs[j]->f); ++pairs; }
    if (pairs) res.spread = res.spread.scaled(1.0 / pairs);
    out.push_back(res);
  }
  return out;
}

// A small table of numbers per sound, read and written by the tool and the test.
inline std::map<std::string, double> load_table(const std::string& path) {
  std::map<std::string, double> t;
  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::stringstream s(line);
    std::string name, value;
    std::getline(s, name, '\t');
    std::getline(s, value, '\t');
    t[name] = std::stod(value);
  }
  return t;
}

}  // namespace reference
