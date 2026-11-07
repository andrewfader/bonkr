// Executable scenarios for features/kit.feature: the kits on the General MIDI map, chromatic
// play, the hi-hat's choke, restriking a ringing drum, MPC swing and note repeat, the stereo
// picture.
#include <cmath>
#include <vector>

#include "../src/dsp/kits.hpp"
#include "../src/engine.hpp"
#include "audio_measure.hpp"
#include "rig.hpp"
#include "spec.hpp"

using namespace bonkr;
using measure::Span;
constexpr double kRate = rig::kRate;

SCENARIO(every_kit_puts_its_drums_where_general_midi_puts_them) {
  GIVEN("every drum-set kit's map");
  const auto& maps = dsp::kits::all();
  const auto role = [](int s) { return dsp::kSoundNames[s]; };
  THEN("C1 (36) is a kick, D1 (38) a snare, F#1 (42) a closed hat and A#1 (46) an open one, in the acoustic and sampled sets");
  for (int k : {dsp::kits::kStudioRock, dsp::kits::kFunk, dsp::kits::kBigRoom, dsp::kits::kGated80s, dsp::kits::kLinn, dsp::kits::kDmx, dsp::kits::kSp1200, dsp::kits::kMpc60, dsp::kits::kHr16}) {
    const auto& m = maps[static_cast<size_t>(k)];
    CHECK(std::string(role(m[36].sound)).find("Kick") == 0, std::string(dsp::kits::kKitNames[k]) + " kick");
    CHECK(std::string(role(m[38].sound)).find("Snare") == 0, std::string(dsp::kits::kKitNames[k]) + " snare");
    CHECK(m[42].sound == dsp::kHatClosed && m[46].sound == dsp::kHatOpen, std::string(dsp::kits::kKitNames[k]) + " hats");
  }
  THEN("the machines' kits use their own circuits on the same keys");
  CHECK(maps[dsp::kits::kKit808][36].sound == dsp::kTR808Kick && maps[dsp::kits::kKit808][39].sound == dsp::kTR808Clap && maps[dsp::kits::kKit808][56].sound == dsp::kTR808Cowbell, "808");
  CHECK(maps[dsp::kits::kKit909][36].sound == dsp::kTR909Kick && maps[dsp::kits::kKit909][42].sound == dsp::kTR909HatClosed, "909");
  THEN("and every kit strikes something on the kick and snare keys");
  for (int k = 0; k < dsp::kits::kKitCount; ++k) CHECK(maps[static_cast<size_t>(k)][36].sound >= 0 && maps[static_cast<size_t>(k)][38].sound >= 0, dsp::kits::kKitNames[k]);
}

SCENARIO(chromatic_play_tunes_by_the_key) {
  GIVEN("the timpani across the keyboard");
  auto a = rig::Engine::sound(dsp::kTimpani), b = rig::Engine::sound(dsp::kTimpani);
  const auto c4 = a.stroke(60, 0.6, 1.0, 0.0), g4 = b.stroke(67, 0.6, 1.0, 0.0);
  THEN("C4 sounds its own D3 and G4 a fifth higher, 700 cents");
  const double f1 = measure::peak_hz_near(Span(c4, 9600, 28800), kRate, 146.8, 60.0);
  const double f2 = measure::peak_hz_near(Span(g4, 9600, 28800), kRate, 146.8 * 1.4983, 60.0);
  CHECK_NEAR(measure::cents(f2, f1), 700.0, 8.0, "a fifth");
  WHEN("Tune is -12 semitones");
  auto c = rig::Engine::sound(dsp::kTimpani);
  c.s.v[params::kTune] = -12.0;
  const auto low = c.stroke(60, 0.6, 1.0, 0.0);
  THEN("it sounds an octave down");
  CHECK_NEAR(measure::cents(measure::peak_hz_near(Span(low, 9600, 28800), kRate, 73.4, 60.0), f1), -1200.0, 8.0, "an octave");
}

SCENARIO(closing_the_hi_hat_stops_the_open_one) {
  GIVEN("the open hi-hat struck, then the pedal closed 150 ms later");
  rig::Engine choked, ringing;
  ringing.s.v[params::kChoke] = 0.0;
  for (auto* e : {&choked, &ringing}) { e->on(46, 0.8); e->render(0.15); e->on(44, 0.1); e->render(0.6); }
  const auto a = choked.mono(), b = ringing.mono();
  THEN("with the choke on, the open hat has stopped by 300 ms; with it off, it still rings");
  const Span late_a(a, static_cast<size_t>(0.35 * kRate), 4800), late_b(b, static_cast<size_t>(0.35 * kRate), 4800);
  CHECK(measure::db(measure::rms(late_a)) < measure::db(measure::rms(late_b)) - 20.0, spec::fmt("choked %.1f dB, ringing %.1f dB", measure::db(measure::rms(late_a)), measure::db(measure::rms(late_b))));
}

SCENARIO(a_ringing_drum_struck_again_is_the_same_drum) {
  GIVEN("the ride struck twelve times in a row");
  rig::Engine e;
  for (int i = 0; i < 12; ++i) { e.on(51, 0.7); e.render(0.15); }
  THEN("one voice carries it, its ring building on itself as a real ride's does");
  CHECK(e.engine->active_voices() == 1, spec::fmt("%.0f voices", e.engine->active_voices()));
  GIVEN("twelve different kit keys struck together");
  rig::Engine f;
  for (int k = 35; k < 47; ++k) f.on(k, 0.7);
  f.render(0.01);
  THEN("each has its own voice");
  CHECK(f.engine->active_voices() >= 12, "twelve voices");
}

SCENARIO(swing_delays_the_second_sixteenth_of_each_eighth) {
  GIVEN("the transport playing at 120 bpm with 66% swing, a closed hat on each sixteenth of a beat");
  const double sixteenth = 60.0 / 120.0 / 4.0;
  std::vector<double> onsets;
  for (int step = 0; step < 4; ++step) {
    rig::Engine e;
    e.s.v[params::kSwing] = 0.66;
    e.engine->set_tempo(120.0);
    e.engine->set_transport(true, step * 0.25);
    e.on(42, 0.8);
    e.render(sixteenth);
    size_t first = 0;
    while (first < e.left.size() && std::abs(e.left[first]) < 1e-5f) ++first;
    onsets.push_back(static_cast<double>(first) / kRate);
  }
  THEN("the off-beat sixteenths sound (0.66 - 0.5) of an eighth late, 40 ms; the on-beat ones at once");
  CHECK(onsets[0] < 0.001 && onsets[2] < 0.001, "on the beat");
  CHECK_NEAR(onsets[1], 0.16 * 0.25, 0.001, "the second sixteenth");
  CHECK_NEAR(onsets[3], 0.16 * 0.25, 0.001, "the fourth sixteenth");
  GIVEN("the transport stopped");
  rig::Engine still;
  still.s.v[params::kSwing] = 0.66;
  still.engine->set_transport(false, 0.25);
  still.on(42, 0.8);
  still.render(0.01);
  THEN("nothing is delayed");
  CHECK(measure::peak(Span(still.left, 0, 48)) > 1e-4, "at once");
}

SCENARIO(note_repeat_strikes_while_held) {
  GIVEN("note repeat at 1/16 and 120 bpm, the snare held for one second");
  rig::Engine e;
  e.s.v[params::kNoteRepeat] = 4;
  e.engine->set_tempo(120.0);
  const auto x = e.stroke(38, 0.8, 1.0, 0.3);
  THEN("it strikes every 125 ms while held: eight strokes, then no more");
  const auto c = measure::contour_db(x, kRate, 0.001, 0.002);
  int strokes = 0;
  for (size_t i = 1; i + 1 < c.size(); ++i)
    if (c[i] > c[i - 1] + 12.0) { ++strokes; i += 30; }
  CHECK(strokes == 8, spec::fmt("%.0f strokes", strokes));
}

SCENARIO(the_kit_is_spread_as_the_audience_sees_it) {
  GIVEN("the hi-hat and the floor tom, at full width");
  rig::Engine hat, tom;
  for (auto* e : {&hat, &tom}) e->s.v[params::kStereoWidth] = 1.0;
  hat.stroke(42, 0.8, 0.1, 0.1);
  tom.stroke(41, 0.8, 0.2, 0.1);
  THEN("the hat sits right and the floor tom left (the drummer faces the audience)");
  CHECK(measure::rms(Span(hat.right)) > 1.5 * measure::rms(Span(hat.left)), "hat right");
  CHECK(measure::rms(Span(tom.left)) > 1.5 * measure::rms(Span(tom.right)), "floor tom left");
  WHEN("the width is none");
  rig::Engine mono;
  mono.s.v[params::kStereoWidth] = 0.0;
  mono.stroke(42, 0.8, 0.1, 0.1);
  THEN("the hat is in the middle");
  CHECK_NEAR(measure::db(measure::rms(Span(mono.left))) - measure::db(measure::rms(Span(mono.right))), 0.0, 0.1, "centred");
}

int main(int argc, char** argv) { return spec::run(argc, argv); }
