// Renders every factory preset with the same short phrase and measures it against the quality
// bar in docs/QUALITY.md, so a preset round can be judged from audio rather than from the
// settings. Writes one stereo WAV per preset for listening and spectrograms, and prints one
// line per preset: peak, momentary loudness, limiter reduction, the first stroke's peak, DC,
// idle noise, brightness, stereo width, tail, and the master trim still wanted.
//
//   bonkr-preset-audit OUTPUT_DIR [PRESET_FILTER]
//   bonkr-preset-audit --levels
//
// `--levels` prints every sound's calibrated trim (levels:: in engine.hpp) and its peak at the
// reference stroke after it.
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "../src/engine.hpp"
#include "../src/presets.hpp"
#include "../src/wav.hpp"
#include "../tests/audio_measure.hpp"
#include "../tests/audit_phrase.hpp"

namespace {

using namespace bonkr;
using audit::kRate;

std::string slug(const char* name) {
  std::string out;
  for (const char* c = name; *c; ++c) out.push_back(std::isalnum(static_cast<unsigned char>(*c)) ? static_cast<char>(std::tolower(static_cast<unsigned char>(*c))) : '_');
  return out;
}

int print_levels() {
  const auto& trims = levels::trims(kRate);
  for (int s = 0; s < dsp::kSoundCount; ++s) {
    const double peak = levels::measure_peak(s, kRate) * trims[static_cast<size_t>(s)];
    std::printf("%-26s trim %+6.1f dB  peak after it %6.1f dBFS\n", dsp::kSoundNames[s], 20.0 * std::log10(trims[static_cast<size_t>(s)]), 20.0 * std::log10(peak));
  }
  return 0;
}

}  // namespace

// Finds each preset's master level: the target loudness, or the limiter's allowance for an
// impulsive one, whichever comes first. Prints "name<TAB>master dB".
int fit() {
  for (size_t p = 0; p < presets::count(); ++p) {
    Settings s = Settings::defaults();
    presets::apply(p, s.v.data());
    double master = s[params::kMasterDb];
    for (int round = 0; round < 16; ++round) {
      auto engine = std::make_unique<Engine>();
      engine->prepare(kRate);
      s.v[params::kMasterDb] = master;
      const audit::Rendered r = audit::render(s, *engine);
      const audit::Bar b = audit::measure_bar(s, *engine, r);
      double step = audit::kTargetLufs - b.loudness;
      if (b.limit_db > audit::kImpulsiveLimitDb + 0.5) step = std::min(step, -(b.limit_db - audit::kImpulsiveLimitDb) - 0.3);
      else if (b.limit_db > 0.0) step = std::min(step, audit::kImpulsiveLimitDb + 0.4 - b.limit_db);
      if (std::abs(step) < 0.3) break;
      master = std::clamp(master + 0.8 * step, params::kSpecs[params::kMasterDb].min, params::kSpecs[params::kMasterDb].max);
    }
    std::printf("%s\t%.1f\n", presets::all()[p].name, master);
  }
  return 0;
}

int main(int argc, char** argv) {
  if (argc >= 2 && std::strcmp(argv[1], "--levels") == 0) return print_levels();
  if (argc >= 2 && std::strcmp(argv[1], "--fit") == 0) return fit();
  if (argc < 2) { std::fprintf(stderr, "usage: bonkr-preset-audit OUTPUT_DIR [PRESET_FILTER]\n"); return 2; }
  const std::string out_dir = argv[1];
  const std::string filter = argc >= 3 ? argv[2] : "";
  std::filesystem::create_directories(out_dir);
  std::printf("%-3s %-34s %6s %6s %6s %6s %6s %6s %7s %5s %6s %5s %s\n", "#", "preset", "peak", "loud", "limit", "first", "dc", "idle", "bright", "width", "tail", "want", "flags");
  int failures = 0;
  for (size_t p = 0; p < presets::count(); ++p) {
    const char* name = presets::all()[p].name;
    if (!filter.empty() && std::string(name).find(filter) == std::string::npos) continue;
    auto engine = std::make_unique<Engine>();
    engine->prepare(kRate);
    Settings s = Settings::defaults();
    presets::apply(p, s.v.data());
    const audit::Rendered r = audit::render(s, *engine);
    const audit::Bar b = audit::measure_bar(s, *engine, r);
    if (!b.flags.empty()) ++failures;
    std::printf("%-3zu %-34s %6.1f %6.1f %6.1f %6.1f %6.1f %6.1f %7.0f %5.2f %6.1f %+5.1f%s\n", p, name, b.peak_db, b.loudness, b.limit_db, b.first_db, b.dc_db, b.idle_db, b.bright, b.width, b.tail_db,
                audit::kTargetLufs - b.loudness, b.flags.c_str());
    char file[96];
    std::snprintf(file, sizeof(file), "%02zu_%s.wav", p, slug(name).c_str());
    wav::write_file((out_dir + "/" + file).c_str(), {r.left, r.right}, kRate);
  }
  std::printf("%d preset(s) flagged\n", failures);
  return 0;
}
