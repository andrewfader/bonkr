// Sound sources that are not a single struck body: noise shaped by what makes it, rattling
// particles, snare wires, analogue drum-machine circuits, blast waves and shock waves, a
// lip-buzzed horn, and a moving source heard from the ground.
//
// - Particles (Cook's PhISEM, 1997): beads, seeds or jingles in a shell. Shaking puts energy into
//   the system, which dies away at the system's decay; each sample a collision happens with a
//   probability proportional to the number of particles, adding a click of the system's current
//   energy, which rings the shell's resonances and dies at the sound's decay.
// - Snare wires: the wires rest pressed against the snare-side head and follow it while its
//   acceleration away from them is less than their tension can give them; when the head pulls
//   away faster they part, fly free and slap back. Every slap is a click of the closing speed
//   into the wires' own bright, dense resonances.
// - Blast waves: the pressure at a distance from an explosion is Friedlander's waveform,
//   p(t) = Ps (1 - t / td) exp(-b t / td), with the peak overpressure Ps and positive duration td
//   of Kinney and Graham's fits ("Explosive Shocks in Air", 1985) at the scaled distance
//   Z = R / W^(1/3) (W in kg of TNT). Sound reflects from the ground (a second arrival later by
//   the path difference) and from terrain far away (the rumble), losing the air's absorption
//   (ISO 9613-1) on every path.
// - Supersonic bodies trail an N-wave (Whitham, 1952): a jump to +dp, a straight fall to -dp, a
//   jump back. Its duration grows as M d r^(1/4) / (c (M^2 - 1)^(3/8) l^(1/4)); a rifle bullet's is a
//   few hundred microseconds, an aircraft's a few hundred milliseconds.
// - The lip-buzzed horn: the lips, locked to the horn's resonance, let the breath through in
//   pulses that the cone's harmonic impedance peaks and its bell shape.
// - A moving source is heard at the time its sound left it: the emission clock runs at
//   1 / (1 + dr/dt / c) of the listener's (the Doppler shift falls out), the level falls as 1/r,
//   and the air takes the top end off over r.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "air.hpp"
#include "core.hpp"
#include "filters.hpp"

namespace bonkr::dsp::src {

// A two-pole resonator (constant peak gain), set by frequency and Q.
class Resonator {
 public:
  void set(double hz, double q, double rate) {
    hz = std::clamp(hz, 10.0, rate * 0.45);
    const double r = std::exp(-kPi * hz / (std::max(q, 0.3) * rate));
    a1_ = static_cast<float>(2.0 * r * std::cos(kTau * hz / rate));
    a2_ = static_cast<float>(-r * r);
    g_ = static_cast<float>((1.0 - r * r) * 0.5);
  }
  float process(float x) {
    const float y = g_ * (x - x2_) + a1_ * y1_ + a2_ * y2_;
    x2_ = x1_; x1_ = x; y2_ = y1_; y1_ = y;
    return y;
  }
  void reset() { x1_ = x2_ = y1_ = y2_ = 0.0f; }
 private:
  float a1_ = 0.0f, a2_ = 0.0f, g_ = 0.0f, x1_ = 0.0f, x2_ = 0.0f, y1_ = 0.0f, y2_ = 0.0f;
};

// Noise through a band (or high-pass, or low-pass), with an attack-decay envelope that a strike
// starts, or driven sample by sample from outside (the wires, a cymbal's cascade).
class NoiseBand {
 public:
  enum class Shape : uint8_t { Band, High, Low };
  void prepare(double rate) { rate_ = rate; filter_.prepare(rate); filter2_.prepare(rate); reset(); }
  void reset() { filter_.reset(); filter2_.reset(); env_ = 0.0; level_ = 0.0f; attack_t_ = 1e9; }
  void set(Shape shape, double hz, double q, double attack_ms, double decay_ms, double level, bool steep = false) {
    shape_ = shape; level_ = static_cast<float>(level); steep_ = steep;
    filter_.set(hz, q); filter2_.set(hz, q);
    attack_ = std::max(1.0, attack_ms * 0.001 * rate_);
    decay_ = std::exp(-6.907755 / std::max(1.0, decay_ms * 0.001 * rate_ / 1.0));
    // decay_ms is a 60 dB time.
  }
  void set_decay(double rate, double decay_ms) { decay_ = std::exp(-6.907755 / std::max(1.0, decay_ms * 0.001 * rate)); }
  void trigger(double amount) { peak_ = std::max(env_, amount); attack_t_ = 0.0; rising_ = true; }
  void drive(double amount) { env_ += amount; }
  // Follows a target amplitude with a time constant (coef = 1 / (tau * rate)): a band fed by
  // energy cascading from the low modes (its amplitude settles at the target after tau).
  void follow(double target, double coef) { env_ += (target - env_) * coef; }
  float process(Rng& rng) {
    if (rising_) {
      attack_t_ += 1.0;
      env_ = std::min(1.0, attack_t_ / attack_) * peak_;
      if (attack_t_ >= attack_) rising_ = false;
    } else {
      env_ *= decay_;
    }
    if (env_ < 1e-7 && !rising_) return 0.0f;
    const float n = rng.bipolar() * static_cast<float>(env_) * level_;
    float y = 0.0f;
    switch (shape_) {
      case Shape::Band: y = filter_.band(n); if (steep_) y = filter2_.band(y); break;
      case Shape::High: y = n - filter_.low(n); if (steep_) y = y - filter2_.low(y); break;
      case Shape::Low: y = filter_.low(n); if (steep_) y = filter2_.low(y); break;
    }
    return y;
  }
  bool active() const { return rising_ || env_ > 1e-6; }
  double envelope() const { return env_; }
 private:
  StateVariable filter_, filter2_;
  Shape shape_ = Shape::Band;
  double rate_ = 48000.0, env_ = 0.0, attack_ = 1.0, decay_ = 0.999, peak_ = 1.0, attack_t_ = 1e9;
  float level_ = 0.0f;
  bool rising_ = false, steep_ = false;
};

// PhISEM: particles in a shell.
class Particles {
 public:
  static constexpr int kBands = 4;
  void prepare(double rate) { rate_ = rate; reset(); }
  void reset() { energy_ = 0.0; sound_ = 0.0; for (auto& r : res_) r.reset(); }
  struct Setup {
    int objects = 25;            // how many beads, seeds or jingles
    double system_t60 = 0.25;    // how long the shaken system keeps moving, s
    double sound_t60 = 0.004;    // how long one collision's click lasts, s
    std::array<double, kBands> hz{3200.0, 0, 0, 0};
    std::array<double, kBands> q{1.5, 1.5, 1.5, 1.5};
    std::array<double, kBands> gain{1.0, 0, 0, 0};
    double jitter = 0.0;         // per-collision spread of the resonances (jingles that differ)
  };
  void configure(const Setup& s) {
    setup_ = s;
    system_decay_ = std::exp(-6.907755 / (std::max(s.system_t60, 0.001) * rate_));
    sound_decay_ = std::exp(-6.907755 / (std::max(s.sound_t60, 0.0002) * rate_));
    for (int i = 0; i < kBands; ++i) res_[static_cast<size_t>(i)].set(s.hz[static_cast<size_t>(i)] > 0 ? s.hz[static_cast<size_t>(i)] : 1000.0, s.q[static_cast<size_t>(i)], rate_);
  }
  void shake(double energy) { energy_ += energy; }
  void stop() { energy_ *= 0.05; }
  float process(Rng& rng) {
    if (energy_ < 1e-7 && sound_ < 1e-7) return 0.0f;
    energy_ *= system_decay_;
    const double p = std::min(0.5, setup_.objects / 1024.0 * 48000.0 / rate_ * 1.0);
    if (rng.unipolar() < p) {
      sound_ += energy_ * (0.6 + 0.8 * rng.unipolar());
      if (setup_.jitter > 0.0 && (rng.next() & 3) == 0) {
        const size_t i = rng.next() % kBands;
        if (setup_.hz[i] > 0.0) res_[i].set(setup_.hz[i] * (1.0 + setup_.jitter * rng.bipolar()), setup_.q[i], rate_);
      }
    }
    const float x = rng.bipolar() * static_cast<float>(sound_);
    sound_ *= sound_decay_;
    float y = 0.0f;
    for (int i = 0; i < kBands; ++i)
      if (setup_.gain[static_cast<size_t>(i)] != 0.0) y += static_cast<float>(setup_.gain[static_cast<size_t>(i)]) * res_[static_cast<size_t>(i)].process(x);
    return y;
  }
  bool active() const { return energy_ > 1e-6 || sound_ > 1e-6; }
 private:
  Setup setup_{};
  std::array<Resonator, kBands> res_{};
  double rate_ = 48000.0, energy_ = 0.0, sound_ = 0.0, system_decay_ = 0.999, sound_decay_ = 0.95;
};

// The snare wires against the snare-side head.
class SnareWires {
 public:
  void prepare(double rate) {
    rate_ = rate; dt_ = 1.0 / rate;
    for (auto& r : bright_) r.reset();
    reset();
  }
  void reset() { w_ = v_ = 0.0; last_d_ = 0.0; env_ = 0.0; for (auto& r : bright_) r.reset(); }
  // tension 0..1: how hard the strainer presses them on (looser wires part earlier and buzz longer);
  // amount 0..1: off (thrown off) to full.
  void configure(double tension, double amount, double decay_ms) {
    amount_ = amount;
    // The wires' restoring acceleration toward the head, per unit of head displacement scale.
    preload_ = 0.15 + 1.6 * tension;
    env_decay_ = std::exp(-6.907755 / std::max(1.0, decay_ms * 0.001 * rate_));
    // The wires' own ringing: many short steel strands, a broad band from 1.5 to 7 kHz.
    static constexpr double kHz[4] = {1600.0, 2700.0, 4000.0, 5600.0};
    for (int i = 0; i < 4; ++i) bright_[static_cast<size_t>(i)].set(std::min(kHz[i], rate_ * 0.42), 1.0, rate_);
  }
  // d: the snare-side head's displacement (scaled so a firm stroke peaks near 1).
  float process(double d, Rng& rng) {
    if (amount_ <= 0.0) return 0.0f;
    const double vd = (d - last_d_) * rate_;
    last_d_ = d;
    // The wires are below the head (on the outside); the strainer's tension pulls them up into it.
    // Pressed up into the head by the strainer: a fixed upward acceleration (the head moves of the
    // order of 1 at a firm stroke, at a few hundred hertz: some 1e6 per s^2 at its peak).
    const double a = preload_ * 6.0e5;
    v_ += a * dt_;
    w_ += v_ * dt_;
    if (w_ > d) {  // they meet the head
      // Resting contact (the wires simply riding on the head) makes no sound; a slap does, its
      // strength the closing speed beyond what one sample of their own pull gives them.
      const double closing = std::abs(v_ - vd) - 3.0 * a * dt_;
      if (closing > 0.0) env_ += closing * 0.0015;
      w_ = d; v_ = vd;
    }
    env_ *= env_decay_;
    const float n = rng.bipolar() * static_cast<float>(std::min(env_, 2.0));
    float y = 0.0f;
    y += bright_[0].process(n) * 0.9f;
    y += bright_[1].process(n) * 1.0f;
    y += bright_[2].process(n) * 0.8f;
    y += bright_[3].process(n) * 0.5f;
    return y * static_cast<float>(amount_ * 0.25);
  }
  bool active() const { return env_ > 1e-5; }
 private:
  std::array<Resonator, 4> bright_{};
  double rate_ = 48000.0, dt_ = 1.0 / 48000.0, w_ = 0.0, v_ = 0.0, last_d_ = 0.0, env_ = 0.0;
  double amount_ = 1.0, preload_ = 1.0, env_decay_ = 0.99;
};

// --- analogue circuits ----------------------------------------------------------------------------------

// The TR-808's metal: six square-wave oscillators at fixed, mutually inharmonic frequencies.
class SquareBank {
 public:
  void prepare(double rate) { rate_ = rate; phase_.fill(0.0); }
  void set(const double* hz, int n, double ratio) {
    count_ = std::min(n, 6);
    for (int i = 0; i < count_; ++i) inc_[static_cast<size_t>(i)] = hz[i] * ratio / rate_;
  }
  float process() {
    float s = 0.0f;
    for (int i = 0; i < count_; ++i) {
      double& p = phase_[static_cast<size_t>(i)];
      p += inc_[static_cast<size_t>(i)]; if (p >= 1.0) p -= 1.0;
      s += p < 0.5 ? 1.0f : -1.0f;
    }
    return count_ ? s / static_cast<float>(count_) : 0.0f;
  }
 private:
  std::array<double, 6> phase_{}, inc_{};
  int count_ = 0;
  double rate_ = 48000.0;
};

// The TR-909's swept oscillators: a triangle-ish core whose pitch falls exponentially from a start
// to a rest frequency, waveshaped toward a sine by the following transistor pair.
class SweptOsc {
 public:
  void prepare(double rate) { rate_ = rate; reset(); }
  void reset() { phase_ = 0.0; t_ = 1e9; }
  void trigger(double start_hz, double end_hz, double sweep_ms, double decay_ms) {
    start_ = start_hz; end_ = end_hz; t_ = 0.0;
    sweep_ = std::exp(-1.0 / std::max(1.0, sweep_ms * 0.001 * rate_));
    decay_ = std::exp(-6.907755 / std::max(1.0, decay_ms * 0.001 * rate_));
    sweep_level_ = 1.0; env_ = 1.0; phase_ = 0.0;
  }
  float process() {
    if (env_ < 1e-6) return 0.0f;
    const double hz = end_ + (start_ - end_) * sweep_level_;
    sweep_level_ *= sweep_;
    env_ *= decay_;
    phase_ += hz / rate_; if (phase_ >= 1.0) phase_ -= 1.0;
    const double tri = 1.0 - 4.0 * std::abs(phase_ - 0.5);
    const double shaped = std::sin(tri * kPi * 0.5);
    return static_cast<float>(shaped * env_);
  }
  bool active() const { return env_ > 1e-6; }
 private:
  double rate_ = 48000.0, phase_ = 0.0, t_ = 0.0, start_ = 100.0, end_ = 50.0, sweep_ = 0.99, decay_ = 0.999, sweep_level_ = 0.0, env_ = 0.0;
};

// --- blasts and shocks ----------------------------------------------------------------------------------

// Kinney and Graham: peak overpressure over ambient, and the positive phase's duration in ms per
// kg^(1/3), at the scaled distance z = R / W^(1/3) (m/kg^(1/3)).
inline double kinney_overpressure_ratio(double z) {
  const double num = 808.0 * (1.0 + (z / 4.5) * (z / 4.5));
  const double den = std::sqrt(1.0 + (z / 0.048) * (z / 0.048)) * std::sqrt(1.0 + (z / 0.32) * (z / 0.32)) * std::sqrt(1.0 + (z / 1.35) * (z / 1.35));
  return num / den;
}
inline double kinney_duration_ms(double z) {
  const double num = 980.0 * (1.0 + std::pow(z / 0.54, 10.0));
  const double den = (1.0 + std::pow(z / 0.02, 3.0)) * (1.0 + std::pow(z / 0.74, 6.0)) * std::sqrt(1.0 + (z / 6.9) * (z / 6.9));
  return num / den;
}
// Whitham's N-wave duration, seconds, for a body of diameter d and length l at Mach m, heard r away.
inline double whitham_duration_s(double mach, double diameter_m, double length_m, double r_m, double c = 343.0) {
  mach = std::max(mach, 1.05);
  return 1.82 * mach * std::pow(r_m, 0.25) * diameter_m / (c * std::pow(mach * mach - 1.0, 0.375) * std::pow(length_m, 0.25));
}

// One pressure event in flight: a Friedlander blast or an N-wave, arriving `delay` samples from now.
struct Shock {
  enum class Kind : uint8_t { Friedlander, NWave } kind = Kind::Friedlander;
  double t = 0.0;         // samples since arrival (negative: not yet arrived)
  double length = 100.0;  // positive duration (Friedlander) or whole length (N-wave), samples
  double amp = 1.0;
  double b = 1.5;         // Friedlander's decay coefficient
  int path = 0;           // which air filter it goes through (0 direct, 1 ground, 2 terrain)
};

// A set of shocks, each through the air of its path.
class ShockField {
 public:
  static constexpr int kMax = 96;
  static constexpr int kPaths = 3;
  void prepare(double rate) { rate_ = rate; reset(); }
  void reset() { count_ = 0; for (auto& s : lp_) s = 0.0f; for (auto& s : lp2_) s = 0.0f; }
  // The air's loss over a path of r metres becomes a two-pole low-pass at the frequency where
  // ISO 9613-1 takes 6 dB off (spreading is in the amplitude).
  void set_path(int path, double r_m, double celsius = 15.0, double rh = 0.6) {
    double lo = 20.0, hi = std::min(20000.0, rate_ * 0.45);
    for (int i = 0; i < 40; ++i) {
      const double mid = std::sqrt(lo * hi);
      if (air::absorption_db_per_m(mid, celsius, rh) * r_m > 6.0) hi = mid; else lo = mid;
    }
    pole_[static_cast<size_t>(path)] = static_cast<float>(std::exp(-kTau * std::sqrt(lo * hi) / rate_));
  }
  void add(const Shock& s) { if (count_ < kMax) shocks_[static_cast<size_t>(count_++)] = s; }
  float process() {
    std::array<float, kPaths> in{};
    for (int i = 0; i < count_;) {
      Shock& s = shocks_[static_cast<size_t>(i)];
      s.t += 1.0;
      double p = 0.0;
      if (s.t > 0.0) {
        if (s.kind == Shock::Kind::Friedlander) {
          const double x = s.t / s.length;
          p = s.amp * (1.0 - x) * std::exp(-s.b * x);
          if (x > 12.0) { shocks_[static_cast<size_t>(i)] = shocks_[static_cast<size_t>(--count_)]; continue; }
        } else {
          const double x = s.t / s.length;
          if (x >= 1.0) { shocks_[static_cast<size_t>(i)] = shocks_[static_cast<size_t>(--count_)]; continue; }
          p = s.amp * (1.0 - 2.0 * x);
        }
      }
      in[static_cast<size_t>(std::clamp(s.path, 0, kPaths - 1))] += static_cast<float>(p);
      ++i;
    }
    float out = 0.0f;
    for (size_t k = 0; k < kPaths; ++k) {
      lp_[k] = in[k] + (lp_[k] - in[k]) * pole_[k];
      lp2_[k] = lp_[k] + (lp2_[k] - lp_[k]) * pole_[k];
      out += lp2_[k];
    }
    return out;
  }
  bool active() const {
    if (count_ > 0) return true;
    for (size_t k = 0; k < kPaths; ++k) if (std::abs(lp2_[k]) > 1e-6f) return true;
    return false;
  }
  int count() const { return count_; }
 private:
  std::array<Shock, kMax> shocks_{};
  std::array<float, kPaths> lp_{}, lp2_{}, pole_{0.0f, 0.0f, 0.0f};
  int count_ = 0;
  double rate_ = 48000.0;
};

// --- the lip-buzzed horn -------------------------------------------------------------------------------

// The lips locked to the horn's resonance: once a cycle they part and slap shut, so the flow
// through them is a train of pulses (the opening's area, a clipped half-wave whose sharp closure
// carries the brass's high harmonics, rougher at higher breath pressure), heard through the
// conical horn's input-impedance peaks, harmonic for a cone, and its bell, which radiates the
// high harmonics better than the low.
class LipHorn {
 public:
  static constexpr int kPeaks = 12;
  void prepare(double rate) { rate_ = rate; reset(); }
  void reset() { phase_ = 0.0; pressure_ = 0.0; target_ = 0.0; bell_lp_ = 0.0f; for (auto& r : peaks_) r.reset(); }
  void tune(double hz) {
    hz_ = std::clamp(hz, 45.0, 1200.0);
    for (int n = 0; n < kPeaks; ++n) peaks_[static_cast<size_t>(n)].set(hz_ * (n + 1), 14.0 + 2.0 * n, rate_);
  }
  void blow(double pressure) { target_ = pressure; }
  float process(Rng& rng) {
    pressure_ += (target_ - pressure_) * (target_ > pressure_ ? 0.004 : 0.0015);
    if (pressure_ < 1e-4) return 0.0f;
    // The lips' pitch wanders a little with the breath.
    phase_ += hz_ * (1.0 + 0.004 * rng.bipolar()) / rate_;
    if (phase_ >= 1.0) phase_ -= 1.0;
    // Open for the part of the cycle the breath can hold them apart; shut hard.
    const double open_share = std::clamp(0.35 + 0.25 * pressure_, 0.2, 0.7);
    const double x = phase_ / open_share;
    const double area = x < 1.0 ? std::pow(std::sin(kPi * x), 0.6) : 0.0;
    const double flow = area * std::sqrt(pressure_) * (1.0 + 0.15 * pressure_ * rng.bipolar());
    const float u = static_cast<float>(flow - open_share * 0.6 * std::sqrt(pressure_));
    float y = 0.0f;
    for (int n = 0; n < kPeaks; ++n) y += peaks_[static_cast<size_t>(n)].process(u) * static_cast<float>(1.0 / (1.0 + 0.15 * n));
    const float radiated = y - bell_lp_;
    bell_lp_ += 0.08f * (y - bell_lp_);
    return radiated * 3.0f;
  }
  bool active() const { return pressure_ > 1e-4 || target_ > 0.0; }
 private:
  std::array<Resonator, kPeaks> peaks_{};
  double rate_ = 48000.0, hz_ = 233.0, phase_ = 0.0, pressure_ = 0.0, target_ = 0.0;
  float bell_lp_ = 0.0f;
};

// --- a source passing by ------------------------------------------------------------------------------------

// A jet flying past at `speed` m/s, `closest` metres away at its closest, heard on the ground: the
// fan's blade-passing whine and its buzz-saw harmonics, the jet's mixing roar peaking at a
// Strouhal number of 0.2 (aft-directed: loudest after it has passed), Doppler-shifted, falling as
// 1/r, filtered by the air over r.
class Flyby {
 public:
  void prepare(double rate) { rate_ = rate; roar_.prepare(rate); roar2_.prepare(rate); reset(); }
  void reset() { t_ = 1e9; phase_.fill(0.0); lp_ = lp2_ = 0.0f; }
  void start(double speed, double closest, double blade_hz, double duration_s) {
    speed_ = speed; closest_ = closest; blade_ = blade_hz;
    // Emission time runs from -duration/2 to +duration/2 around the closest point.
    te_ = -duration_s * 0.5; t_ = 0.0; end_ = duration_s * 0.5;
  }
  float process(Rng& rng) {
    if (te_ > end_ + 2.0 || t_ > 1e8) return 0.0f;
    const double c = 343.0;
    const double x = speed_ * te_;
    const double r = std::sqrt(x * x + closest_ * closest_);
    const double rdot = speed_ * x / r;            // dr/dt_e
    const double doppler = 1.0 / (1.0 + rdot / c);  // received / emitted frequency
    te_ += 1.0 / rate_ * doppler;
    t_ += 1.0;
    // Directivity: cos of the angle from the flight direction to the listener.
    const double cos_theta = -x / r;  // +1 ahead, -1 behind
    const double fan = 0.5 * (1.0 + cos_theta) + 0.15;  // the inlet's whine radiates forward
    const double jet = 0.3 + 0.7 * std::pow(0.5 * (1.0 - cos_theta), 2.0);  // the exhaust aft
    const double spread = closest_ / r;
    float whine = 0.0f;
    for (int h = 0; h < 4; ++h) {
      double& p = phase_[static_cast<size_t>(h)];
      const double hz = (h == 0 ? blade_ : h == 1 ? blade_ * 2.0 : blade_ * (0.12 * h)) * doppler;
      p += hz / rate_; if (p >= 1.0) p -= 1.0;
      const double g = h == 0 ? 0.5 : h == 1 ? 0.2 : 0.18;
      whine += static_cast<float>(g * std::sin(kTau * p));
    }
    roar_.set(220.0 * doppler, 0.5);
    roar2_.set(1800.0 * doppler, 0.5);
    const float n = rng.bipolar();
    const float roar = roar_.low(n) * 1.6f + roar2_.band(n) * 0.5f;
    float y = static_cast<float>(spread * (fan * whine * 0.6 + jet * roar));
    // The air over r: the cutoff where ISO 9613-1 takes 6 dB.
    const double cutoff = std::clamp(9000.0 * std::pow(100.0 / r, 0.6), 400.0, rate_ * 0.45);
    const float pole = static_cast<float>(std::exp(-kTau * cutoff / rate_));
    lp_ = y + (lp_ - y) * pole;
    lp2_ = lp_ + (lp2_ - lp_) * pole;
    return lp2_;
  }
  bool active() const { return te_ <= end_ + 2.0 && t_ < 1e8; }
 private:
  StateVariable roar_, roar2_;
  std::array<double, 4> phase_{};
  double rate_ = 48000.0, t_ = 1e9, te_ = 0.0, end_ = 0.0, speed_ = 120.0, closest_ = 150.0, blade_ = 2400.0;
  float lp_ = 0.0f, lp2_ = 0.0f;
};

}  // namespace bonkr::dsp::src
