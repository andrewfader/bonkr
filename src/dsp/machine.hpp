// What the drum machines and the studio did to drums after they were struck.
//
// - The sample-playback machines' converters. A converter holds each sample for one period of its
//   clock (a zero-order hold) and quantises it; without an anti-aliasing filter in front, what is
//   above half the clock folds down, and without a reconstruction filter behind, the clock's
//   images stay. Eight-bit machines (the LinnDrum and the DMX) stored samples companded (finer
//   steps near zero, a mu-law curve, as their AM6070 DACs expanded them), so a decay keeps its
//   shape but grows grainy; twelve-bit linear storage (the SP-1200, the MPC60) gives a fixed
//   step, so a quiet tail turns to grit. Clock rates and filters are those the manuals and
//   service notes give, rounded: LinnDrum 28 kHz, DMX 25 kHz, SP-1200 26.04 kHz with no input
//   filter, MPC60 40 kHz, HR-16 47.6 kHz at 16 bits.
// - Driving a machine's or a desk's input: tanh saturation.
// - The SSL listen-mic compressor: a fixed, very high ratio with a fast attack and release, which
//   pulls a room's tail up to the level of the hit.
// - A noise gate on that room: open while the sound is over the threshold, held open for the hold
//   time, then shut fast. Together they are the gated drum sound of the early 1980s.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "core.hpp"
#include "filters.hpp"
#include "kits.hpp"

namespace bonkr::dsp::machine {

struct ConverterSpec {
  const char* name;
  double clock_hz;     // 0: no converter
  int bits;
  bool companded;      // mu-law storage
  double input_lp_hz;  // anti-aliasing in front (0: none)
  double output_lp_hz; // reconstruction behind (0: none)
};
inline constexpr std::array<ConverterSpec, kits::kConverterCount> kConverters{{
    {"Kit's own", 0.0, 0, false, 0.0, 0.0},
    {"Clean", 0.0, 0, false, 0.0, 0.0},
    {"LinnDrum 8-bit", 28000.0, 8, true, 11000.0, 11000.0},
    {"Oberheim DMX 8-bit", 25000.0, 8, true, 10000.0, 10000.0},
    {"E-mu SP-1200 12-bit", 26040.0, 12, false, 0.0, 0.0},
    {"Akai MPC60 12-bit", 40000.0, 12, false, 17000.0, 17000.0},
    {"Alesis HR-16 16-bit", 47600.0, 16, false, 15000.0, 15000.0},
}};
inline constexpr const char* kConverterNames[kits::kConverterCount] = {
    "Kit's own", "Clean", "LinnDrum 8-bit", "Oberheim DMX 8-bit", "E-mu SP-1200 12-bit", "Akai MPC60 12-bit", "Alesis HR-16 16-bit"};

class Converter {
 public:
  void prepare(double rate) {
    rate_ = rate;
    for (auto& f : in_) f.prepare(rate);
    for (auto& f : out_) f.prepare(rate);
    reset();
  }
  void reset() { phase_ = 1.0; held_ = {0.0f, 0.0f}; for (auto& f : in_) f.reset(); for (auto& f : out_) f.reset(); }
  void set(int which) {
    which = std::clamp(which, 0, kits::kConverterCount - 1);
    if (which == which_) return;
    which_ = which;
    const ConverterSpec& s = kConverters[static_cast<size_t>(which)];
    spec_ = s;
    step_ = s.clock_hz > 0.0 ? s.clock_hz / rate_ : 0.0;
    for (auto& f : in_) f.set(s.input_lp_hz > 0.0 ? s.input_lp_hz : 20000.0, 0.7071);
    for (auto& f : out_) f.set(s.output_lp_hz > 0.0 ? s.output_lp_hz : 20000.0, 0.7071);
    reset();
  }
  int which() const { return which_; }
  const ConverterSpec& spec() const { return spec_; }
  void process(float& l, float& r) {
    if (step_ <= 0.0) return;
    float x[2] = {l, r};
    if (spec_.input_lp_hz > 0.0) for (int c = 0; c < 2; ++c) x[c] = in_[static_cast<size_t>(c)].low(x[c]);
    phase_ += step_;
    if (phase_ >= 1.0) {
      phase_ -= std::floor(phase_);
      for (int c = 0; c < 2; ++c) held_[static_cast<size_t>(c)] = quantise(x[c]);
    }
    float y[2] = {held_[0], held_[1]};
    if (spec_.output_lp_hz > 0.0) for (int c = 0; c < 2; ++c) y[c] = out_[static_cast<size_t>(c)].low(y[c]);
    l = y[0]; r = y[1];
  }
  // One sample through the store: clipped at full scale, then quantised (companded or linear).
  float quantise(float v) const {
    v = std::clamp(v, -1.0f, 1.0f);
    const float levels = static_cast<float>((1 << (spec_.bits - 1)) - 1);
    if (spec_.companded) {
      constexpr float mu = 255.0f;
      const float c = std::copysign(std::log1p(mu * std::abs(v)) / std::log1p(mu), v);
      const float q = std::round(c * levels) / levels;
      return std::copysign((std::pow(1.0f + mu, std::abs(q)) - 1.0f) / mu, q);
    }
    return std::round(v * levels) / levels;
  }
 private:
  std::array<StateVariable, 2> in_{}, out_{};
  ConverterSpec spec_ = kConverters[1];
  double rate_ = 48000.0, step_ = 0.0, phase_ = 1.0;
  std::array<float, 2> held_{};
  int which_ = 1;
};

// The listen-mic compressor: feed-forward, peak-sensing, 20 : 1 over a threshold that `amount`
// lowers from 0 to -40 dBFS, attack 0.1 ms, release 60 ms, with the make-up gain that puts a lone
// hit back where it started.
class Compressor {
 public:
  void prepare(double rate) {
    attack_ = 1.0 - std::exp(-1.0 / (0.0001 * rate));
    release_ = 1.0 - std::exp(-1.0 / (0.060 * rate));
    env_ = 0.0;
  }
  void reset() { env_ = 0.0; }
  void set(double amount) { amount_ = std::clamp(amount, 0.0, 1.0); threshold_db_ = -40.0 * amount_; }
  void process(float& l, float& r) {
    if (amount_ <= 0.0) return;
    const double peak = std::max(std::abs(l), std::abs(r));
    env_ += (peak - env_) * (peak > env_ ? attack_ : release_);
    const double level_db = gain_to_db(env_);
    double gain_db = 0.0;
    if (level_db > threshold_db_) gain_db = (threshold_db_ + (level_db - threshold_db_) / 20.0) - level_db;
    // Make-up: half the reduction a full-scale hit would get, so the tail rises and the hit stays.
    const double makeup = -0.5 * ((threshold_db_ + (0.0 - threshold_db_) / 20.0) - 0.0);
    const float g = static_cast<float>(db_to_gain(gain_db + makeup));
    l *= g; r *= g;
  }
  double reduction_db() const {
    const double level_db = gain_to_db(env_);
    return level_db > threshold_db_ ? level_db - (threshold_db_ + (level_db - threshold_db_) / 20.0) : 0.0;
  }
 private:
  double attack_ = 0.1, release_ = 0.001, env_ = 0.0, amount_ = 0.0, threshold_db_ = 0.0;
};

// The gate. `threshold` 0 is off; above it the gate opens at a level from -50 to -10 dBFS.
class Gate {
 public:
  void prepare(double rate) {
    rate_ = rate;
    open_coef_ = 1.0 - std::exp(-1.0 / (0.0005 * rate));
    close_coef_ = 1.0 - std::exp(-1.0 / (0.004 * rate));
    detect_ = 1.0 - std::exp(-1.0 / (0.0003 * rate));
    detect_release_ = 1.0 - std::exp(-1.0 / (0.010 * rate));
    reset();
  }
  void reset() { gain_ = 1.0; env_ = 0.0; hold_left_ = 0.0; }
  void set(double threshold, double hold_ms) {
    on_ = threshold > 0.0;
    threshold_ = db_to_gain(-50.0 + 40.0 * std::clamp(threshold, 0.0, 1.0));
    hold_ = hold_ms * 0.001 * rate_;
  }
  // `key` is the signal that opens the gate (the dry drums); it gates l and r (the room).
  void process(float key, float& l, float& r) {
    if (!on_) { gain_ = 1.0; return; }
    const double k = std::abs(key);
    env_ += (k - env_) * (k > env_ ? detect_ : detect_release_);
    if (env_ > threshold_) hold_left_ = hold_;
    const double target = hold_left_ > 0.0 ? 1.0 : 0.0;
    if (hold_left_ > 0.0) hold_left_ -= 1.0;
    gain_ += (target - gain_) * (target > gain_ ? open_coef_ : close_coef_);
    l *= static_cast<float>(gain_); r *= static_cast<float>(gain_);
  }
  double gain() const { return gain_; }
 private:
  double rate_ = 48000.0, open_coef_ = 0.1, close_coef_ = 0.01, detect_ = 0.1, detect_release_ = 0.01;
  double gain_ = 1.0, env_ = 0.0, hold_left_ = 0.0, threshold_ = 0.01, hold_ = 0.0;
  bool on_ = false;
};

}  // namespace bonkr::dsp::machine
