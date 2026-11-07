// BONKR against recordings of the real instruments (references/README.md). A review tool;
// reference_tests holds the gate and the scores.
//
//   bonkr-reference extract                 the recordings' features -> references/features.tsv
//   bonkr-reference match                   every sound's score, its recordings' spread and its gate
//   bonkr-reference match --features        and BONKR's features beside the recordings'
//   bonkr-reference match Snare             only the sounds whose names contain "Snare"
//   bonkr-reference match --update-baseline write the scores to references/baseline.tsv
//
// The recordings are read from ~/.cache/bonkr-references/wav (tools/references/fetch.py puts
// them there), or $BONKR_REFERENCES/wav.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "../src/wav.hpp"
#include "../tests/reference_match.hpp"

namespace {

std::string cache_dir() {
  if (const char* env = std::getenv("BONKR_REFERENCES")) return env;
  const char* home = std::getenv("HOME");
  return std::string(home ? home : ".") + "/.cache/bonkr-references";
}

// The same key fetch.py names a recording's files by: the first 16 hex digits of the SHA-1 of
// its URL. Computed here so the tool needs nothing but the manifest.
std::string sha1_16(const std::string& text) {
  uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
  std::vector<uint8_t> m(text.begin(), text.end());
  const uint64_t bits = static_cast<uint64_t>(m.size()) * 8u;
  m.push_back(0x80);
  while (m.size() % 64 != 56) m.push_back(0);
  for (int i = 7; i >= 0; --i) m.push_back(static_cast<uint8_t>(bits >> (i * 8)));
  const auto rol = [](uint32_t x, int n) { return (x << n) | (x >> (32 - n)); };
  for (size_t chunk = 0; chunk < m.size(); chunk += 64) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i) w[i] = (static_cast<uint32_t>(m[chunk + 4 * i]) << 24) | (static_cast<uint32_t>(m[chunk + 4 * i + 1]) << 16) | (static_cast<uint32_t>(m[chunk + 4 * i + 2]) << 8) | m[chunk + 4 * i + 3];
    for (int i = 16; i < 80; ++i) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; ++i) {
      uint32_t f, k;
      if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999u; }
      else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1u; }
      else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
      else { f = b ^ c ^ d; k = 0xCA62C1D6u; }
      const uint32_t t = rol(a, 5) + f + e + k + w[i];
      e = d; d = c; c = rol(b, 30); b = a; a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
  }
  char out[41];
  for (int i = 0; i < 5; ++i) std::snprintf(out + 8 * i, 9, "%08x", h[i]);
  return std::string(out, 16);
}

int extract() {
  std::ifstream manifest("references/manifest.tsv");
  if (!manifest) { std::fprintf(stderr, "run from the repository root\n"); return 2; }
  std::FILE* out = std::fopen("references/features.tsv", "w");
  std::fprintf(out, "# Features (tests/timbre.hpp) of every reference recording, by bonkr-reference extract.\n");
  std::fprintf(out, "# sound\tid\tcentroid_log2\tdecay_ms\tattack_ms\tflatness_db\tprofile 31.5 Hz .. 16 kHz (dB)\n");
  std::string line;
  int done = 0, missing = 0;
  while (std::getline(manifest, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::stringstream s(line);
    std::string sound, url;
    std::getline(s, sound, '\t');
    std::getline(s, url, '\t');
    const std::string id = sha1_16(url);
    const std::string path = cache_dir() + "/wav/" + id + ".wav";
    bonkr::wav::Audio audio;
    std::string error;
    if (!bonkr::wav::read_file(path.c_str(), audio, &error)) { std::fprintf(stderr, "missing %s (%s): %s\n", sound.c_str(), url.c_str(), error.c_str()); ++missing; continue; }
    const int index = reference::sound_index(sound);
    if (index < 0) { std::fprintf(stderr, "unknown sound %s\n", sound.c_str()); continue; }
    reference::Recording r;
    r.sound = sound; r.id = id;
    r.f = timbre::extract(audio.mono, audio.rate, reference::window_s(index));
    reference::write_features(out, r);
    ++done;
  }
  std::fclose(out);
  std::printf("%d recordings measured, %d missing\n", done, missing);
  return 0;
}

int match(bool features, bool update, const std::string& only) {
  auto refs = reference::load_features("references/features.tsv");
  if (!only.empty()) refs.erase(std::remove_if(refs.begin(), refs.end(), [&](const reference::Recording& r) { return r.sound.find(only) == std::string::npos; }), refs.end());
  if (refs.empty()) { std::fprintf(stderr, "no references/features.tsv: run extract first\n"); return 2; }
  const auto results = reference::match(refs);
  const auto baseline = reference::load_table("references/baseline.tsv");
  std::printf("%-22s %4s %7s %7s %7s %7s %7s  %7s %7s %6s %s\n", "sound", "recs", "profile", "bright", "decay", "flat", "SCORE", "spread", "gate", "base", "");
  int failing = 0;
  for (const auto& r : results) {
    const std::string id = reference::sound_id(r.sound);
    const double total = r.model.total();
    const auto b = baseline.find(id);
    const bool over_gate = total > r.gate();
    const bool worse = b != baseline.end() && total > b->second + 0.05;
    failing += over_gate || worse;
    std::printf("%-22s %4d %7.1f %7.2f %7.2f %7.1f %7.2f  %7.2f %7.2f %6.2f %s%s\n", id.c_str(), r.recordings, r.model.profile_db, r.model.centroid_oct, r.model.decay_oct, r.model.flatness_db, total,
                r.spread.total(), r.gate(), b == baseline.end() ? 0.0 : b->second, over_gate ? "OVER GATE " : "", worse ? "WORSE" : "");
    if (features) {
      const auto show = [](const char* who, const timbre::Features& f) {
        std::printf("    %-8s %6.0f Hz %6.0f ms %5.1f ms %6.1f dB |", who, std::exp2(f.centroid), f.decay_ms, f.attack_ms, f.flatness_db);
        for (double p : f.profile) std::printf(" %5.1f", p);
        std::printf("\n");
      };
      show("BONKR", r.features);
      for (const auto& ref : refs) if (ref.sound == id) show(ref.id.substr(0, 8).c_str(), ref.f);
    }
  }
  std::printf("%d sound(s) over the gate or behind the baseline\n", failing);
  if (update) {
    std::FILE* out = std::fopen("references/baseline.tsv", "w");
    std::fprintf(out, "# BONKR's score against the reference recordings (tests/reference_match.hpp), by sound; reference_tests\n# fails if any gets worse. Written by bonkr-reference match --update-baseline.\n");
    for (const auto& r : results) std::fprintf(out, "%s\t%.3f\n", reference::sound_id(r.sound).c_str(), r.model.total());
    std::fclose(out);
    std::printf("baseline written\n");
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 2 && std::strcmp(argv[1], "extract") == 0) return extract();
  if (argc >= 2 && std::strcmp(argv[1], "match") == 0) {
    bool features = false, update = false;
    std::string only;
    for (int i = 2; i < argc; ++i) {
      if (!std::strcmp(argv[i], "--features")) features = true;
      else if (!std::strcmp(argv[i], "--update-baseline")) update = true;
      else only = argv[i];
    }
    if (update && !only.empty()) { std::fprintf(stderr, "update the baseline from a full match\n"); return 2; }
    return match(features, update, only);
  }
  std::fprintf(stderr, "usage: bonkr-reference extract | match [--features] [--update-baseline]\n");
  return 2;
}
