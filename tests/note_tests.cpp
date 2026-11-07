// Executable scenarios for features/clap_plugin.feature (notes and automation): sound comes out
// when and only when notes say so, through both the CLAP and MIDI dialects, with sample-accurate
// event timing and automation.
#include "../src/params.hpp"
#include "audio_measure.hpp"
#include "clap_harness.hpp"
#include "spec.hpp"

#include <vector>

using namespace harness;
using measure::Span;
namespace P = bonkr::params;

namespace {
const char* g_path = nullptr;
constexpr double kRate = 48000.0;

// Renders `blocks` blocks collecting the mono output.
std::vector<float> collect(Runner& runner, int blocks, Events* events = nullptr) {
  std::vector<float> out;
  for (int b = 0; b < blocks; ++b) {
    runner.run(b == 0 ? events : nullptr);
    for (uint32_t i = 0; i < runner.frames(); ++i) out.push_back(0.5f * (runner.left()[i] + runner.right()[i]));
  }
  return out;
}
}  // namespace

SCENARIO(a_clap_note_on_strikes_the_kit_and_it_rings_out) {
  GIVEN("the default kit (studio rock), given a CLAP note on for the snare (D1, key 38)");
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  Runner runner(plugin, kRate, 256);
  Events on; on.push(note_event(CLAP_EVENT_NOTE_ON, 0, 38, 7, 0.9));
  const auto hit = collect(runner, 20, &on);
  THEN("the snare sounds at once, and dies away by itself though the key is still held");
  CHECK(measure::peak(Span(hit, 0, 2400)) > 0.05, "struck");
  const auto rest = collect(runner, 400);
  CHECK(measure::peak(Span(rest, rest.size() - 6000)) < 1e-3, "rang out with the key held");
  WHEN("the note is released by id");
  Events off; off.push(note_event(CLAP_EVENT_NOTE_OFF, 0, 38, 7, 0.0));
  collect(runner, 4, &off);
  THEN("the plug-in stays silent");
  CHECK(runner.run().peak < 1e-4f, "silent");
  runner.destroy();
}

SCENARIO(midi_notes_chokes_and_panic_are_honoured) {
  GIVEN("MIDI note on for the 18 inch crash (A1 is the crash 2, key 57) on the studio kit");
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  Runner runner(plugin, kRate, 256);
  Events on; on.push(midi_event(0x90, 57, 110));
  const auto crash = collect(runner, 40, &on);
  CHECK(measure::rms(Span(crash, crash.size() - 2400)) > 0.003, "the crash rings");
  WHEN("the drummer grabs it (polyphonic pressure on its key)");
  Events grab; grab.push(midi_event(0xa0, 57, 127));
  const auto grabbed = collect(runner, 60, &grab);
  THEN("it stops within a fraction of a second");
  CHECK(measure::peak(Span(grabbed, grabbed.size() - 4800)) < 0.05 * measure::peak(Span(crash, crash.size() - 2400)), "choked");
  WHEN("a CLAP choke event arrives for a ringing ride (key 51)");
  Events ride; ride.push(midi_event(0x90, 51, 110));
  collect(runner, 40, &ride);
  Events choke; choke.push(note_event(CLAP_EVENT_NOTE_CHOKE, 0, 51, -1, 0.0));
  const auto choked = collect(runner, 60, &choke);
  THEN("the ride stops too");
  CHECK(measure::peak(Span(choked, choked.size() - 4800)) < 2e-3, "choked by CLAP");
  WHEN("the telephone bell is held in the foley kit and all notes off (CC 123) arrives");
  set_param(plugin, P::kKit, bonkr::dsp::kits::kFoley);
  Events ring; ring.push(midi_event(0x90, 42, 100));
  const auto ringing = collect(runner, 60, &ring);
  CHECK(measure::rms(Span(ringing, ringing.size() - 4800)) > 0.003, "the telephone rings while held");
  Events panic; panic.push(midi_event(0xb0, 123, 0));
  const auto silenced = collect(runner, 400, &panic);
  THEN("it stops ringing and dies away");
  CHECK(measure::peak(Span(silenced, silenced.size() - 6000)) < 1e-3, "all notes off");
  runner.destroy();
}

SCENARIO(events_are_applied_at_their_sample_offset) {
  GIVEN("a note on scheduled 200 samples into a 256-sample block, the kick (key 36)");
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  Runner runner(plugin, kRate, 256);
  Events events;
  auto on = note_event(CLAP_EVENT_NOTE_ON, 0, 36, -1, 1.0);
  on.header.time = 200;
  events.push(on);
  std::vector<float> out;
  for (int b = 0; b < 8; ++b) {
    runner.run(b == 0 ? &events : nullptr);
    out.insert(out.end(), runner.left().begin(), runner.left().end());
  }
  THEN("the first 200 samples are exactly silent and the stroke starts right at the event");
  bool silent_before = true;
  for (uint32_t i = 0; i < 200; ++i) silent_before = silent_before && out[i] == 0.0f;
  size_t first = out.size();
  for (size_t i = 0; i < out.size(); ++i) if (std::abs(out[i]) > 1e-6f) { first = i; break; }
  std::printf("    first sound at sample %zu\n", first);
  CHECK(silent_before, "silent before the event");
  CHECK(first >= 200 && first < 200 + 8, spec::fmt("first sound at sample %.0f (the event is at 200)", static_cast<double>(first)));
  runner.destroy();
}

SCENARIO(parameter_automation_changes_the_sound_and_flush_works_offline) {
  GIVEN("the master level set by a parameter event inside process, then by flush, on a ringing tam-tam");
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  set_param(plugin, P::kLayout, P::kLayoutChromatic);
  set_param(plugin, P::kSound, bonkr::dsp::kTamTam);
  Runner runner(plugin, kRate, 256);
  Events on; on.push(note_event(CLAP_EVENT_NOTE_ON, 0, 60, -1, 1.0)); on.push(param_event(P::kMasterDb, 0.0));
  const auto loud = collect(runner, 120, &on);
  Events quieter; quieter.push(param_event(P::kMasterDb, -24.0));
  const auto quiet = collect(runner, 120, &quieter);
  THEN("the level drops by about 24 dB (less the tam-tam's own decay over the gap)");
  const double drop = measure::db(measure::rms(Span(loud, 26000, 4000))) - measure::db(measure::rms(Span(quiet, 2000, 4000)));
  CHECK(drop > 21.0 && drop < 28.0, spec::fmt("automation applied: %.1f dB", drop));
  CHECK(param(plugin, P::kMasterDb) == -24.0, "value readable through the params extension");
  WHEN("a parameter arrives through flush");
  set_param(plugin, P::kMasterDb, -6.0);
  THEN("the plug-in reports it");
  CHECK(param(plugin, P::kMasterDb) == -6.0, "flush applied");
  WHEN("an out-of-range value arrives");
  set_param(plugin, P::kMasterDb, 500.0);
  THEN("it is clamped to the declared range");
  CHECK(param(plugin, P::kMasterDb) == P::spec(P::kMasterDb).max, "clamped");
  runner.destroy();
}

SCENARIO(choosing_a_preset_rewrites_the_other_parameters) {
  GIVEN("the preset parameter set to 'In the Air Tonight toms'");
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  double index = -1.0;
  CHECK(params_of(plugin)->text_to_value(plugin, P::kPreset, "In the Air Tonight toms", &index), "preset found by name");
  set_param(plugin, P::kPreset, index);
  THEN("the kit becomes the gated 80s kit, in a room, compressed and gated");
  CHECK(param(plugin, P::kKit) == static_cast<double>(bonkr::dsp::kits::kGated80s), "kit switched");
  CHECK(param(plugin, P::kRoomSize) > 0.0 && param(plugin, P::kGate) > 0.0 && param(plugin, P::kCrush) > 0.0, "room, gate and compressor applied");
  char text[64]{};
  params_of(plugin)->value_to_text(plugin, P::kPreset, index, text, sizeof(text));
  CHECK(std::string(text) == "In the Air Tonight toms", "preset name formats back");
  WHEN("a parameter is edited afterwards and the same preset index is sent again");
  set_param(plugin, P::kMicDistance, 0.9);
  set_param(plugin, P::kPreset, index);
  THEN("the edit survives (re-sending the current preset is not a reload)");
  CHECK(param(plugin, P::kMicDistance) == 0.9, "edit kept");
  plugin->destroy(plugin);
}

SCENARIO(every_sound_sounds_through_the_plugin) {
  GIVEN("each sound chosen by parameter in chromatic play, struck once on C4");
  Library lib(g_path);
  bool all = true;
  const clap_plugin_t* plugin = lib.create();
  set_param(plugin, P::kLayout, P::kLayoutChromatic);
  Runner runner(plugin, kRate, 256);
  for (int sound = 0; sound < P::kSoundCount; ++sound) {
    set_param(plugin, P::kSound, sound);
    Events on; on.push(note_event(CLAP_EVENT_NOTE_ON, 0, 60, -1, 0.8));
    // The jet takes seconds to arrive; everything else speaks within a quarter second.
    const auto out = collect(runner, sound == bonkr::dsp::kJetFlyby ? 600 : 60, &on);
    Events off; off.push(note_event(CLAP_EVENT_NOTE_OFF, 0, 60, -1, 0.0)); off.push(midi_event(0xb0, 120, 0));
    collect(runner, 1, &off);
    bool finite = true;
    for (float v : out) finite = finite && std::isfinite(v);
    const double peak = measure::peak(Span(out));
    if (!finite || peak < 0.02) { all = false; std::printf("    sound %d (%s): peak %.5f finite %d\n", sound, bonkr::dsp::kSoundNames[sound], peak, finite); }
  }
  runner.destroy();
  THEN("every one is audible and finite");
  CHECK(all, "every sound sounds");
}

SCENARIO(tail_reports_longer_in_a_church) {
  GIVEN("the tail extension on the kit alone, then in a church with the microphones 5 m away");
  Library lib(g_path);
  const clap_plugin_t* plugin = lib.create();
  const auto* tail = static_cast<const clap_plugin_tail_t*>(plugin->get_extension(plugin, CLAP_EXT_TAIL));
  Runner runner(plugin, kRate, 256);
  runner.run();
  const uint32_t dry = tail->get(plugin);
  set_param(plugin, P::kRoomSize, 6); set_param(plugin, P::kMicDistance, 5.0);
  runner.run();
  const uint32_t wet = tail->get(plugin);
  THEN("the wet tail is longer and both are sane");
  CHECK(dry > static_cast<uint32_t>(6.0 * kRate) && dry < 48000 * 20, spec::fmt("dry tail %.2f s covers the longest sound", dry / kRate));
  CHECK(wet > dry, spec::fmt("wet tail %.1f s exceeds dry %.1f s", wet / kRate, dry / kRate));
  runner.destroy();
}

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: note_tests <BONKR.clap> [filter]\n"); return 2; }
  g_path = argv[1];
  return spec::run(argc, argv, 2);
}
