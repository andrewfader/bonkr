// Plays the rock groove of tests/stem_groove.hpp through the built plug-in, as a host would, and
// writes it to a 44.1 kHz WAV to be measured as a drum stem against records
// (song-corpus/scripts/features.py); prints the same features as measured here. A review tool.
//
//   bonkr-groove-render <BONKR.clap> <out.wav> preset=<name> [Parameter_name=value ...]
//
// Values are what the parameter's text field accepts ("Studio", "75%", "-3 dB", "0.4").

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../src/wav.hpp"
#include "clap_harness.hpp"
#include "stem_features.hpp"
#include "stem_groove.hpp"

using namespace harness;

int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: bonkr-groove-render <BONKR.clap> <out.wav> preset=<name> [Parameter=value ...]\n"); return 2; }
  Library library(argv[1]);
  const clap_plugin_t* plugin = library.create();
  const auto* params = params_of(plugin);
  for (int i = 3; i < argc; ++i) {
    const char* eq = std::strchr(argv[i], '=');
    if (!eq) { std::fprintf(stderr, "bad argument %s\n", argv[i]); return 2; }
    std::string name(argv[i], static_cast<size_t>(eq - argv[i]));
    for (char& c : name) if (c == '_') c = ' ';
    if (name == "preset") name = "Preset";
    clap_id id = CLAP_INVALID_ID;
    for (uint32_t k = 0; k < params->count(plugin); ++k) {
      clap_param_info_t info{};
      params->get_info(plugin, k, &info);
      std::string have(info.name);
      if (std::equal(have.begin(), have.end(), name.begin(), name.end(), [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); })) id = info.id;
    }
    double value = 0.0;
    if (id == CLAP_INVALID_ID || !params->text_to_value(plugin, id, eq + 1, &value)) {
      std::fprintf(stderr, "cannot set %s to %s\n", name.c_str(), eq + 1);
      return 2;
    }
    set_param(plugin, id, value);
  }
  Runner runner(plugin, stem::kRate, stem::kBlock);
  runner.tempo = stem::kBpm;
  struct Event { size_t at; int key; bool on; double velocity; };
  std::vector<Event> events;
  for (const auto& n : stem::groove()) {
    events.push_back({static_cast<size_t>(n.at_s * stem::kRate), n.key, true, n.velocity});
    events.push_back({static_cast<size_t>((n.at_s + n.length_s) * stem::kRate), n.key, false, 0.0});
  }
  std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b) { return a.at < b.at; });
  const size_t frames = static_cast<size_t>(stem::seconds() * stem::kRate);
  std::vector<float> left, right;
  size_t next = 0;
  Events queue;
  for (size_t done = 0; done < frames; done += stem::kBlock) {
    queue.clear();
    while (next < events.size() && events[next].at < done + stem::kBlock) {
      const Event& e = events[next++];
      clap_event_note_t note = note_event(e.on ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF, 0, static_cast<int16_t>(e.key), -1, e.velocity);
      note.header.time = static_cast<uint32_t>(e.at - done);
      queue.push(note);
    }
    runner.run(&queue);
    left.insert(left.end(), runner.left().begin(), runner.left().end());
    right.insert(right.end(), runner.right().begin(), runner.right().end());
  }
  runner.destroy();
  left.resize(frames); right.resize(frames);
  bonkr::wav::write_file(argv[2], {left, right}, stem::kRate);
  const stem::Features f = stem::measure(left, right, stem::kRate);
  std::printf("rolloff85 %.0f Hz  low share %.2f dB  centroid %.0f Hz  crest %.2f dB  L/R corr %.3f\n",
              f.rolloff85_hz, f.low_share_db, f.centroid_hz, f.crest100_db, f.corr);
  return 0;
}
