// Executable scenarios for features/physics.feature: the physics every sound is built from,
// proved from the model's own numbers where a number is the claim (a Bessel zero, a bar's mode
// ratio, a blast's duration) and from the audio where the claim is what is heard (a pitch, a
// glide, a buzz, a Doppler shift).
#include <cmath>
#include <vector>

#include "../src/dsp/modal.hpp"
#include "../src/dsp/sources.hpp"
#include "../src/dsp/voice.hpp"
#include "audio_measure.hpp"
#include "rig.hpp"
#include "spec.hpp"

using namespace bonkr;
using measure::Span;
namespace modal = bonkr::dsp::modal;
constexpr double kRate = rig::kRate;

namespace {
// The RMS contour on a millisecond hop, linear, of the signal's second difference: the strokes'
// clicks stand out of the ringing they start.
std::vector<double> click_contour(const std::vector<float>& x) {
  std::vector<float> d(x.size(), 0.0f);
  for (size_t i = 2; i < x.size(); ++i) d[i] = x[i] - 2.0f * x[i - 1] + x[i - 2];
  auto c = measure::contour_db(d, kRate, 0.001, 0.002);
  for (double& v : c) v = std::pow(10.0, v / 20.0);
  return c;
}
}  // namespace

SCENARIO(the_membrane_modes_are_the_bessel_zeros) {
  GIVEN("the membrane's mode table, computed from J_m by Newton's method");
  const auto& t = modal::membrane_modes();
  THEN("its first modes are the tabulated zeros j01, j11, j21, j02, j31, j12 (Abramowitz and Stegun 9.5)");
  const double want[] = {2.404826, 3.831706, 5.135622, 5.520078, 6.380162, 7.015587};
  for (int i = 0; i < 6; ++i) CHECK_NEAR(t[static_cast<size_t>(i)].j, want[i], 1e-4, "zero");
  CHECK(t[0].m == 0 && t[1].m == 1 && t[2].m == 2 && t[3].m == 0 && t[3].n == 2, "the modes in order (0,1) (1,1) (2,1) (0,2)");
  THEN("J_0 at its first zero is zero, and J_1 at 1.0 is 0.4400506");
  CHECK_NEAR(modal::bessel_j(0, 2.404825557695773), 0.0, 1e-9, "J0(j01)");
  CHECK_NEAR(modal::bessel_j(1, 1.0), 0.4400505857, 1e-9, "J1(1)");
}

SCENARIO(a_drumhead_sounds_its_tension_over_its_density) {
  GIVEN("a 14 inch head at 2750 N/m of 0.353 kg/m^2 Mylar");
  const double f = modal::membrane_f01(0.356, 2750.0, modal::kHeadSingle10mil);
  THEN("f01 = 2.405 / (pi d) sqrt(T / sigma) is 189.8 Hz, and four times the tension doubles it");
  CHECK_NEAR(f, 2.404825557695773 / (dsp::kPi * 0.356) * std::sqrt(2750.0 / 0.353), 1e-9, "the formula");
  CHECK_NEAR(f, 189.8, 0.5, "Hz");
  CHECK_NEAR(modal::membrane_f01(0.356, 11000.0, modal::kHeadSingle10mil) / f, 2.0, 1e-9, "sqrt of the tension");
  WHEN("a frame drum (one head, 18 inch, goatskin at 320 N/m) is struck off centre");
  auto p = rig::play(0.6);
  p.tension = 0.0;  // no glide: the ratios at rest
  const auto x = rig::voice(dsp::kFrameDrum, p, 1.0);
  const double f01 = modal::membrane_f01(0.457, 320.0, modal::kGoatskin);
  THEN("its spectrum has the (1,1) and (2,1) modes at 1.594 and 2.136 times f01");
  const Span body(x, static_cast<size_t>(0.02 * kRate), static_cast<size_t>(0.5 * kRate));
  CHECK_NEAR(measure::peak_hz_near(body, kRate, f01 * 1.594, 30.0) / f01, 1.594, 0.01, "(1,1) / (0,1)");
  CHECK_NEAR(measure::peak_hz_near(body, kRate, f01 * 2.136, 30.0) / f01, 2.136, 0.01, "(2,1) / (0,1)");
}

SCENARIO(tuning_a_drum_scales_every_mode) {
  GIVEN("the 12 inch tom, then the same tom an octave up (four times the tension)");
  auto p = rig::play(0.5);
  p.tension = 0.0;
  const auto low = rig::voice(dsp::kTomMid, p, 0.8);
  p.tune = 2.0;
  const auto high = rig::voice(dsp::kTomMid, p, 0.8);
  const double f11 = modal::membrane_f01(0.305, 1130.0, modal::kHeadTwoPly7mil) * 1.5933;
  THEN("its (1,1) mode, which the other head does not split, moves from 1.593 f01 to twice that");
  const double a = rig::strongest_hz(Span(low, 1000, 30000), kRate, f11 * 0.98, f11 * 1.02);
  const double b = rig::strongest_hz(Span(high, 1000, 30000), kRate, 2.0 * f11 * 0.98, 2.0 * f11 * 1.02);
  CHECK_NEAR(a / f11, 1.0, 0.01, "(1,1) at its formula");
  CHECK_NEAR(b / a, 2.0, 0.02, "an octave");
}

SCENARIO(a_hard_stroke_starts_sharp_and_falls) {
  GIVEN("the 14 inch floor tom struck as hard as the keys allow");
  auto p = rig::play(1.0);
  const auto x = rig::voice(dsp::kTomFloor, p, 1.0);
  p.tension = 0.0;
  const auto flat = rig::voice(dsp::kTomFloor, p, 1.0);
  const double f01 = modal::membrane_f01(0.356, 965.0, modal::kHeadTwoPly7mil);
  WHEN("its lowest mode (the heads breathing together) is measured in the first 60 ms and after half a second");
  const auto early = [&](const std::vector<float>& v) { return measure::peak_hz_near(Span(v, 0, static_cast<size_t>(0.06 * kRate)), kRate, f01 * 0.9, 400.0); };
  const auto late = [&](const std::vector<float>& v) { return measure::peak_hz_near(Span(v, static_cast<size_t>(0.5 * kRate), static_cast<size_t>(0.25 * kRate)), kRate, f01 * 0.9, 400.0); };
  const double drop = measure::cents(early(x), late(x)), none = measure::cents(early(flat), late(flat));
  THEN("tension modulation starts it between 30 and 250 cents sharp; without it the pitch holds");
  CHECK(drop > 30.0 && drop < 250.0, spec::fmt("glide %.0f cents", drop));
  CHECK(std::abs(none) < 25.0 && drop - none > 30.0, spec::fmt("no glide %.1f cents", none));
}

SCENARIO(a_harder_stroke_is_a_shorter_contact) {
  GIVEN("Hertzian contact for a stick (alpha 1.5) and a felt mallet (alpha 2.8)");
  const auto stick = dsp::striker_of(dsp::kStick), felt = dsp::striker_of(dsp::kFeltMallet);
  THEN("four times the speed shortens the contact by 4^-((alpha - 1)/(alpha + 1)): 0.758 and 0.519");
  CHECK_NEAR(modal::Contact::contact_ms(stick, 12.0) / modal::Contact::contact_ms(stick, 3.0), std::pow(4.0, -0.2), 1e-9, "stick");
  CHECK_NEAR(modal::Contact::contact_ms(felt, 12.0) / modal::Contact::contact_ms(felt, 3.0), std::pow(4.0, -1.8 / 3.8), 1e-9, "felt");
  WHEN("the timpani is struck softly and hard with its felt mallet");
  const auto soft = rig::voice(dsp::kTimpani, rig::play(0.3), 0.5), hard = rig::voice(dsp::kTimpani, rig::play(1.0), 0.5);
  const double cs = measure::spectral_centroid(Span(soft, 0, 9600), kRate), ch = measure::spectral_centroid(Span(hard, 0, 9600), kRate);
  THEN("the hard stroke is brighter (a higher spectral centroid)");
  CHECK(ch > cs * 1.15, spec::fmt("centroid %.0f Hz against %.0f Hz", ch, cs));
}

SCENARIO(snare_wires_buzz_against_the_head) {
  GIVEN("the maple snare with its snares on and thrown off");
  auto p = rig::play(0.8);
  const auto on = rig::voice(dsp::kSnareMaple, p, 0.5);
  p.snares = 0.0;
  const auto off = rig::voice(dsp::kSnareMaple, p, 0.5);
  const Span after_stick_on(on, static_cast<size_t>(0.015 * kRate), static_cast<size_t>(0.15 * kRate));
  const Span after_stick_off(off, static_cast<size_t>(0.015 * kRate), static_cast<size_t>(0.15 * kRate));
  THEN("with the snares on, far more of the sound lies above 3 kHz after the stick has left");
  const double hi_on = measure::band_fraction(after_stick_on, kRate, 3000.0, 16000.0), hi_off = measure::band_fraction(after_stick_off, kRate, 3000.0, 16000.0);
  CHECK(hi_on > 4.0 * hi_off, spec::fmt("share above 3 kHz %.3f against %.3f", hi_on, hi_off));
  WHEN("the wires are tightened");
  p.snares = 1.0; p.snare_tension = 1.0;
  const auto tight = rig::voice(dsp::kSnareMaple, p, 0.5);
  p.snare_tension = 0.0;
  const auto loose = rig::voice(dsp::kSnareMaple, p, 0.5);
  THEN("tight wires part from the head less: less buzz after 60 ms than loose ones");
  const double buzz_tight = measure::rms(Span(tight, static_cast<size_t>(0.06 * kRate), static_cast<size_t>(0.1 * kRate)));
  const double buzz_loose = measure::rms(Span(loose, static_cast<size_t>(0.06 * kRate), static_cast<size_t>(0.1 * kRate)));
  CHECK(buzz_loose > buzz_tight, spec::fmt("loose %.4f against tight %.4f", buzz_loose, buzz_tight));
}

SCENARIO(the_kettle_makes_the_timpani_nearly_harmonic) {
  GIVEN("the timpani tuned to D3 (its principal (1,1) mode at 146.8 Hz)");
  const auto x = rig::voice(dsp::kTimpani, rig::play(0.6), 1.5);
  const Span body(x, static_cast<size_t>(0.1 * kRate), static_cast<size_t>(1.2 * kRate));
  THEN("the (2,1) and (3,1) modes sound a fifth and an octave above it (Rossing's 1.50 and 1.98)");
  const double f1 = rig::strongest_hz(body, kRate, 140.0, 154.0);
  CHECK_NEAR(f1, 146.8, 1.0, "principal");
  CHECK_NEAR(rig::strongest_hz(body, kRate, f1 * 1.45, f1 * 1.55) / f1, 1.50, 0.01, "fifth");
  CHECK_NEAR(rig::strongest_hz(body, kRate, f1 * 1.93, f1 * 2.03) / f1, 1.98, 0.01, "octave");
}

SCENARIO(bars_ring_the_euler_bernoulli_ratios) {
  GIVEN("the free-free bar's modes");
  THEN("they stand at 1 : 2.756 : 5.404 : 8.933 : 13.34");
  const double f1 = modal::free_bar_hz(1, 1.0, 1.0);
  CHECK_NEAR(modal::free_bar_hz(2, 1.0, 1.0) / f1, 2.7565, 1e-3, "second");
  CHECK_NEAR(modal::free_bar_hz(3, 1.0, 1.0) / f1, 5.4039, 1e-3, "third");
  CHECK_NEAR(modal::free_bar_hz(4, 1.0, 1.0) / f1, 8.9330, 1e-3, "fourth");
  CHECK_NEAR(modal::free_bar_hz(5, 1.0, 1.0) / f1, 13.3443, 1e-3, "fifth");
  THEN("so a tubular bell's 4th, 5th and 6th modes are nearly 2 : 3 : 4, the strike note an octave below the 4th");
  CHECK_NEAR(modal::free_bar_hz(5, 1.0, 1.0) / modal::free_bar_hz(4, 1.0, 1.0), 1.5, 0.01, "3 : 2");
  CHECK_NEAR(modal::free_bar_hz(6, 1.0, 1.0) / modal::free_bar_hz(4, 1.0, 1.0), 2.0, 0.1, "4 : 2");
  WHEN("rosewood claves 25 mm across and 0.2 m long are struck");
  const double claves = modal::free_bar_hz(1, 0.20, modal::bar_wave_constant(modal::kRosewood, 0.0125 / 2.0));
  const auto x = rig::voice(dsp::kClaves, rig::play(0.8), 0.3);
  THEN("they ring at the bar's fundamental, between 2 and 2.8 kHz like real claves");
  CHECK(claves > 2000.0 && claves < 2800.0, spec::fmt("%.0f Hz", claves));
  CHECK_NEAR(rig::strongest_hz(Span(x), kRate, 1000.0, 8000.0, 14) / claves, 1.0, 0.02, "the loudest peak");
}

SCENARIO(a_gong_rises_as_it_dies) {
  GIVEN("the opera gong, struck hard");
  const auto x = rig::voice(dsp::kOperaGong, rig::play(1.0), 1.5);
  const double early = measure::peak_hz_near(Span(x, 0, static_cast<size_t>(0.08 * kRate)), kRate, 380.0, 500.0);
  const double late = measure::peak_hz_near(Span(x, static_cast<size_t>(0.9 * kRate), static_cast<size_t>(0.4 * kRate)), kRate, 380.0, 500.0);
  THEN("its pitch climbs by more than a quarter-tone as the vibration dies (a softening nonlinearity)");
  CHECK(measure::cents(late, early) > 50.0, spec::fmt("%.0f cents up", measure::cents(late, early)));
}

SCENARIO(a_cymbal_is_dense_and_its_highs_bloom) {
  GIVEN("an 18 inch crash struck firmly");
  const auto x = rig::voice(dsp::kCrash18, rig::play(0.9), 1.5);
  THEN("its spectrum is dense: more than 40 separate peaks between 300 Hz and 16 kHz");
  CHECK(rig::peaks_hz(Span(x, 0, static_cast<size_t>(0.5 * kRate)), kRate, 200, 300.0, 16000.0, 15).size() > 40, "dense");
  THEN("and, with the energy its low modes pass up into the high ones, a third or more of it lies above 4 kHz, as in the recordings");
  const double high = measure::band_fraction(Span(x, 0, static_cast<size_t>(0.3 * kRate)), kRate, 4000.0, 20000.0, 12);
  CHECK(high > 0.33, spec::fmt("%.2f of the energy above 4 kHz", high));
  auto soft = rig::play(0.3);
  const auto y = rig::voice(dsp::kCrash18, soft, 0.5);
  const double soft_high = measure::band_fraction(Span(y, 0, static_cast<size_t>(0.3 * kRate)), kRate, 4000.0, 20000.0, 12);
  THEN("and the cascade grows with the square of the stroke: a soft stroke is darker");
  CHECK(soft_high < high, spec::fmt("%.2f soft against %.2f firm", soft_high, high));
}

SCENARIO(blasts_follow_kinney_and_graham) {
  GIVEN("Kinney and Graham's fits");
  THEN("overpressure falls with scaled distance and is 0.0986 atmospheres at 10 m/kg^(1/3)");
  CHECK_NEAR(dsp::src::kinney_overpressure_ratio(10.0), 0.0986, 0.001, "Ps / Pa at Z = 10");
  CHECK(dsp::src::kinney_overpressure_ratio(5.0) > dsp::src::kinney_overpressure_ratio(10.0) * 2.5, "closer is far stronger");
  const double z = 80.0 / std::cbrt(0.45);
  const double td_ms = dsp::src::kinney_duration_ms(z) * std::cbrt(0.45);
  WHEN("the cannon's pressure (0.45 kg TNT equivalent, 80 m away) arrives, before any microphone");
  dsp::src::ShockField field;
  field.prepare(kRate);
  field.set_path(0, 1.0);
  dsp::src::Shock shock;
  shock.length = td_ms * 0.001 * kRate; shock.amp = 1.0; shock.b = 1.4;
  field.add(shock);
  std::vector<float> p(static_cast<size_t>(0.05 * kRate));
  for (float& v : p) v = field.process();
  size_t onset = 0;
  while (onset < p.size() && std::abs(p[onset]) < 1e-4f) ++onset;
  size_t crossing = onset + 2;
  while (crossing < p.size() && p[crossing] > 0.0f) ++crossing;
  const double positive_ms = static_cast<double>(crossing - onset) / kRate * 1000.0;
  THEN("its positive phase lasts Kinney and Graham's duration");
  CHECK(td_ms > 2.0 && td_ms < 20.0, spec::fmt("td %.2f ms", td_ms));
  CHECK_NEAR(positive_ms / td_ms, 1.0, 0.05, spec::fmt("positive phase %.2f ms", positive_ms).c_str());
  const auto x = rig::voice(dsp::kCannon, rig::play(0.8), 0.5);
  size_t start_x = 0;
  while (start_x < x.size() && std::abs(x[start_x]) < 1e-4f) ++start_x;
  size_t zero_x = start_x + 2;
  while (zero_x < x.size() && x[zero_x] > 0.0f) ++zero_x;
  const size_t start = start_x, zero = zero_x;
  WHEN("the charge is eight times larger (an octave down)");
  auto big = rig::play(0.8);
  big.tune = 0.5;
  const auto y = rig::voice(dsp::kCannon, big, 0.5);
  size_t s2 = 0;
  while (s2 < y.size() && std::abs(y[s2]) < 1e-4f) ++s2;
  size_t z2 = s2 + 2;
  while (z2 < y.size() && y[z2] > 0.0f) ++z2;
  THEN("its positive phase is longer");
  CHECK(static_cast<double>(z2 - s2) > 1.3 * static_cast<double>(zero - start), "a bigger charge, a longer push");
}

SCENARIO(supersonic_bodies_trail_n_waves) {
  GIVEN("Whitham's N-wave");
  THEN("a 7.62 mm bullet at Mach 2.5 passing 5 m away makes one of 0.15 to 0.35 ms");
  const double t = dsp::src::whitham_duration_s(2.5, 0.00762, 0.03, 5.0);
  CHECK(t > 0.15e-3 && t < 0.35e-3, spec::fmt("%.3f ms", t * 1000.0));
  WHEN("the sonic boom (a 120 ms N-wave) is heard");
  const auto x = rig::voice(dsp::kSonicBoom, rig::play(0.8), 0.4);
  // Through a microphone the N-wave's slow ramp is gone; its two shocks remain: the double boom.
  size_t first = 0;
  for (size_t i = 0; i < x.size(); ++i) if (std::abs(x[i]) > std::abs(x[first])) first = i;
  size_t second = 0;
  for (size_t i = 0; i < x.size(); ++i)
    if ((i + static_cast<size_t>(0.05 * kRate) < first || i > first + static_cast<size_t>(0.05 * kRate)) && std::abs(x[i]) > std::abs(x[second])) second = i;
  THEN("its two shocks, the jump up at the front and back up at the tail, are 120 ms apart");
  CHECK_NEAR(std::abs(static_cast<double>(second) - static_cast<double>(first)) / kRate * 1000.0, 120.0, 4.0, "ms between the shocks");
}

SCENARIO(a_passing_jet_is_doppler_shifted) {
  GIVEN("the jet flyby: 140 m/s, 90 m off at its closest, its fan whining at 2450 Hz");
  const auto x = rig::voice(dsp::kJetFlyby, rig::play(0.8), 6.0);
  const double c = 343.0, v = 140.0;
  const double cos0 = 420.0 / std::sqrt(420.0 * 420.0 + 90.0 * 90.0);
  const double approach = 2450.0 / (1.0 - v / c * cos0), recede = 2450.0 / (1.0 + v / c * cos0);
  WHEN("the whine is measured as it approaches and as it leaves");
  const double heard_in = rig::strongest_hz(Span(x, static_cast<size_t>(0.2 * kRate), static_cast<size_t>(0.5 * kRate)), kRate, approach * 0.85, approach * 1.15, 15);
  const double heard_out = rig::strongest_hz(Span(x, static_cast<size_t>(5.0 * kRate), static_cast<size_t>(0.5 * kRate)), kRate, recede * 0.85, recede * 1.15, 15);
  THEN("the approaching pitch is f / (1 - v cos/c) and the receding f / (1 + v cos/c)");
  CHECK_NEAR(heard_in / approach, 1.0, 0.05, "approach");
  CHECK_NEAR(heard_out / recede, 1.0, 0.07, "recede");
}

SCENARIO(a_cold_day_shifts_the_passing_jet_further) {
  GIVEN("the jet flyby on a day at -10 C (sound at 325 m/s) and at 40 C (355 m/s)");
  auto cold = rig::play(0.8), hot = rig::play(0.8);
  cold.celsius = -10.0; hot.celsius = 40.0;
  const auto a = rig::voice(dsp::kJetFlyby, cold, 1.0), b = rig::voice(dsp::kJetFlyby, hot, 1.0);
  const double cos0 = 420.0 / std::sqrt(420.0 * 420.0 + 90.0 * 90.0);
  const auto approach = [&](double celsius) { return 2450.0 / (1.0 - 140.0 / dsp::air::speed_of_sound(celsius) * cos0); };
  WHEN("the approaching whine is measured");
  const double fa = rig::strongest_hz(Span(a, static_cast<size_t>(0.2 * kRate), static_cast<size_t>(0.5 * kRate)), kRate, approach(-10.0) * 0.9, approach(-10.0) * 1.1, 15);
  const double fb = rig::strongest_hz(Span(b, static_cast<size_t>(0.2 * kRate), static_cast<size_t>(0.5 * kRate)), kRate, approach(40.0) * 0.9, approach(40.0) * 1.1, 15);
  THEN("each is shifted by its own day's speed of sound: higher when the air is cold");
  CHECK_NEAR(fa / approach(-10.0), 1.0, 0.03, "cold day");
  CHECK_NEAR(fb / approach(40.0), 1.0, 0.03, "hot day");
  CHECK(fa > fb * 1.03, spec::fmt("%.0f Hz cold against %.0f Hz hot", fa, fb));
}

SCENARIO(dry_air_dulls_what_comes_from_far_away) {
  GIVEN("the big explosion 900 m away and the passing jet, in air at 20 C with 10% and with 70% humidity");
  auto dry = rig::play(0.8), humid = rig::play(0.8);
  dry.humidity = 0.1; humid.humidity = 0.7;
  THEN("ISO 9613-1 says dry air absorbs more of the treble over the path: both arrive darker when it is dry");
  for (int s : {dsp::kExplosion, dsp::kJetFlyby, dsp::kThunder}) {
    const double len = s == dsp::kJetFlyby ? 6.0 : 3.0;
    const auto a = rig::voice(s, dry, len), b = rig::voice(s, humid, len);
    const double ca = measure::spectral_centroid(Span(a), kRate, 13), cb = measure::spectral_centroid(Span(b), kRate, 13);
    CHECK(ca < 0.9 * cb, std::string(dsp::kSoundNames[s]) + spec::fmt(": centroid %.0f Hz dry against %.0f Hz humid", ca, cb));
  }
  THEN("and the model's corner for 900 m of air is where ISO 9613-1 takes 6 dB, lower in the dry air");
  const double fd = dsp::src::air_cutoff_hz(900.0, 20.0, 0.1, kRate), fh = dsp::src::air_cutoff_hz(900.0, 20.0, 0.7, kRate);
  CHECK_NEAR(dsp::air::absorption_db_per_m(fd, 20.0, 0.1) * 900.0, 6.0, 0.05, "6 dB at the dry corner");
  CHECK(fd < fh, spec::fmt("corner %.0f Hz dry against %.0f Hz humid", fd, fh));
}

SCENARIO(lips_buzz_a_vuvuzela_at_b_flat) {
  GIVEN("the vuvuzela blown and held");
  const auto x = rig::voice(dsp::kVuvuzela, rig::play(0.8), 1.0);
  THEN("the lips lock to the horn's resonance at B-flat 3 (233 Hz), within 40 cents");
  const double hz = measure::peak_hz_near(Span(x, static_cast<size_t>(0.5 * kRate), static_cast<size_t>(0.3 * kRate)), kRate, 233.0, 200.0);
  const double yin = measure::pitch_hz(Span(x, static_cast<size_t>(0.5 * kRate), static_cast<size_t>(0.3 * kRate)), kRate, 150.0, 600.0);
  CHECK(std::abs(measure::cents(yin, 233.0)) < 40.0, spec::fmt("periodic at %.1f Hz", yin));
  CHECK(std::abs(measure::cents(hz, 233.0)) < 40.0, spec::fmt("%.1f Hz", hz));
}

SCENARIO(a_siren_winds_up_while_held_and_down_after) {
  GIVEN("the air-raid siren held for 3 seconds, then let go");
  const auto x = rig::voice(dsp::kSiren, rig::play(0.8), 5.0, kRate, 3.0);
  const auto pitch_at = [&](double t) { return measure::pitch_hz(Span(x, static_cast<size_t>(t * kRate), 4800), kRate, 40.0, 1200.0, 0.3); };
  const double early = pitch_at(0.6), held = pitch_at(2.8), coasting = pitch_at(4.5);
  THEN("its pitch (ports times turns per second) climbs while the motor drives it and falls as it coasts");
  CHECK(held > early * 1.3, spec::fmt("%.0f Hz then %.0f Hz", early, held));
  CHECK(coasting < held * 0.95, spec::fmt("coasting %.0f Hz", coasting));
}

SCENARIO(a_telephone_bell_rings_at_the_ringing_current) {
  GIVEN("the telephone bell held for a second");
  const auto x = rig::voice(dsp::kTelephone, rig::play(0.8), 1.0);
  WHEN("the envelope's modulation is measured");
  const auto contour = click_contour(x);
  const double rate = measure::modulation_hz(std::vector<double>(contour.begin() + 50, contour.end()), 0.001, 20.0, 80.0);
  THEN("the clapper's swing repeats at the 20 Hz ringing current, striking one gong on each half");
  CHECK_NEAR(rate, 20.0, 1.5, "swings per second");
}

SCENARIO(a_ratchet_clacks_once_per_tooth) {
  GIVEN("the ratchet turned for a second: 12 teeth, 4 turns a second");
  const auto x = rig::voice(dsp::kRatchet, rig::play(0.8), 1.0);
  const auto contour = click_contour(x);
  THEN("it clacks 48 times a second");
  CHECK_NEAR(measure::modulation_hz(contour, 0.001, 20.0, 100.0), 48.0, 4.0, "clacks per second");
}

SCENARIO(shaken_particles_ring_their_shell) {
  GIVEN("the maracas, shaken");
  const auto x = rig::voice(dsp::kMaracas, rig::play(0.8), 0.4);
  THEN("the clicks of the seeds ring the gourd's resonance near 5.5 kHz (where the recordings put it), so the sound's centroid lies near it");
  const double c = measure::spectral_centroid(Span(x, 0, static_cast<size_t>(0.2 * kRate)), kRate, 11);
  CHECK(c > 3800.0 && c < 8000.0, spec::fmt("centroid %.0f Hz", c));
  THEN("and the shake's return stroke comes back after the seeds have settled");
  const auto contour = measure::contour_db(x, kRate, 0.005, 0.010);
  CHECK(contour.size() > 30 && contour[24] > contour[18], "a second burst at 110 ms");
}

SCENARIO(every_sound_is_finite_and_settles) {
  GIVEN("every sound, struck once at a firm velocity and released after 0.3 s");
  for (int s = 0; s < dsp::kSoundCount; ++s) {
    // A tam-tam rings for half a minute; everything else is done within nine seconds.
    const auto x = rig::voice(s, rig::play(0.8), s == dsp::kTamTam ? 30.0 : 9.0, kRate, 0.3);
    CHECK(rig::finite(x), std::string(dsp::kSoundNames[s]) + " finite");
    CHECK(measure::peak(Span(x)) > 1e-4, std::string(dsp::kSoundNames[s]) + " makes a sound");
    CHECK(measure::peak(Span(x, x.size() - 4800)) < 0.02 * measure::peak(Span(x)), std::string(dsp::kSoundNames[s]) + " dies away");
  }
}

int main(int argc, char** argv) { return spec::run(argc, argv); }
