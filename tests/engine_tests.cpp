// Executable scenarios for features/engine.feature: strokes, held sounds, every sound at a
// comparable level, every preset against the quality bar, the limiter and the tail, through the
// engine's own interface (no CLAP yet).
#include <memory>
#include <vector>

#include "../src/engine.hpp"
#include "../src/presets.hpp"
#include "audio_measure.hpp"
#include "audit_phrase.hpp"
#include "rig.hpp"
#include "spec.hpp"

using namespace bonkr;
using measure::Span;
constexpr double kRate = rig::kRate;

SCENARIO(a_stroke_rings_out_by_itself_and_a_held_sound_stops_on_release) {
  GIVEN("the snare on the studio kit, struck and held for a second");
  rig::Engine e;
  const auto x = e.stroke(38, 0.8, 1.0, 0.5);
  THEN("it sounds at once and has died away before the key is let go");
  CHECK(rig::finite(x), "finite");
  CHECK(measure::peak(Span(x, 0, 2400)) > 0.1, "struck");
  CHECK(measure::peak(Span(x, static_cast<size_t>(0.9 * kRate), 4800)) < 1e-3, "rang out with the key held");
  GIVEN("the vuvuzela, blown for half a second and let go");
  auto v = rig::Engine::sound(dsp::kVuvuzela);
  const auto y = v.stroke(60, 0.8, 0.5, 1.0);
  THEN("it sounds while held and stops after the release");
  CHECK(measure::rms(Span(y, static_cast<size_t>(0.3 * kRate), 4800)) > 0.02, "blowing");
  CHECK(measure::peak(Span(y, static_cast<size_t>(1.2 * kRate))) < 1e-3, "stopped");
  CHECK(v.engine->silent(), "the engine reports silence");
}

SCENARIO(velocity_reaches_the_stroke) {
  GIVEN("the floor tom struck softly and hard");
  auto soft = rig::Engine::kit(dsp::kits::kStudioRock), hard = rig::Engine::kit(dsp::kits::kStudioRock);
  const auto a = soft.stroke(43, 0.25, 0.3, 0.1), b = hard.stroke(43, 1.0, 0.3, 0.1);
  THEN("the hard stroke is more than 20 dB louder and brighter");
  CHECK(measure::db(measure::peak(Span(b))) - measure::db(measure::peak(Span(a))) > 20.0, "louder");
  CHECK(measure::spectral_centroid(Span(b, 0, 4800), kRate) > measure::spectral_centroid(Span(a, 0, 4800), kRate), "brighter");
  WHEN("dynamics is off");
  auto soft2 = rig::Engine::kit(dsp::kits::kStudioRock), hard2 = rig::Engine::kit(dsp::kits::kStudioRock);
  soft2.s.v[params::kDynamics] = 0.0; hard2.s.v[params::kDynamics] = 0.0;
  const auto c = soft2.stroke(43, 0.25, 0.3, 0.1), d = hard2.stroke(43, 1.0, 0.3, 0.1);
  THEN("both strokes are the same");
  CHECK_NEAR(measure::db(measure::rms(Span(d, 0, 9600))) - measure::db(measure::rms(Span(c, 0, 9600))), 0.0, 1.0, "the same stroke");
}

SCENARIO(every_sound_is_calibrated_to_the_same_peak) {
  GIVEN("every sound's trim, measured from the model at this sample rate");
  const auto& trims = levels::trims(kRate);
  THEN("a reference stroke through its trim peaks at -9 dBFS");
  for (int s = 0; s < dsp::kSoundCount; ++s) {
    const double peak = levels::measure_peak(s, kRate) * trims[static_cast<size_t>(s)];
    CHECK_NEAR(measure::db(peak), -9.0, 0.5, dsp::kSoundNames[s]);
  }
  THEN("the trims are measured afresh at another sample rate, and agree within a few dB (more for the random ones)");
  const auto& other = levels::trims(44100.0);
  for (int s = 0; s < dsp::kSoundCount; ++s) {
    const bool random = dsp::family(s) == dsp::Family::Particles || s == dsp::kGlassSmash || s == dsp::kThunder || s == dsp::kFireworkCrackle || s == dsp::kGroupClap;
    CHECK(std::abs(measure::db(other[static_cast<size_t>(s)]) - measure::db(trims[static_cast<size_t>(s)])) < (random ? 8.0 : 4.0), dsp::kSoundNames[s]);
  }
}

SCENARIO(every_preset_meets_the_quality_bar) {
  GIVEN("every factory preset, playing the audit phrase (docs/QUALITY.md)");
  for (size_t p = 0; p < presets::count(); ++p) {
    auto engine = std::make_unique<Engine>();
    engine->prepare(kRate);
    Settings s = Settings::defaults();
    presets::apply(p, s.v.data());
    const auto r = audit::render(s, *engine);
    const auto b = audit::measure_bar(s, *engine, r);
    CHECK(b.flags.empty(), std::string(presets::all()[p].name) + ":" + b.flags + spec::fmt(" (loudness %.1f LUFS, limiter %.1f dB)", b.loudness, b.limit_db));
  }
}

SCENARIO(the_output_limiter_holds_the_ceiling_and_leaves_quiet_signals_alone) {
  GIVEN("the whole studio kit struck at once, as hard as possible, with the master at +12 dB");
  rig::Engine e;
  e.s.v[params::kMasterDb] = 12.0;
  for (int k = 35; k <= 59; ++k) e.on(k, 1.0);
  e.render(1.0);
  THEN("nothing leaves above the -2 dBFS ceiling and the engine flags the squash");
  CHECK(std::max(measure::peak(Span(e.left)), measure::peak(Span(e.right))) <= Limiter::kCeiling + 1e-6, "ceiling");
  CHECK(e.engine->clipped(), "flagged");
  GIVEN("one quiet stroke with the master at -24 dB");
  rig::Engine q;
  q.s.v[params::kMasterDb] = -24.0;
  q.stroke(38, 0.5, 0.2, 0.2);
  THEN("the limiter never acts");
  CHECK(q.engine->limited_db() == 0.0 && !q.engine->clipped(), "untouched");
}

SCENARIO(the_advertised_tail_covers_the_decay) {
  GIVEN("the tam-tam struck in a church, the microphones far away");
  auto e = rig::Engine::sound(dsp::kTamTam);
  e.s.v[params::kRoomSize] = 6; e.s.v[params::kMicDistance] = 8.0;
  const double tail = e.engine->tail_samples(e.s) / kRate;
  e.on(60, 1.0); e.render(0.05); e.off(60);
  e.render(tail);
  const auto m = e.mono();
  THEN("by the end of the advertised tail it is 50 dB down");
  CHECK(measure::db(measure::peak(Span(m, m.size() - 4800))) < measure::db(measure::peak(Span(m))) - 50.0, spec::fmt("tail %.1f s", tail));
}

SCENARIO(the_converter_holds_and_quantises_at_the_machines_clock) {
  GIVEN("the SP-1200 converter on a tom");
  rig::Engine clean, sp;
  sp.s.v[params::kConverter] = dsp::kits::kConvSp1200;
  clean.s.v[params::kConverter] = dsp::kits::kConvClean;
  const auto a = clean.stroke(45, 0.8, 0.3, 0.1), b = sp.stroke(45, 0.8, 0.3, 0.1);
  THEN("over the loud first 20 ms its output moves in steps held for 48000 / 26040 samples, and the clean one does not");
  int changes_b = 0, changes_a = 0;
  const size_t n = static_cast<size_t>(0.02 * kRate);
  for (size_t i = 1; i < n; ++i) { changes_b += sp.left[i] != sp.left[i - 1]; changes_a += clean.left[i] != clean.left[i - 1]; }
  CHECK_NEAR(static_cast<double>(changes_b) / (0.02 * 26040.0), 1.0, 0.05, "steps per clock tick");
  CHECK(changes_a > static_cast<int>(0.9 * static_cast<double>(n)), "clean changes every sample");
  (void)a; (void)b;
  GIVEN("the LinnDrum kit on its own converter, and on the LinnDrum converter chosen by hand");
  auto own = rig::Engine::kit(dsp::kits::kLinn), chosen = rig::Engine::kit(dsp::kits::kLinn);
  chosen.s.v[params::kConverter] = dsp::kits::kConvLinn;
  own.stroke(38, 0.8, 0.2, 0.1);
  chosen.stroke(38, 0.8, 0.2, 0.1);
  THEN("they are the same: the kit's own converter is the LinnDrum's");
  CHECK(own.left == chosen.left, "identical output");
}

SCENARIO(drive_and_the_compressor_raise_the_tail_against_the_hit) {
  GIVEN("the floor tom in a club, plain and through the listen-mic compressor");
  auto plain = rig::Engine::kit(dsp::kits::kStudioRock), crushed = rig::Engine::kit(dsp::kits::kStudioRock);
  for (auto* e : {&plain, &crushed}) { e->s.v[params::kRoomSize] = 4; e->s.v[params::kMicDistance] = 2.0; }
  crushed.s.v[params::kCrush] = 0.8;
  const auto a = plain.stroke(43, 0.9, 0.6, 0.1), b = crushed.stroke(43, 0.9, 0.6, 0.1);
  const auto ratio = [&](const std::vector<float>& x) { return measure::db(measure::rms(Span(x, static_cast<size_t>(0.3 * kRate), 4800))) - measure::db(measure::peak(Span(x, 0, 4800))); };
  THEN("the compressed tail sits at least 10 dB closer to the hit");
  CHECK(ratio(b) > ratio(a) + 10.0, spec::fmt("tail against hit %.1f dB, plain %.1f dB", ratio(b), ratio(a)));
}

SCENARIO(the_gate_cuts_the_room_after_its_hold) {
  GIVEN("the snare in a hall, ungated and gated with a 250 ms hold");
  auto open = rig::Engine::kit(dsp::kits::kStudioRock), gated = rig::Engine::kit(dsp::kits::kStudioRock);
  for (auto* e : {&open, &gated}) { e->s.v[params::kRoomSize] = 5; e->s.v[params::kMicDistance] = 3.0; e->s.v[params::kRoomAbsorption] = 0.1; }
  gated.s.v[params::kGate] = 0.6; gated.s.v[params::kGateHold] = 250.0;
  const auto a = open.stroke(38, 0.9, 0.2, 1.0), b = gated.stroke(38, 0.9, 0.2, 1.0);
  THEN("before the hold ends they are the same; 50 ms after it the gated room is 30 dB quieter");
  const Span before_a(a, static_cast<size_t>(0.1 * kRate), 4800), before_b(b, static_cast<size_t>(0.1 * kRate), 4800);
  CHECK_NEAR(measure::db(measure::rms(before_b)), measure::db(measure::rms(before_a)), 1.0, "open during the hold");
  const Span after_a(a, static_cast<size_t>(0.32 * kRate), 4800), after_b(b, static_cast<size_t>(0.32 * kRate), 4800);
  CHECK(measure::db(measure::rms(after_b)) < measure::db(measure::rms(after_a)) - 30.0, "shut after it");
}

int main(int argc, char** argv) { return spec::run(argc, argv); }
