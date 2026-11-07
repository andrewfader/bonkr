// Renders strokes straight from the engine, for listening and for comparing against recordings.
// A review tool, not a test.
//
//   bonkr-note-render <out.wav> sound=<index or part of a name> [velocity=0.8 key=60 hold=0.3 tail=2]
//                     [any parameter by name, spaces as underscores: decay=1.5 room=Studio converter=...]
//   bonkr-note-render <out.wav> kit=<index or name> key=38 ...      one key of a kit
//   bonkr-note-render <dir> all [velocity=0.8 tail=2]                every sound, one file each
//
// Prints the sound, its peak and how long it rings.

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../src/engine.hpp"
#include "../src/wav.hpp"

namespace {

std::string lower(std::string s) {
  for (char& c : s) c = c == ' ' ? '_' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

int find_option(const char* const* names, int count, const std::string& want) {
  char* end = nullptr;
  const long n = std::strtol(want.c_str(), &end, 10);
  if (end && *end == '\0') return static_cast<int>(n);
  const std::string w = lower(want);
  for (int i = 0; i < count; ++i) if (lower(names[i]) == w) return i;
  for (int i = 0; i < count; ++i) if (lower(names[i]).find(w) != std::string::npos) return i;
  return -1;
}

struct Render { std::vector<float> l, r; };

Render render(bonkr::Settings s, int key, double velocity, double hold, double tail, double rate) {
  auto engine = std::make_unique<bonkr::Engine>();
  engine->prepare(rate);
  Render out;
  const size_t total = static_cast<size_t>((hold + tail) * rate);
  out.l.assign(total, 0.0f); out.r.assign(total, 0.0f);
  engine->note_on(s, 0, key, -1, velocity);
  const size_t off = static_cast<size_t>(hold * rate);
  constexpr size_t kBlock = 256;
  for (size_t at = 0; at < total; at += kBlock) {
    if (at <= off && off < at + kBlock) {
      const size_t first = off - at;
      if (first) engine->render(s, out.l.data() + at, out.r.data() + at, static_cast<uint32_t>(first));
      engine->note_off(s, 0, key, -1);
      const size_t n = std::min(kBlock, total - at) - first;
      engine->render(s, out.l.data() + at + first, out.r.data() + at + first, static_cast<uint32_t>(n));
      continue;
    }
    engine->render(s, out.l.data() + at, out.r.data() + at, static_cast<uint32_t>(std::min(kBlock, total - at)));
  }
  return out;
}

void report(const char* name, const Render& r, double rate) {
  double peak = 0.0;
  size_t peak_at = 0;
  for (size_t i = 0; i < r.l.size(); ++i) { const double a = std::max(std::abs(r.l[i]), std::abs(r.r[i])); if (a > peak) { peak = a; peak_at = i; } }
  // Ring time: the last sample within 40 dB of the peak.
  size_t last = 0;
  for (size_t i = 0; i < r.l.size(); ++i) if (std::max(std::abs(r.l[i]), std::abs(r.r[i])) > peak * 0.01) last = i;
  std::printf("%-28s peak %6.1f dBFS at %5.0f ms, rings %6.0f ms\n", name, 20.0 * std::log10(std::max(peak, 1e-9)), static_cast<double>(peak_at) / rate * 1000.0, static_cast<double>(last) / rate * 1000.0);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: bonkr-note-render <out.wav|dir> sound=... | kit=... key=... | all\n"); return 2; }
  std::map<std::string, std::string> a{{"key", "60"}, {"velocity", "0.8"}, {"hold", "0.3"}, {"tail", "2.0"}, {"rate", "48000"}};
  bonkr::Settings s = bonkr::Settings::defaults();
  s.v[bonkr::params::kLayout] = bonkr::params::kLayoutChromatic;
  bool all = false;
  for (int i = 2; i < argc; ++i) {
    if (std::strcmp(argv[i], "all") == 0) { all = true; continue; }
    const char* eq = std::strchr(argv[i], '=');
    if (!eq) { std::fprintf(stderr, "bad argument %s\n", argv[i]); return 2; }
    const std::string key(argv[i], static_cast<size_t>(eq - argv[i]));
    const std::string value(eq + 1);
    if (a.count(key)) { a[key] = value; continue; }
    if (key == "sound") {
      const int sound = find_option(bonkr::dsp::kSoundNames, bonkr::dsp::kSoundCount, value);
      if (sound < 0) { std::fprintf(stderr, "no sound %s\n", value.c_str()); return 2; }
      s.v[bonkr::params::kSound] = sound;
      continue;
    }
    if (key == "kit") {
      const int kit = find_option(bonkr::dsp::kits::kKitNames, bonkr::dsp::kits::kKitCount, value);
      if (kit < 0) { std::fprintf(stderr, "no kit %s\n", value.c_str()); return 2; }
      s.v[bonkr::params::kKit] = kit;
      s.v[bonkr::params::kLayout] = bonkr::params::kLayoutKit;
      continue;
    }
    bool found = false;
    for (uint32_t id = 0; id < bonkr::params::kParamCount; ++id) {
      if (lower(bonkr::params::kSpecs[id].name) != lower(key)) continue;
      double v = 0.0;
      if (!bonkr::params::parse_value(id, value.c_str(), &v)) { std::fprintf(stderr, "bad value %s\n", value.c_str()); return 2; }
      s.v[id] = v;
      found = true;
    }
    if (!found) { std::fprintf(stderr, "unknown key %s\n", key.c_str()); return 2; }
  }
  const double rate = std::atof(a["rate"].c_str());
  const double velocity = std::atof(a["velocity"].c_str()), hold = std::atof(a["hold"].c_str()), tail = std::atof(a["tail"].c_str());
  if (all) {
    std::filesystem::create_directories(argv[1]);
    for (int sound = 0; sound < bonkr::dsp::kSoundCount; ++sound) {
      s.v[bonkr::params::kSound] = sound;
      const Render r = render(s, bonkr::params::kChromaticRoot, velocity, hold, tail, rate);
      char path[512];
      std::snprintf(path, sizeof(path), "%s/%03d.wav", argv[1], sound);
      bonkr::wav::write_file(path, {r.l, r.r}, rate);
      report(bonkr::dsp::kSoundNames[sound], r, rate);
    }
    return 0;
  }
  const int key = std::atoi(a["key"].c_str());
  const Render r = render(s, key, velocity, hold, tail, rate);
  bonkr::wav::write_file(argv[1], {r.l, r.r}, rate);
  const auto t = bonkr::Engine::target(s, key);
  report(t.sound >= 0 ? bonkr::dsp::kSoundNames[t.sound] : "(silent key)", r, rate);
  return 0;
}
