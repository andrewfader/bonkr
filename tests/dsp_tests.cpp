// Executable scenarios for features/dsp_building_blocks.feature: the temperaments, the limiter,
// the body and mute filters and the delay line, measured in cents, dB and samples.
#include "../src/dsp/core.hpp"
#include "../src/dsp/filters.hpp"
#include "../src/dsp/machine.hpp"
#include "../src/dsp/modal.hpp"
#include "../src/dsp/sources.hpp"
#include "../src/engine.hpp"
#include "audio_measure.hpp"
#include "spec.hpp"

#include <vector>

using namespace bonkr::dsp;

SCENARIO(the_limiter_holds_the_ceiling_and_recovers) {
  GIVEN("the output limiter fed a sine 6 dB over the ceiling, then one 6 dB under");
  bonkr::Limiter lim;
  lim.prepare(48000.0);
  std::vector<float> hot(24000), cool(24000);
  for (size_t i = 0; i < hot.size(); ++i) { const float x = static_cast<float>(2.0 * bonkr::Limiter::kCeiling * std::sin(kTau * 440.0 * static_cast<double>(i) / 48000.0)); hot[i] = x * lim.gain(x, x); }
  const double held = lim.held_db();
  for (size_t i = 0; i < cool.size(); ++i) { const float x = static_cast<float>(0.5 * bonkr::Limiter::kCeiling * std::sin(kTau * 440.0 * static_cast<double>(i) / 48000.0)); cool[i] = x * lim.gain(x, x); }
  THEN("nothing leaves above the ceiling, the meter reads the 6 dB it took, and the quiet signal passes untouched once the gain has recovered");
  CHECK(measure::peak(measure::Span(hot)) <= bonkr::Limiter::kCeiling + 1e-4, "ceiling held");
  CHECK_NEAR(held, 6.0, 0.3, "reduction metered");
  CHECK_NEAR(measure::db(measure::peak(measure::Span(cool, 12000))), measure::db(0.5 * bonkr::Limiter::kCeiling), 0.1, "quiet signal untouched");
}

namespace {
// The steady amplitude, in dB against the input, of a sine at `hz` through `filter`.
template <typename F>
double sine_gain_db(F& filter, double hz) {
  std::vector<float> out(48000);
  for (size_t i = 0; i < out.size(); ++i) out[i] = filter.process(static_cast<float>(0.25 * std::sin(kTau * hz * static_cast<double>(i) / 48000.0)));
  return measure::db(measure::tone_amplitude(measure::Span(out, 24000, 16384), 48000.0, hz) / 0.25);
}
}  // namespace

SCENARIO(the_biquads_shape_the_spectrum_as_designed) {
  GIVEN("an RBJ peaking filter of +8 dB at 1800 Hz, Q 1.5");
  THEN("a sine at the centre gains 8 dB and one far below it passes untouched");
  Biquad peak;
  peak.set(Biquad::Type::Peak, 1800.0, 1.5, 8.0, 48000.0);
  CHECK_NEAR(sine_gain_db(peak, 1800.0), 8.0, 0.1, "gain at the centre, dB");
  peak.reset();
  CHECK_NEAR(sine_gain_db(peak, 60.0), 0.0, 0.1, "gain two decades below, dB");
  GIVEN("a Butterworth low-pass (Q 0.707) at 700 Hz");
  THEN("it is 3 dB down at its corner, flat an octave and more below, and falls at least 12 dB per octave above, as the bilinear Butterworth gives");
  Biquad low;
  low.set(Biquad::Type::LowPass, 700.0, 0.7071, 0.0, 48000.0);
  CHECK_NEAR(sine_gain_db(low, 700.0), -3.01, 0.1, "corner, dB");
  low.reset();
  CHECK_NEAR(sine_gain_db(low, 100.0), 0.0, 0.05, "passband, dB");
  low.reset();
  const double at_4 = sine_gain_db(low, 2800.0);
  low.reset();
  const double at_8 = sine_gain_db(low, 5600.0);
  // The bilinear transform's Butterworth: |H|^2 = 1 / (1 + W^4) at the prewarped W = tan(pi f / fs)
  // / tan(pi fc / fs), which tends to 12 dB per octave and steepens toward Nyquist.
  const auto warped_db = [](double hz) { const double w = std::tan(kPi * hz / 48000.0) / std::tan(kPi * 700.0 / 48000.0); return -10.0 * std::log10(1.0 + w * w * w * w); };
  CHECK_NEAR(at_4, warped_db(2800.0), 0.1, "two octaves above, dB");
  CHECK_NEAR(at_4 - at_8, warped_db(2800.0) - warped_db(5600.0), 0.1, "the third octave's slope, dB");
  CHECK(at_4 - at_8 > 12.0, "at least 12 dB per octave");
}

SCENARIO(a_delay_line_delays_by_its_fractional_length) {
  GIVEN("a delay line holding an impulse, read 10.25 samples back");
  DelayLine line;
  line.prepare(64);
  std::vector<float> out;
  for (int n = 0; n < 32; ++n) { line.push(n == 0 ? 1.0f : 0.0f); out.push_back(line.read(10.25)); }
  THEN("the impulse comes out split between samples 10 and 11 as 0.75 and 0.25: its centre at 10.25");
  CHECK_NEAR(static_cast<double>(out[10]), 0.75, 1e-6, "sample 10");
  CHECK_NEAR(static_cast<double>(out[11]), 0.25, 1e-6, "sample 11");
  double sum = 0.0, moment = 0.0;
  for (size_t i = 0; i < out.size(); ++i) { sum += out[i]; moment += static_cast<double>(i) * out[i]; }
  CHECK_NEAR(sum, 1.0, 1e-6, "energy neither lost nor gained");
  CHECK_NEAR(moment / sum, 10.25, 1e-6, "centre of the delayed impulse, samples");
  THEN("a whole-sample read is exact, and a read past the capacity is held at the capacity");
  CHECK(line.read_int(31) == 1.0f && line.read_int(30) == 0.0f, "whole-sample read");
  CHECK(line.capacity() == 64 && line.read(1000.0) == line.read(64.0), "clamped to the capacity");
}

SCENARIO(a_contact_delivers_its_impulse_over_its_duration) {
  GIVEN("a contact of impulse 2 over 1 ms at 48 kHz");
  bonkr::dsp::modal::Contact c;
  c.prepare(48000.0);
  c.add(2.0, 1.0);
  std::vector<float> f;
  while (c.active()) f.push_back(c.next());
  THEN("the force is a half-sine 48 samples long whose area is the impulse");
  double area = 0.0;
  for (float v : f) area += v;
  CHECK(f.size() == 48, spec::fmt("%.0f samples", static_cast<double>(f.size())));
  CHECK_NEAR(area, 2.0, 0.01, "impulse");
  CHECK(f[24] > f[4] && f[24] > f[44], "rises and falls");
}

SCENARIO(a_mode_rings_at_its_frequency_for_its_decay_time) {
  GIVEN("one mode at 440 Hz with a T60 of 0.5 s, struck once");
  bonkr::dsp::modal::ModeBank bank;
  bank.prepare(48000.0);
  const bonkr::dsp::modal::ModeSpec m{440.0, 0.5, 1.0, 1.0};
  bank.set(&m, 1);
  std::vector<float> x(48000);
  for (size_t i = 0; i < x.size(); ++i) x[i] = bank.process(i == 0 ? 1.0f : 0.0f);
  THEN("it rings at 440 Hz and is 60 dB down after half a second");
  CHECK_NEAR(measure::peak_hz_near(measure::Span(x, 0, 12000), 48000.0, 440.0, 20.0), 440.0, 0.3, "Hz");
  const double a = measure::rms(measure::Span(x, 0, 960)), b = measure::rms(measure::Span(x, 24000, 960));
  CHECK_NEAR(measure::db(b) - measure::db(a), -60.0, 1.5, "60 dB in T60");
}

SCENARIO(the_converters_quantise_as_their_storage_does) {
  GIVEN("the SP-1200's 12-bit linear store and the LinnDrum's 8-bit companded one");
  bonkr::dsp::machine::Converter sp, linn;
  sp.prepare(48000.0); linn.prepare(48000.0);
  sp.set(bonkr::dsp::kits::kConvSp1200); linn.set(bonkr::dsp::kits::kConvLinn);
  THEN("the SP-1200's step is 1/2047 everywhere");
  CHECK_NEAR(static_cast<double>(sp.quantise(0.5f) - sp.quantise(0.5f - 1.0f / 2047.0f)), 1.0 / 2047.0, 1e-6, "step at half scale");
  CHECK(sp.quantise(0.0001f) == 0.0f, "below half a step is silence");
  THEN("the LinnDrum's steps are fine near zero and coarse near full scale (mu-law)");
  const float small = linn.quantise(0.0005f), big_step = linn.quantise(0.9f) - linn.quantise(0.85f);
  CHECK(small > 0.0f && small < 0.002f, "a quiet sample survives");
  CHECK(big_step > 0.02f, "loud samples move in coarse steps");
  CHECK(sp.quantise(1.7f) <= 1.0f && linn.quantise(-3.0f) >= -1.0f, "clipped at full scale");
}

SCENARIO(the_listen_mic_compressor_squashes_hard_and_fast) {
  GIVEN("the compressor at full crush, fed a -6 dBFS tone then a -40 dBFS one");
  bonkr::dsp::machine::Compressor c;
  c.prepare(48000.0);
  c.set(1.0);
  const auto out_level = [&](double amp) {
    c.reset();
    std::vector<float> y(9600);
    for (size_t i = 0; i < y.size(); ++i) { float l = static_cast<float>(amp * std::sin(kTau * 200.0 * static_cast<double>(i) / 48000.0)), r = l; c.process(l, r); y[i] = l; }
    return measure::db(measure::peak(measure::Span(y, 4800)));
  };
  const double loud = out_level(0.5), quiet = out_level(0.01);
  THEN("34 dB of input difference comes out as less than 8 dB (above a -40 dBFS threshold at 20 : 1)");
  CHECK(loud - quiet < 8.0, spec::fmt("%.1f dB apart", loud - quiet));
}

int main(int argc, char** argv) { return spec::run(argc, argv); }
