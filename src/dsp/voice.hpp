// One sounding instrument: a drum, a cymbal, a shaker, a circuit, a cannon. Every sound is built
// at the note from its physical description (the head's diameter, tension and areal density, the
// cymbal's radius, thickness and bronze, the charge in kilograms and the distance in metres) by
// the recipes below, out of the parts in modal.hpp and sources.hpp. A drum struck again while it
// still rings is struck again: the new stroke adds to the vibration already there.
//
// Never allocates after prepare(); everything a recipe needs is sized there.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "catalog.hpp"
#include "core.hpp"
#include "filters.hpp"
#include "modal.hpp"
#include "sources.hpp"

namespace bonkr::dsp {

// Who strikes, as the Striker parameter names them. AsPlayed leaves each sound its own.
enum StrikerKind : int { kAsPlayed, kStick, kFeltMallet, kHand, kBrush, kRods, kHardMallet, kStrikerCount };

// How one note is played and what the player has set.
struct Play {
  double velocity = 0.7;   // 0..1
  double dynamics = 1.0;   // how far velocity reaches the stroke's speed
  double tune = 1.0;       // frequency ratio: kit tuning, the Tune knob, the key in chromatic play
  double decay = 1.0;      // multiplies every decay time
  double damping = 0.0;    // a hand, a felt, a pillow: 0 free .. 1 dead
  double position = 0.0;   // -1 the centre .. +1 the edge, added to the sound's own spot
  int striker = kAsPlayed;
  double hardness = 1.0;   // multiplies the striker's contact time (lower is harder)
  double tension = 1.0;    // multiplies the tension modulation (the pitch drop of a hard stroke)
  double snares = 1.0;     // snare wires: 0 off .. 1 on
  double snare_tension = 0.5;
  double resonance = 0.5;  // the resonant head, the shell, the body: 0 dead .. 1 ringing
  double celsius = 20.0;   // the air outdoors: the speed of sound, and what the air absorbs over
  double humidity = 0.45;  // a blast's, a storm's or a jet's long path (relative, 0..1)
  uint32_t seed = 1;
};

// The stroke's speed in m/s from the velocity: 0.4 m/s (ppp) to 12 m/s (fff), exponentially, as a
// player's stick speeds are spread; with no dynamics every stroke is 2.2 m/s.
inline double stroke_speed(double velocity, double dynamics) {
  const double v = 0.4 * std::pow(30.0, std::clamp(velocity, 0.0, 1.0));
  return std::exp(dynamics * std::log(v) + (1.0 - dynamics) * std::log(2.2));
}

inline modal::Striker striker_of(int kind) {
  modal::Striker s;
  switch (kind) {
    case kFeltMallet: s.alpha = 2.8; s.contact_ms = 3.0; s.restitution = 0.4; break;
    case kHand: s.alpha = 3.0; s.contact_ms = 4.5; s.restitution = 0.3; break;
    case kBrush: s.alpha = 1.5; s.contact_ms = 0.35; s.restitution = 0.3; s.bristles = 18; s.spread_ms = 9.0; break;
    case kRods: s.alpha = 1.5; s.contact_ms = 0.6; s.restitution = 0.5; s.bristles = 7; s.spread_ms = 1.8; break;
    case kHardMallet: s.alpha = 2.0; s.contact_ms = 1.4; s.restitution = 0.5; break;
    default: s.alpha = 1.5; s.contact_ms = 0.8; s.restitution = 0.6; break;  // a hickory stick
  }
  return s;
}

class Voice {
 public:
  void prepare(double rate) {
    rate_ = rate;
    for (auto& c : contact_) c.prepare(rate);
    for (auto& b : bank_) b.prepare(rate);
    for (auto& n : noise_) n.prepare(rate);
    particles_.prepare(rate);
    wires_.prepare(rate);
    squares_.prepare(rate);
    for (auto& o : osc_) o.prepare(rate);
    shocks_.prepare(rate);
    horn_.prepare(rate);
    flyby_.prepare(rate);
    for (auto& f : sq_filter_) f.prepare(rate);
    for (auto& f : tone_) f.prepare(rate);
    kill();
  }
  void kill() {
    sound_ = -1; awake_ = false; held_ = false; level_ = 0.0f;
    for (auto& c : contact_) c.reset();
    for (auto& b : bank_) b.clear();
    for (auto& n : noise_) n.reset();
    particles_.reset(); wires_.reset(); shocks_.reset(); horn_.reset(); flyby_.reset();
    for (auto& o : osc_) o.reset();
    for (auto& f : sq_filter_) f.reset();
    for (auto& f : tone_) f.reset();
    sq_env_ = 0.0; sq_env2_ = 0.0; pattern_ = Pattern{}; siren_rps_ = siren_target_ = 0.0; siren_phase_ = 0.0;
    hp_state_ = lp_state_ = lp2_state_ = 0.0f; quiet_ = 0; age_ = 0;
    bank_out_ = {1.0f, 1.0f};
  }

  int sound() const { return sound_; }
  bool awake() const { return awake_; }
  bool held() const { return held_; }
  uint64_t age() const { return age_; }
  int key = -1;          // the key that started it (the engine's bookkeeping)
  int choke_group = 0;   // 0: none
  float pan = 0.0f;      // -1 .. 1
  float gain = 1.0f;     // the kit's level for this voice times the sound's trim

  // Whether striking this sound again while it rings excites the same body.
  static bool restrikes(int s) {
    switch (family(s)) {
      case Family::Membrane: case Family::Cymbal: case Family::Bar: case Family::Bell: case Family::Junk: return s != kGlassSmash && s != kMetalPipe && s != kSnareBrushSweep;
      default: return false;
    }
  }
  // Sounds that last while the key is held.
  static bool is_held(int s) {
    switch (s) {
      case kSnareBrushSweep: case kCymbalSwell: case kCastanetRoll: case kRatchet: case kTelephone: case kWashingMachine:
      case kVuvuzela: case kSiren: case kMachineGun: return true;
      default: return false;
    }
  }

  void start(int s, const Play& p) {
    kill();
    sound_ = s; play_ = p; awake_ = true; held_ = is_held(s);
    rng_.seed(p.seed);
    speed_ = stroke_speed(p.velocity, p.dynamics);
    build(p);
    strike(p, true);
  }
  void restrike(const Play& p) {
    play_.velocity = p.velocity; play_.seed = p.seed;
    speed_ = stroke_speed(p.velocity, p.dynamics);
    rng_.seed(p.seed);
    strike(p, false);
    awake_ = true; quiet_ = 0;
  }
  void release() {
    if (!held_) return;
    held_ = false;
    pattern_.running = false;
    switch (sound_) {
      case kVuvuzela: horn_.blow(0.0); break;
      case kSiren: siren_target_ = 0.0; break;
      case kSnareBrushSweep: noise_[0].set_decay(rate_, 120.0); break;
      default: break;
    }
  }
  // Another voice in this one's group was struck (the hi-hat closing on its open self).
  void choke() {
    for (auto& b : bank_) b.damp(0.02);
    for (auto& n : noise_) n.set_decay(rate_, 25.0);
    pattern_.running = false;
    held_ = false;
  }

  float process() {
    if (!awake_) return 0.0f;
    ++age_;
    if (pattern_.running || pattern_.pending > 0) run_pattern();
    const float f0 = contact_[0].next(), f1 = contact_[1].next();
    float y = 0.0f;
    if (bank_[0].count()) y += bank_[0].process(f0) * bank_out_[0];
    if (bank_[1].count()) y += bank_[1].process(couple_ ? f0 * couple_gain_ : f1) * bank_out_[1];
    // Noise sources and what drives them.
    for (size_t i = 0; i < noise_.size(); ++i) {
      switch (noise_drive_[i]) {
        case Drive::Force: noise_[i].drive(std::abs(i == 1 && f1 != 0.0f ? f1 : f0) * noise_k_[i]); break;
        case Drive::Cascade: {
          const double a = bank_[0].amplitude();
          noise_[i].follow(noise_k_[i] * a * a, follow_[i]);
          break;
        }
        default: break;
      }
      float v = noise_[i].process(rng_);
      if (i == 2 && tail_air_pole_ > 0.0f) {
        tail_lp_[0] = v + (tail_lp_[0] - v) * tail_air_pole_;
        tail_lp_[1] = tail_lp_[0] + (tail_lp_[1] - tail_lp_[0]) * tail_air_pole_;
        v = tail_lp_[1];
      }
      y += v;
    }
    if (wires_on_) y += wires_.process(bank_[wire_bank_].displacement() * wire_scale_, rng_);
    if (particles_on_) y += particles_.process(rng_) * particle_gain_;
    if (squares_on_) y += metal();
    if (osc_[0].active()) y += osc_[0].process() * osc_gain_[0];
    if (osc_[1].active()) y += osc_[1].process() * osc_gain_[1];
    if (shocks_on_) y += shocks_.process() * shock_gain_;
    if (horn_on_) y += horn_.process(rng_) * 0.8f;
    if (flyby_on_) y += flyby_.process(rng_);
    if (siren_on_) y += siren();
    // The voice's tone: what the recipe's air, shell or circuit leaves of it.
    if (hp_pole_ > 0.0f) { hp_state_ = y + (hp_state_ - y) * hp_pole_; y -= hp_state_; }
    if (lp_pole_ > 0.0f) { lp_state_ = y + (lp_state_ - y) * lp_pole_; lp2_state_ = lp_state_ + (lp2_state_ - lp_state_) * lp_pole_; y = lp2_state_; }
    if (drive_ > 0.0f) y = std::tanh(y * drive_) / std::tanh(drive_);
    if (bits_ > 0) {
      const float steps = static_cast<float>((1 << (bits_ - 1)) - 1);
      y = std::round(std::clamp(y * 2.0f, -1.0f, 1.0f) * steps) / steps * 0.5f;
    }
    // Sleep when everything has died away (and nothing is held or scheduled).
    const float mag = std::abs(y);
    level_ = std::max(mag, level_ * 0.9995f);
    if (mag < 1e-5f && !held_ && !pattern_.running && pattern_.pending == 0 && !contact_[0].active() && !contact_[1].active() && !flyby_.active() && !(shocks_on_ && shocks_.count() > 0)) {
      if (++quiet_ > static_cast<int>(rate_ * 0.05) && level_ < 2e-5f) awake_ = false;
    } else {
      quiet_ = 0;
    }
    return y;
  }
  float level() const { return level_; }
  // The first body's fundamental as it sounds now (after the stroke's glide), for the editor.
  double sounding_hz() const { return bank_[0].count() ? bank_[0].hz(0) * bank_[0].glide() : 0.0; }
  const modal::ModeBank& bank(int i) const { return bank_[static_cast<size_t>(i)]; }

 private:
  enum class Drive : uint8_t { Trigger, Force, Cascade };

  // A repeating gesture: a ringer, a ratchet, a roll, a gun, a bouncing pipe, the claps of a crowd.
  struct Pattern {
    bool running = false;   // repeats while held
    int pending = 0;        // events still to come regardless of the key
    double next = 0.0;      // samples to the next event
    double interval = 0.0;  // samples between events
    double shrink = 1.0;    // each interval times this (a bounce)
    double jitter = 0.0;    // random share of the interval
    int count = 0;          // events so far
    double amp = 1.0;       // the next event's strength
    double amp_step = 1.0;  // times this per event
    double t = 0.0;         // samples since the gesture began
  };

  // --- building blocks ------------------------------------------------------------------------------

  struct Membrane {
    double diameter = 0.356, tension = 2700.0, sigma = modal::kHeadSingle10mil;
    double position = 0.35;     // strike radius as a share of the radius
    double t60 = 0.8;           // the fundamental's decay, s
    double loss = 0.7;          // decay time falls as (f01 / f)^loss
    double breath_t60 = 0.0;    // the (0,1) mode's own decay when it radiates fast (0: as the rest)
    int max_modes = 40;
    double max_ratio = 16.0;
    double beta = 0.15;         // tension modulation at a firm stroke
    double reso = 0.0;          // the other head's f01 against this one's (0: one head)
    double coupling = 0.35;     // the enclosed air's coupling of the two heads' breathing modes
  };
  void membrane(int b, const Membrane& m, const Play& p) {
    const double f01 = modal::membrane_f01(m.diameter, m.tension, m.sigma) * p.tune;
    const auto& table = modal::membrane_modes();
    std::array<modal::ModeSpec, modal::ModeBank::kMaxModes> spec{};
    int n = 0;
    const double pos = std::clamp(m.position + 0.5 * p.position, 0.0, 0.97);
    const double decay = p.decay * (1.0 - 0.85 * p.damping);
    for (const auto& mode : table) {
      if (n >= m.max_modes || n >= modal::ModeBank::kMaxModes - 2) break;
      const double ratio = mode.j / table[0].j;
      if (ratio > m.max_ratio) break;
      modal::ModeSpec s;
      s.hz = f01 * ratio;
      s.t60 = m.t60 * decay * std::pow(1.0 / ratio, m.loss);
      if (mode.m == 0 && mode.n == 1 && m.breath_t60 > 0.0) s.t60 = m.breath_t60 * decay;
      s.drive = modal::bessel_j(mode.m, mode.j * pos);
      s.radiate = modal::bessel_j(mode.m, mode.j * 0.55) / (1.0 + 0.1 * mode.m) * (mode.m == 0 ? 1.4 : 1.0);
      if (mode.m == 0 && mode.n == 1 && m.reso > 0.0) {
        // The two heads' breathing modes, coupled by the air between them, split into a pair:
        // the heads moving together (lower) and apart (higher).
        const double w1 = s.hz * s.hz, w2 = (f01 * m.reso) * (f01 * m.reso);
        const double k = m.coupling * std::sqrt(w1 * w2) * (0.4 + p.resonance);
        const double mean = 0.5 * (w1 + w2), half = std::sqrt(0.25 * (w1 - w2) * (w1 - w2) + k * k);
        const double lo = std::sqrt(mean - half), hi = std::sqrt(mean + half);
        const double share = 0.5 + 0.5 * (w1 - w2) / (2.0 * half + 1e-9);  // batter's share of the upper mode
        modal::ModeSpec a = s, c = s;
        a.hz = lo; a.drive = s.drive * std::sqrt(std::max(0.05, 1.0 - share)); a.radiate = s.radiate * 1.6;
        c.hz = hi; c.drive = s.drive * std::sqrt(std::max(0.05, share)); c.radiate = s.radiate * 0.6;
        a.t60 *= 0.8 + 0.6 * p.resonance;
        spec[static_cast<size_t>(n++)] = a; spec[static_cast<size_t>(n++)] = c;
        continue;
      }
      spec[static_cast<size_t>(n++)] = s;
    }
    normalise(spec.data(), n);
    bank_[static_cast<size_t>(b)].set(spec.data(), n);
    bank_[static_cast<size_t>(b)].set_nonlinearity(m.beta * p.tension);
  }

  // Modes from ratio tables (bells, blocks, timpani, the tabla): f0 times each ratio.
  void table(int b, double f0, const double* ratios, const double* t60s, const double* gains, int n, const Play& p, double beta = 0.0) {
    std::array<modal::ModeSpec, modal::ModeBank::kMaxModes> spec{};
    const double decay = p.decay * (1.0 - 0.85 * p.damping);
    for (int i = 0; i < n && i < modal::ModeBank::kMaxModes; ++i)
      spec[static_cast<size_t>(i)] = {f0 * ratios[i] * p.tune, t60s[i] * decay, gains[i], 1.0};
    normalise(spec.data(), n);
    bank_[static_cast<size_t>(b)].set(spec.data(), n);
    bank_[static_cast<size_t>(b)].set_nonlinearity(beta * p.tension);
  }

  struct Plate {
    double radius = 0.229, thickness = 0.0010;
    modal::Material material = modal::kBronzeB20;
    double curvature_hz = 180.0;  // the dome's and the profile's stiffening of the low modes
    double t60 = 4.0;             // at 1 kHz
    double loss = 0.45;           // decay time falls as (f / 1 kHz)^-loss
    double tilt = 0.0;            // how the stroke's spot weights the high modes against the low
    double low_hz = 70.0, high_hz = 17000.0;
    double irregularity = 0.04;   // hand hammering and lathing: each mode's random shift
    double beta = 0.0;
    int modes = 72;
  };
  void plate(int b, const Plate& pl, const Play& p, uint32_t seed) {
    // Every free-plate mode up to the top, then an even spread of them across log frequency.
    std::array<double, 600> all{};
    int count = 0;
    for (int m = 0; m <= 60 && count < 600; ++m)
      for (int k = 0; k <= 12 && count < 600; ++k) {
        if (m < 2 && k == 0) continue;  // rigid-body motion
        const double flat = modal::plate_hz(modal::free_plate_lambda2(m, k), pl.radius, pl.thickness, pl.material);
        const double hz = std::sqrt(flat * flat + pl.curvature_hz * pl.curvature_hz) * p.tune;
        if (hz >= pl.low_hz * p.tune && hz <= pl.high_hz) all[static_cast<size_t>(count++)] = hz;
      }
    std::sort(all.begin(), all.begin() + count);
    Rng rng(seed);
    std::array<modal::ModeSpec, modal::ModeBank::kMaxModes> spec{};
    const int want = std::min({pl.modes, count, modal::ModeBank::kMaxModes});
    const double decay = p.decay * (1.0 - 0.9 * p.damping);
    for (int i = 0; i < want; ++i) {
      const double hz = all[static_cast<size_t>(std::min(count - 1, static_cast<int>((i + 0.5) * count / want)))] * (1.0 + pl.irregularity * rng.bipolar());
      modal::ModeSpec s;
      s.hz = hz;
      s.t60 = pl.t60 * decay * std::min(1.5, std::pow(hz / 1000.0, -pl.loss));
      const double edge = std::clamp(0.5 + 0.5 * p.position, 0.0, 1.0);
      const double size = 0.5 + rng.unipolar();  // one draw per statement: one stream on every compiler
      const double sign = rng.next() & 1 ? 1.0 : -1.0;
      s.drive = std::pow(hz / 1000.0, -pl.tilt * (0.6 + 0.8 * edge)) * size * sign;
      // A thin plate radiates better as its bending waves speed up toward coincidence (some
      // 10 kHz for a millimetre of bronze): efficiency rising with frequency.
      s.radiate = std::sqrt(std::min(hz, 12000.0) / 1000.0);
      spec[static_cast<size_t>(i)] = s;
    }
    normalise(spec.data(), want);
    bank_[static_cast<size_t>(b)].set(spec.data(), want);
    bank_[static_cast<size_t>(b)].set_nonlinearity(pl.beta * p.tension);
  }

  // A free-free bar (a triangle's rod, a tube, a pipe): both bending planes, slightly apart.
  void bar(int b, double f1, int modes, double t60_1k, double loss, double strike_at, double radiate_slope, const Play& p, double split = 1.012) {
    std::array<modal::ModeSpec, modal::ModeBank::kMaxModes> spec{};
    int n = 0;
    const double decay = p.decay * (1.0 - 0.9 * p.damping);
    const double wc = 1.0;  // f_n = f1 (beta_n / beta_1)^2
    const double f1_rate = modal::free_bar_hz(1, 1.0, wc);
    for (int k = 1; k <= modes && n < modal::ModeBank::kMaxModes - 1; ++k) {
      const double hz = f1 * modal::free_bar_hz(k, 1.0, wc) / f1_rate * p.tune;
      if (hz > 0.46 * rate_) break;
      const double bl = k <= 5 ? std::sqrt(modal::free_bar_hz(k, 1.0, wc) * kTau) : (2.0 * k + 1.0) * kPi / 2.0;
      const double x = std::clamp(strike_at + 0.1 * p.position, 0.0, 1.0);
      // The free bar's mode shape, its standing-wave part (good away from the ends).
      const double shape = std::cos(bl * x - bl * 0.5) + 0.3;
      for (int plane = 0; plane < 2; ++plane) {
        modal::ModeSpec s;
        s.hz = hz * (plane ? split : 1.0);
        s.t60 = t60_1k * decay * std::pow(s.hz / 1000.0, -loss);
        s.drive = shape * (plane ? 0.6 : 1.0);
        s.radiate = std::pow(s.hz / 1000.0, radiate_slope);
        spec[static_cast<size_t>(n++)] = s;
      }
    }
    normalise(spec.data(), n);
    bank_[static_cast<size_t>(b)].set(spec.data(), n);
  }

  // A thin cylindrical shell (a can, a drum, a washing machine's tub).
  void shell(int b, double radius, double length, double thickness, const modal::Material& mat, double t60_1k, double low_hz, double high_hz, const Play& p, uint32_t seed) {
    std::array<double, 400> all{};
    int count = 0;
    for (int ax = 1; ax <= 6; ++ax)
      for (int c = 2; c <= 60 && count < 400; ++c) {
        const double hz = modal::shell_hz(ax, c, radius, length, thickness, mat) * p.tune;
        if (hz >= low_hz && hz <= high_hz) all[static_cast<size_t>(count++)] = hz;
      }
    std::sort(all.begin(), all.begin() + count);
    Rng rng(seed);
    std::array<modal::ModeSpec, modal::ModeBank::kMaxModes> spec{};
    const int want = std::min(count, 60);
    const double decay = p.decay * (1.0 - 0.9 * p.damping);
    for (int i = 0; i < want; ++i) {
      const double hz = all[static_cast<size_t>(std::min(count - 1, static_cast<int>((i + 0.5) * count / want)))];
      const double size = 0.4 + rng.unipolar();
      const double sign = rng.next() & 1 ? 1.0 : -1.0;
      spec[static_cast<size_t>(i)] = {hz, t60_1k * decay * std::pow(hz / 1000.0, -0.4), size * sign * std::pow(hz / 1000.0, -0.3), 1.0};
    }
    normalise(spec.data(), want);
    bank_[static_cast<size_t>(b)].set(spec.data(), want);
  }

  // Scales the strike shapes so the first modes' drives sum to one: a firm stroke then moves the
  // body by about 1 in the units the tension modulation is stated in.
  static void normalise(modal::ModeSpec* s, int n) {
    double sum = 0.0;
    for (int i = 0; i < std::min(n, 4); ++i) sum += std::abs(s[i].drive);
    if (sum < 1e-6) { for (int i = 0; i < n; ++i) sum = std::max(sum, std::abs(s[i].drive)); }
    if (sum < 1e-9) return;
    for (int i = 0; i < n; ++i) s[i].drive /= sum;
  }

  // A noise band. Struck (Trigger): it rises over attack_ms to `level` and dies in decay_ms.
  // Driven by the contact force (Force): k is its gain, and it dies in decay_ms after the force.
  // Fed by the body's cascade (Cascade): its amplitude follows k A^2 (A the low modes'
  // amplitude) with the time constant decay_ms, rising after the stroke and falling with the body.
  void noise(int i, src::NoiseBand::Shape shape, double hz, double q, double attack_ms, double decay_ms, double level, Drive drive = Drive::Trigger, double k = 1.0, bool steep = false) {
    const size_t n = static_cast<size_t>(i);
    const bool cascade = drive == Drive::Cascade;
    noise_[n].set(shape, std::min(hz, rate_ * 0.45), q, attack_ms, cascade ? 1e9 : decay_ms * play_.decay, drive == Drive::Trigger ? level : 1.0, steep);
    noise_drive_[n] = drive;
    noise_k_[n] = k;
    follow_[n] = cascade ? 1.0 / std::max(1.0, decay_ms * 0.001 * rate_) : 0.0;
  }
  void tone(double hp_hz, double lp_hz) {
    hp_pole_ = hp_hz > 0.0 ? static_cast<float>(std::exp(-kTau * hp_hz / rate_)) : 0.0f;
    lp_pole_ = lp_hz > 0.0 && lp_hz < rate_ * 0.45 ? static_cast<float>(std::exp(-kTau * lp_hz / rate_)) : 0.0f;
  }
  modal::Striker striker(int own, double body) const {
    modal::Striker s = striker_of(play_.striker == kAsPlayed ? own : play_.striker);
    s.contact_ms *= body * play_.hardness;
    return s;
  }
  double impulse(const modal::Striker& s, double scale = 1.0) const { return speed_ / modal::kReferenceSpeed * (1.0 + s.restitution) / 1.6 * scale; }
  void hit(int c, const modal::Striker& s, double scale = 1.0, double delay = 0.0) {
    contact_[static_cast<size_t>(c)].strike(s, speed_ * std::sqrt(scale), impulse(s, scale), rng_.next(), delay);
  }

  // The day's air: the speed of sound, and ISO 9613-1's absorption over a path of r metres.
  double sound_speed() const { return air::speed_of_sound(play_.celsius); }
  void air_path(int path, double r) { shocks_.set_path(path, r, play_.celsius, play_.humidity); }

  // A blast heard R metres away, with its ground reflection, the terrain's echoes and the
  // scattered tail: the many small reflections from trees, walls and the ground around, which
  // arrive as a diffuse rumble dying over `tail_s` (a 60 dB time), centred near `tail_hz`.
  void blast(double kg, double r, int echoes, double echo_spread_s, double amp = 1.0, double delay_s = 0.0, double tail_s = 1.5, double tail_hz = 500.0, double tail_level = 0.12) {
    kg = std::max(1e-6, kg / (play_.tune * play_.tune * play_.tune));
    const double z = r / std::cbrt(kg);
    const double ps = src::kinney_overpressure_ratio(z);
    const double td = std::max(0.05, src::kinney_duration_ms(z) * std::cbrt(kg)) * 0.001 * rate_;
    // Peak pressure, compressed to a level the kit can sit with (the full range of blasts is
    // 80 dB): the square root keeps their order.
    const double level = amp * std::sqrt(ps) * 8.0;
    src::Shock s;
    s.length = td; s.amp = level; s.b = 1.2 + 0.4 * std::min(1.0, z / 20.0); s.t = -delay_s * rate_;
    shocks_.add(s);
    const double hs = 1.5, hr = 1.5;
    const double extra = std::sqrt(r * r + (hs + hr) * (hs + hr)) - r;
    s.t -= extra / sound_speed() * rate_ + 1.0; s.amp = level * 0.6; s.path = 1;
    shocks_.add(s);
    for (int e = 0; e < echoes; ++e) {
      src::Shock echo = s;
      echo.path = 2;
      echo.t = -(delay_s + 0.25 + echo_spread_s * rng_.unipolar()) * rate_;
      echo.amp = level * (0.08 + 0.18 * rng_.unipolar()) * (1.0 - 0.5 * e / std::max(1, echoes));
      echo.length = td * (1.5 + 2.0 * rng_.unipolar());  // a distant echo arrives smeared
      shocks_.add(echo);
    }
    air_path(0, r); air_path(1, r + extra); air_path(2, r + 600.0);
    shocks_on_ = true;
    if (tail_level > 0.0) {
      noise(2, src::NoiseBand::Shape::Band, tail_hz, 0.9, 12.0 + 1000.0 * delay_s, tail_s * 1000.0, level * tail_level);
      noise_[2].trigger(1.0);
      // The scattered sound comes the long way round (out to what it scatters from and back, a few
      // times the distance, up to the terrain some 600 m off) and loses the air's treble over it.
      tail_air_pole_ = static_cast<float>(std::exp(-kTau * src::air_cutoff_hz(r + std::min(600.0, 2.0 * r + 40.0), play_.celsius, play_.humidity, rate_) / rate_));
    }
  }
  void nwave(double seconds, double amp, double delay_s = 0.0, int path = 0) {
    src::Shock s;
    s.kind = src::Shock::Kind::NWave;
    s.length = std::max(2.0, seconds * rate_);
    s.amp = amp; s.t = -delay_s * rate_; s.path = path;
    shocks_.add(s);
    shocks_on_ = true;
  }

  // A gesture of `pending` events (or endless while held), `seconds` apart; the first comes after
  // `first_s`, or one interval after the stroke when that is negative.
  void every(double seconds, bool running, int pending, double jitter = 0.0, double first_s = -1.0) {
    pattern_.running = running; pattern_.pending = pending;
    pattern_.interval = std::max(1.0, seconds * rate_);
    pattern_.next = (first_s < 0.0 ? seconds : first_s) * rate_;
    pattern_.jitter = jitter; pattern_.count = 0; pattern_.amp = 1.0; pattern_.amp_step = 1.0; pattern_.shrink = 1.0; pattern_.t = 0.0;
  }

  // --- the recipes ----------------------------------------------------------------------------------

  void build(const Play& p) {
    using Shape = src::NoiseBand::Shape;
    couple_ = false; couple_gain_ = 1.0f; wires_on_ = particles_on_ = squares_on_ = shocks_on_ = horn_on_ = flyby_on_ = siren_on_ = false;
    noise_drive_.fill(Drive::Trigger); noise_k_.fill(1.0);
    for (auto& n : noise_) n.set(Shape::Band, 1000.0, 1.0, 1.0, 1.0, 0.0);
    particle_gain_ = 1.0f; shock_gain_ = 1.0f; osc_gain_ = {0.0f, 0.0f};
    hp_pole_ = lp_pole_ = 0.0f; drive_ = 0.0f; bits_ = 0; wire_bank_ = 0; wire_scale_ = 1.0;
    tail_air_pole_ = 0.0f; tail_lp_ = {0.0f, 0.0f};
    tone(20.0, 0.0);
    const uint32_t body_seed = 0x5eedu + static_cast<uint32_t>(sound_) * 7919u;  // a body is the same every stroke
    // A shock wave's push is mostly below 20 Hz; what is heard of it is what a microphone (and an
    // ear) takes: from about 40 Hz up.
    if (family(sound_) == Family::Blast) tone(40.0, 0.0);
    switch (sound_) {
      // --- kicks -------------------------------------------------------------------------------
      case kKickStudio: case kKickVintage: case kKickJazz: case kKickDeep: {
        Membrane m;
        m.sigma = modal::kHeadTwoPly7mil;
        if (sound_ == kKickStudio) { m.diameter = 0.559; m.tension = 1350.0; m.t60 = 0.45; m.reso = 1.15; m.beta = 0.10; }
        if (sound_ == kKickVintage) { m.diameter = 0.559; m.tension = 1050.0; m.t60 = 1.2; m.reso = 1.05; m.beta = 0.14; m.sigma = modal::kHeadSingle10mil; }
        if (sound_ == kKickJazz) { m.diameter = 0.457; m.tension = 1500.0; m.t60 = 1.0; m.reso = 1.0; m.beta = 0.12; m.sigma = modal::kHeadSingle10mil; }
        if (sound_ == kKickDeep) { m.diameter = 0.660; m.tension = 820.0; m.t60 = 1.4; m.reso = 1.1; m.beta = 0.16; }
        m.position = 0.25; m.loss = 0.9; m.max_ratio = 9.0;
        membrane(0, m, p);
        // The beater on the head: the head's high modes beyond the bank, heard as the click.
        noise(0, Shape::Band, sound_ == kKickJazz ? 2200.0 : 3000.0, 0.5, 0.2, 25.0, 0.0, Drive::Force, 4.0);
        noise(1, Shape::Band, 700.0, 0.7, 0.2, 50.0, 0.0, Drive::Force, 6.0);
        tone(25.0, 0.0);
        break;
      }
      // --- snares -------------------------------------------------------------------------------
      case kSnareMaple: case kSnareBrass: case kSnareFat: case kSnareRimshot: case kSnareCrossStick: case kSnareBrush: case kSnareBrushSweep: {
        Membrane m;
        m.diameter = 0.356; m.tension = 4000.0; m.t60 = 0.55; m.reso = 1.45; m.beta = 0.10; m.loss = 0.6;
        if (sound_ == kSnareBrass) { m.diameter = 0.330; m.tension = 4600.0; m.t60 = 0.6; }
        if (sound_ == kSnareFat) { m.tension = 1500.0; m.t60 = 0.35; m.sigma = modal::kHeadTwoPly7mil; }
        if (sound_ == kSnareRimshot) m.position = 0.85;
        if (sound_ == kSnareCrossStick) { m.position = 0.95; m.t60 = 0.2; }
        membrane(0, m, p);
        wires_on_ = true;
        wires_.configure(p.snare_tension, p.snares * (sound_ == kSnareCrossStick ? 0.12 : 1.0), (sound_ == kSnareFat ? 70.0 : 45.0) * p.decay);
        wire_scale_ = 1.2;
        // The stick on the head, and the dense modes of the head and shell above the bank.
        noise(0, Shape::Band, 2500.0, 0.6, 0.1, 12.0, 0.0, Drive::Force, 3.0);
        noise(2, Shape::Band, 750.0, 0.6, 0.3, 70.0, 0.0, Drive::Force, 14.0);
        if (sound_ == kSnareRimshot || sound_ == kSnareCrossStick) {
          // The stick across the hoop: the shell and the rim ring, sharp and woody.
          static constexpr double kR[] = {1.0, 1.47, 2.09, 2.56, 3.39, 4.1};
          static constexpr double kT[] = {0.09, 0.07, 0.06, 0.05, 0.04, 0.03};
          static constexpr double kG[] = {1.0, 0.8, 0.9, 0.6, 0.5, 0.4};
          table(1, sound_ == kSnareRimshot ? 820.0 : 560.0, kR, kT, kG, 6, p);
          bank_out_[1] = sound_ == kSnareRimshot ? 1.2f : 2.0f;
          noise(1, Shape::High, 1800.0, 0.7, 0.1, 18.0, 0.0, Drive::Force, sound_ == kSnareRimshot ? 1.3 : 0.15);
          if (sound_ == kSnareCrossStick) {  // the stick lies on the head: the hoop and shell ring longer
            static constexpr double kTx[] = {0.16, 0.12, 0.1, 0.08, 0.06, 0.05};
            table(1, 560.0, kR, kTx, kG, 6, p);
          }
        }
        if (sound_ == kSnareBrushSweep) {
          noise(0, Shape::Band, 3500.0, 0.5, 220.0, 6000.0, 0.35);
          every(0.004, true, 0, 0.8);
        }
        break;
      }
      // --- toms ----------------------------------------------------------------------------------
      case kTomHigh: case kTomMid: case kTomFloor: case kTomFloorLow: {
        Membrane m;
        m.sigma = modal::kHeadTwoPly7mil; m.reso = 1.08; m.loss = 0.75; m.beta = 0.16;
        if (sound_ == kTomHigh) { m.diameter = 0.254; m.tension = 1220.0; m.t60 = 1.0; }
        if (sound_ == kTomMid) { m.diameter = 0.305; m.tension = 1130.0; m.t60 = 1.2; }
        if (sound_ == kTomFloor) { m.diameter = 0.356; m.tension = 965.0; m.t60 = 1.5; }
        if (sound_ == kTomFloorLow) { m.diameter = 0.406; m.tension = 935.0; m.t60 = 1.8; }
        membrane(0, m, p);
        noise(0, Shape::Band, 1800.0, 0.5, 0.1, 30.0, 0.0, Drive::Force, 6.0);
        break;
      }
      // --- hi-hats and cymbals ----------------------------------------------------------------------
      case kHatClosed: case kHatPedal: case kHatOpen: case kHatHalf: {
        Plate pl; pl.radius = 0.178; pl.thickness = 0.0011; pl.curvature_hz = 260.0; pl.low_hz = 150.0;
        pl.t60 = sound_ == kHatOpen ? 3.5 : sound_ == kHatHalf ? 0.6 : sound_ == kHatPedal ? 0.18 : 0.3;
        pl.loss = 0.3;
        plate(0, pl, p, body_seed);
        // The cascade into the top octave, and the two plates' clatter against each other.
        noise(0, Shape::High, 8000.0, 0.7, 0.1, 3.0, 0.0, Drive::Cascade, 0.5);
        noise(1, Shape::Band, sound_ == kHatPedal ? 8000.0 : 9000.0, 0.6, 0.3, sound_ == kHatPedal ? 70.0 : 45.0, sound_ == kHatPedal ? 2.5 : 1.2, Drive::Trigger);
        if (sound_ == kHatHalf) noise(2, Shape::Band, 6500.0, 1.2, 1.0, 500.0, 0.25);
        if (sound_ == kHatPedal) noise(2, Shape::Band, 300.0, 0.8, 4.0, 50.0, 10.0);  // the plates' "chick" as they close
        tone(250.0, 0.0);
        break;
      }
      case kCrash16: case kCrash18: case kRide: case kRideBell: case kChina: case kSplash: case kCymbalSwell: {
        Plate pl;
        double wash = 0.18, bloom_ms = 8.0, wash_hz = 6000.0;  // bloom: the cascade's time constant
        switch (sound_) {
          case kCrash16: pl.radius = 0.203; pl.thickness = 0.0009; pl.t60 = 2.6; break;
          case kCrash18: case kCymbalSwell: pl.radius = 0.229; pl.thickness = 0.0010; pl.t60 = 3.4; break;
          case kRide: pl.radius = 0.254; pl.thickness = 0.0016; pl.t60 = 6.0; pl.tilt = 0.6; wash = 0.04; bloom_ms = 12.0; break;
          case kRideBell: pl.radius = 0.254; pl.thickness = 0.0018; pl.t60 = 5.0; wash = 0.03; pl.low_hz = 300.0; pl.high_hz = 8000.0; break;
          case kChina: pl.radius = 0.229; pl.thickness = 0.0009; pl.t60 = 2.0; pl.irregularity = 0.12; wash = 0.45; wash_hz = 4200.0; bloom_ms = 5.0; break;
          case kSplash: pl.radius = 0.127; pl.thickness = 0.0007; pl.t60 = 1.0; pl.curvature_hz = 600.0; wash = 0.24; bloom_ms = 4.0; break;
          default: break;
        }
        plate(0, pl, p, body_seed);
        noise(0, Shape::Band, wash_hz, 0.6, 0.1, bloom_ms, 0.0, Drive::Cascade, wash);
        noise(1, Shape::High, 9500.0, 0.7, 0.1, bloom_ms * 0.7, 0.0, Drive::Cascade, wash * 0.8, true);
        // The stick's ping on the bow.
        noise(2, Shape::High, 5000.0, 0.7, 0.05, 6.0, 0.0, Drive::Force, sound_ == kRide ? 0.4 : 0.15);
        if (sound_ == kRideBell) {
          static constexpr double kR[] = {1.0, 2.09, 2.66, 3.92, 5.1};
          static constexpr double kT[] = {4.0, 3.0, 2.5, 1.8, 1.2};
          static constexpr double kG[] = {1.0, 0.8, 0.7, 0.5, 0.3};
          table(1, 760.0, kR, kT, kG, 5, p);
          couple_ = true; couple_gain_ = 1.0f; bank_out_[1] = 3.0f;
        }
        if (sound_ == kCymbalSwell) every(0.07, true, 0, 0.15);
        tone(sound_ == kRideBell ? 300.0 : 150.0, 0.0);
        break;
      }
      // --- the orchestra ----------------------------------------------------------------------------
      case kTimpani: {
        // A kettle's air loads the head so its (1,1), (2,1), (3,1), (4,1) and (5,1) modes fall nearly
        // in the ratios 2 : 3 : 4 : 5 : 6 (Rossing); the (0,1) mode radiates its energy in a moment.
        static constexpr double kR[] = {0.85, 1.0, 1.50, 1.98, 2.44, 2.94, 1.70, 2.20, 2.80, 3.40};
        static constexpr double kT[] = {0.25, 3.6, 2.8, 2.1, 1.6, 1.2, 0.5, 0.6, 0.5, 0.6};
        static constexpr double kG[] = {0.5, 1.0, 0.8, 0.55, 0.35, 0.2, 0.15, 0.2, 0.15, 0.1};
        table(0, 146.8, kR, kT, kG, 10, p, 0.05);
        noise(0, Shape::Band, 1500.0, 0.8, 0.2, 25.0, 0.0, Drive::Force, 0.12);
        break;
      }
      case kBassDrum: {
        Membrane m; m.diameter = 0.81; m.tension = 1050.0; m.sigma = modal::kCalfskin; m.t60 = 2.6; m.reso = 1.03; m.position = 0.4; m.beta = 0.08; m.loss = 0.6;
        m.breath_t60 = 0.25;  // a big head's breathing mode pushes so much air it is gone at once
        membrane(0, m, p);
        noise(0, Shape::Band, 350.0, 0.6, 2.0, 150.0, 0.0, Drive::Force, 3.0);  // the dense high modes
        break;
      }
      case kTamTam: {
        Plate pl; pl.radius = 0.46; pl.thickness = 0.0016; pl.curvature_hz = 60.0; pl.low_hz = 45.0; pl.t60 = 14.0; pl.loss = 0.5; pl.tilt = 0.6;
        plate(0, pl, p, body_seed);
        // The tam-tam's swell: energy climbs into the high modes for a second after the stroke.
        noise(0, Shape::Band, 3200.0, 0.5, 0.1, 500.0, 0.0, Drive::Cascade, 0.006);
        noise(1, Shape::Band, 7000.0, 0.6, 0.1, 800.0, 0.0, Drive::Cascade, 0.004);
        break;
      }
      case kOperaGong: {
        // The small Chinese opera gong: a thick, nearly flat centre whose pitch rises as it dies.
        static constexpr double kR[] = {1.0, 1.53, 2.47, 3.1, 4.4};
        static constexpr double kT[] = {2.6, 1.6, 1.0, 0.8, 0.5};
        static constexpr double kG[] = {1.0, 0.35, 0.25, 0.15, 0.1};
        table(0, 380.0, kR, kT, kG, 5, p, -0.35);
        break;
      }
      case kTriangle: case kTriangleMuted: {
        // A 10 mm steel rod about 0.5 m long, bent into a triangle; the bends and the clip spread
        // the free bar's modes, which a steel beater excites to the top of hearing.
        const double f1 = modal::free_bar_hz(1, 0.50, modal::bar_wave_constant(modal::kSteel, 0.005 / 2.0));
        bar(0, f1, 26, sound_ == kTriangle ? 9.0 : 0.25, 0.25, 0.37, 1.0, p, 1.021);
        noise(0, Shape::High, 8000.0, 0.7, 0.05, 4.0, 0.0, Drive::Force, 0.08);
        tone(600.0, 0.0);
        break;
      }
      case kTubularBell: {
        // A brass tube 38 mm across with 1.5 mm walls; its 4th, 5th and 6th bending modes fall in
        // the ratio 2 : 3 : 4 of a missing fundamental, the strike note an octave below the 4th.
        const double strike_hz = 293.66;
        const double f1 = 2.0 * strike_hz / 8.933;
        bar(0, f1, 12, 6.0, 0.2, 0.0, 0.6, p, 1.002);
        noise(0, Shape::Band, 3000.0, 0.8, 0.1, 5.0, 0.0, Drive::Force, 0.05);
        break;
      }
      case kClashCymbals: {
        Plate pl; pl.radius = 0.229; pl.thickness = 0.0012; pl.t60 = 3.5; pl.tilt = 0.25;
        plate(0, pl, p, body_seed);
        noise(0, Shape::Band, 5500.0, 0.6, 0.1, 15.0, 0.0, Drive::Cascade, 0.12);
        noise(1, Shape::High, 9000.0, 0.7, 0.1, 20.0, 0.0, Drive::Cascade, 0.1, true);
        noise(2, Shape::Band, 2500.0, 0.6, 1.5, 40.0, 0.6);  // the plates' air squeezed out
        break;
      }
      case kTambourineShake: case kSleighBells: case kMaracas: case kCabasa: case kShaker: case kRainStick: {
        src::Particles::Setup s;
        switch (sound_) {
          case kTambourineShake:
            s.objects = 32; s.system_t60 = 0.35; s.sound_t60 = 0.03; s.hz = {2300.0, 5600.0, 8100.0, 0.0}; s.q = {30.0, 35.0, 40.0, 1.0}; s.gain = {1.0, 0.8, 0.6, 0.0}; s.jitter = 0.05; break;
          case kSleighBells:
            s.objects = 32; s.system_t60 = 0.6; s.sound_t60 = 0.12; s.hz = {2500.0, 5300.0, 6500.0, 8300.0}; s.q = {60.0, 70.0, 70.0, 80.0}; s.gain = {1.0, 0.8, 0.7, 0.5}; s.jitter = 0.03; break;
          case kMaracas:
            s.objects = 25; s.system_t60 = 0.25; s.sound_t60 = 0.006; s.hz = {5500.0, 0, 0, 0}; s.q = {2.2, 1, 1, 1}; s.gain = {1.0, 0, 0, 0}; break;
          case kCabasa:
            s.objects = 512; s.system_t60 = 0.09; s.sound_t60 = 0.002; s.hz = {3000.0, 0, 0, 0}; s.q = {1.2, 1, 1, 1}; s.gain = {1.0, 0, 0, 0}; break;
          case kShaker:
            s.objects = 64; s.system_t60 = 0.10; s.sound_t60 = 0.004; s.hz = {5500.0, 0, 0, 0}; s.q = {1.6, 1, 1, 1}; s.gain = {1.0, 0, 0, 0}; break;
          default:  // rain stick: pebbles falling past thorns for seconds
            s.objects = 600; s.system_t60 = 3.0; s.sound_t60 = 0.003; s.hz = {3800.0, 6400.0, 2200.0, 0.0}; s.q = {6.0, 8.0, 5.0, 1.0}; s.gain = {1.0, 0.6, 0.5, 0.0}; s.jitter = 0.1; break;
        }
        s.system_t60 *= p.decay * (1.0 - 0.8 * p.damping);
        for (auto& h : s.hz) h *= p.tune;
        particles_.configure(s);
        particles_on_ = true;
        tone(200.0, 0.0);
        break;
      }
      case kTambourineHit: {
        Membrane m; m.diameter = 0.254; m.tension = 2400.0; m.sigma = modal::kGoatskin; m.t60 = 0.25; m.beta = 0.05;
        membrane(0, m, p);
        bank_out_[0] = 0.5f;
        src::Particles::Setup s;
        s.objects = 32; s.system_t60 = 0.22 * p.decay; s.sound_t60 = 0.04; s.hz = {2300.0 * p.tune, 5600.0 * p.tune, 8100.0 * p.tune, 0.0}; s.q = {30.0, 35.0, 40.0, 1.0}; s.gain = {1.0, 0.8, 0.6, 0.0}; s.jitter = 0.05;
        particles_.configure(s);
        particles_on_ = true;
        break;
      }
      case kCastanets: case kCastanetRoll: {
        // Two hardwood shells clapping: their own modes, and the pocket of air between them
        // pushed out through the gap (a Helmholtz pop).
        static constexpr double kR[] = {1.0, 1.62, 2.38, 3.1};
        static constexpr double kT[] = {0.05, 0.035, 0.025, 0.02};
        static constexpr double kG[] = {1.0, 0.8, 0.6, 0.4};
        table(0, 2300.0, kR, kT, kG, 4, p);
        noise(0, Shape::Band, 1300.0, 2.5, 0.2, 9.0, 0.0, Drive::Force, 1.5);
        if (sound_ == kCastanetRoll) every(0.045, true, 0, 0.25);
        tone(300.0, 0.0);
        break;
      }
      case kWoodBlock: case kTempleBlock: case kClaves: case kSlapstick: case kTaikoRim: {
        if (sound_ == kClaves) {
          // A rosewood dowel 25 mm across and 0.2 m long, held at its node: one ringing mode.
          const double f1 = modal::free_bar_hz(1, 0.20, modal::bar_wave_constant(modal::kRosewood, 0.0125 / 2.0));
          static constexpr double kR[] = {1.0, 2.756, 5.40};
          static constexpr double kT[] = {0.22, 0.06, 0.03};
          static constexpr double kG[] = {1.0, 0.25, 0.1};
          table(0, f1, kR, kT, kG, 3, p);
        } else if (sound_ == kWoodBlock) {
          static constexpr double kR[] = {1.0, 1.95, 2.9, 4.2};
          static constexpr double kT[] = {0.12, 0.06, 0.04, 0.03};
          static constexpr double kG[] = {1.0, 0.5, 0.3, 0.2};
          table(0, 1050.0, kR, kT, kG, 4, p);
        } else if (sound_ == kTempleBlock) {
          static constexpr double kR[] = {1.0, 2.4, 3.6};
          static constexpr double kT[] = {0.2, 0.06, 0.04};
          static constexpr double kG[] = {1.0, 0.3, 0.15};
          table(0, 620.0, kR, kT, kG, 3, p);
        } else if (sound_ == kSlapstick) {
          static constexpr double kR[] = {1.0, 2.2, 3.7};
          static constexpr double kT[] = {0.05, 0.03, 0.02};
          static constexpr double kG[] = {1.0, 0.7, 0.5};
          table(0, 1900.0, kR, kT, kG, 3, p);
          noise(0, Shape::High, 3500.0, 0.7, 0.2, 45.0, 1.0);  // the air squeezed out between the boards
        } else {  // the bachi on the taiko's wooden body
          static constexpr double kR[] = {1.0, 1.8, 2.7, 3.9};
          static constexpr double kT[] = {0.07, 0.05, 0.03, 0.02};
          static constexpr double kG[] = {1.0, 0.7, 0.5, 0.3};
          table(0, 880.0, kR, kT, kG, 4, p);
        }
        noise(1, Shape::Band, 3000.0, 0.8, 0.05, 6.0, 0.0, Drive::Force, 0.25);
        tone(150.0, 0.0);
        break;
      }
      case kRatchet: {
        // A cog turning against a wooden tongue: each tooth lifts the tongue and lets it snap
        // onto the next, a clack into the frame's modes; while held it turns.
        static constexpr double kR[] = {1.0, 1.7, 2.6, 3.4};
        static constexpr double kT[] = {0.12, 0.09, 0.07, 0.05};
        static constexpr double kG[] = {1.0, 0.8, 0.7, 0.5};
        table(0, 2900.0, kR, kT, kG, 4, p);
        noise(0, Shape::High, 5000.0, 0.7, 0.05, 45.0, 0.0, Drive::Force, 12.0);
        every(1.0 / (12.0 * 4.0), true, 0, 0.08);  // 12 teeth, four turns a second
        break;
      }
      case kAnvil: {
        static constexpr double kR[] = {1.0, 1.72, 2.41, 3.12, 4.41, 5.3};
        static constexpr double kT[] = {2.2, 1.8, 1.4, 1.1, 0.8, 0.6};
        static constexpr double kG[] = {1.0, 0.7, 0.6, 0.4, 0.3, 0.2};
        table(0, 1130.0, kR, kT, kG, 6, p);
        noise(0, Shape::High, 4000.0, 0.7, 0.05, 5.0, 0.0, Drive::Force, 0.2);
        every(0.09, false, 1);  // the hammer bounces once
        break;
      }
      // --- hand drums -------------------------------------------------------------------------------
      case kBongoHigh: case kBongoLow: case kCongaOpen: case kCongaSlap: case kCongaMute: case kTumba: case kFrameDrum: {
        Membrane m; m.sigma = modal::kCalfskin; m.loss = 0.6; m.beta = 0.08; m.position = 0.85; m.breath_t60 = 0.08;
        switch (sound_) {
          case kBongoHigh: m.diameter = 0.178; m.tension = 3300.0; m.t60 = 0.35; break;
          case kBongoLow: m.diameter = 0.216; m.tension = 2600.0; m.t60 = 0.4; break;
          case kCongaOpen: case kCongaSlap: case kCongaMute: m.diameter = 0.292; m.tension = 2050.0; m.t60 = 0.6; break;
          case kTumba: m.diameter = 0.318; m.tension = 1500.0; m.t60 = 0.7; break;
          default: m.diameter = 0.457; m.tension = 320.0; m.sigma = modal::kGoatskin; m.t60 = 0.5; m.position = 0.6; m.breath_t60 = 0.0; break;
        }
        Play q = p;
        if (sound_ == kCongaMute) q.damping = std::max(p.damping, 0.75);
        membrane(0, m, q);
        if (sound_ == kCongaSlap) noise(0, Shape::Band, 1900.0, 1.4, 0.3, 45.0, 0.0, Drive::Force, 1.6);
        else noise(0, Shape::Band, 1600.0, 0.9, 0.3, 20.0, 0.0, Drive::Force, 0.15);
        break;
      }
      case kDjembeBass: case kDjembeTone: case kDjembeSlap: {
        Membrane m; m.diameter = 0.33; m.tension = 3300.0; m.sigma = modal::kGoatskin; m.loss = 0.7; m.beta = 0.08; m.t60 = 0.35;
        m.position = sound_ == kDjembeBass ? 0.1 : 0.85; m.breath_t60 = 0.06;
        membrane(0, m, p);
        // The goblet: a Helmholtz resonator whose neck is the stem, sounding under a centre stroke.
        static constexpr double kR[] = {1.0};
        static constexpr double kT[] = {0.35};
        static constexpr double kG[] = {1.0};
        table(1, 72.0, kR, kT, kG, 1, p);
        couple_ = true; couple_gain_ = 1.0f; bank_out_[1] = sound_ == kDjembeBass ? 2.5f : 0.3f;
        if (sound_ == kDjembeSlap) noise(0, Shape::Band, 2600.0, 1.2, 0.2, 40.0, 0.0, Drive::Force, 1.8);
        break;
      }
      case kTabla: {
        // The syahi's loading makes the head's (1,1), (2,1), (3,1)+(0,2), (4,1), (5,1) modes nearly
        // harmonic (Raman, 1934).
        static constexpr double kR[] = {1.0, 2.0, 3.0, 4.0, 5.0, 0.62};
        static constexpr double kT[] = {1.1, 0.8, 0.6, 0.4, 0.3, 0.12};
        static constexpr double kG[] = {1.0, 0.7, 0.5, 0.35, 0.2, 0.3};
        table(0, 293.7, kR, kT, kG, 6, p, 0.02);
        noise(0, Shape::Band, 3500.0, 0.8, 0.2, 10.0, 0.0, Drive::Force, 0.15);
        break;
      }
      case kCowbell: case kAgogoHigh: case kAgogoLow: {
        if (sound_ == kCowbell) {
          static constexpr double kR[] = {1.0, 1.50, 2.40, 2.98, 3.75, 4.32};
          static constexpr double kT[] = {0.45, 0.3, 0.22, 0.18, 0.14, 0.1};
          static constexpr double kG[] = {1.0, 0.85, 0.6, 0.5, 0.35, 0.25};
          table(0, 562.0, kR, kT, kG, 6, p);
        } else {
          static constexpr double kR[] = {1.0, 2.62, 4.9, 7.4};
          static constexpr double kT[] = {1.2, 0.7, 0.4, 0.25};
          static constexpr double kG[] = {1.0, 0.5, 0.3, 0.2};
          table(0, sound_ == kAgogoHigh ? 920.0 : 690.0, kR, kT, kG, 4, p);
        }
        noise(0, Shape::Band, 3500.0, 0.8, 0.05, 6.0, 0.0, Drive::Force, 0.15);
        break;
      }
      case kGuiroShort: case kGuiroLong: {
        // A stick drawn across the gourd's ridges: one click per ridge into the gourd's cavity.
        static constexpr double kR[] = {1.0, 1.8, 2.9};
        static constexpr double kT[] = {0.02, 0.015, 0.01};
        static constexpr double kG[] = {1.0, 0.6, 0.4};
        table(0, 2400.0, kR, kT, kG, 3, p);
        noise(0, Shape::Band, 4200.0, 1.0, 0.05, 4.0, 0.0, Drive::Force, 0.9);
        every(1.0 / (sound_ == kGuiroShort ? 260.0 : 140.0), false, sound_ == kGuiroShort ? 22 : 60, 0.2);
        tone(400.0, 0.0);
        break;
      }
      case kVibraslap: {
        // The ball knocks the wooden box; the loose rivets inside chatter at a slowing rate.
        static constexpr double kR[] = {1.0, 1.9, 3.2, 4.4};
        static constexpr double kT[] = {0.05, 0.04, 0.03, 0.02};
        static constexpr double kG[] = {1.0, 0.8, 0.6, 0.5};
        table(0, 2600.0, kR, kT, kG, 4, p);
        noise(0, Shape::Band, 4500.0, 1.2, 0.05, 35.0, 0.0, Drive::Force, 8.0);
        // Several rivets, each chattering on its own: together a dense, slowing buzz.
        every(0.007, false, 140, 0.7);
        pattern_.shrink = 1.006; pattern_.amp_step = 0.985;
        break;
      }
      // --- taiko ---------------------------------------------------------------------------------------
      case kODaiko: case kNagado: case kShime: {
        Membrane m; m.sigma = modal::kCowhide; m.reso = 1.0; m.coupling = 0.5; m.loss = 0.65; m.position = 0.25;
        // Taiko heads are thick cowhide stretched until a player can stand on them.
        if (sound_ == kODaiko) { m.diameter = 1.2; m.tension = 16000.0; m.t60 = 2.4; m.beta = 0.12; }
        if (sound_ == kNagado) { m.diameter = 0.61; m.tension = 15000.0; m.t60 = 1.3; m.beta = 0.1; }
        if (sound_ == kShime) { m.diameter = 0.36; m.tension = 21000.0; m.t60 = 0.35; m.beta = 0.04; m.reso = 1.02; }
        membrane(0, m, p);
        noise(0, Shape::Band, sound_ == kShime ? 3500.0 : 1600.0, 0.6, 0.3, 30.0, 0.0, Drive::Force, 4.0);
        noise(1, Shape::Band, sound_ == kShime ? 900.0 : 400.0, 0.6, 0.5, 120.0, 0.0, Drive::Force, 3.0);  // the dense high modes
        break;
      }
      // --- hands -----------------------------------------------------------------------------------------
      case kClap: case kGroupClap: {
        // The pocket of air between the palms, pushed out (Peltola et al., 2007): a damped
        // resonance around 1-2 kHz rung by the turbulent burst of the clap.
        noise(0, Shape::Band, 1150.0 * p.tune, 3.5, 0.4, 70.0, 1.0);
        noise(1, Shape::Band, 2300.0 * p.tune, 1.5, 0.3, 40.0, 0.5);
        if (sound_ == kGroupClap) every(0.006, false, 7, 0.9, 0.0);
        tone(200.0, 0.0);
        break;
      }
      case kSnap: {
        noise(0, Shape::Band, 2700.0 * p.tune, 2.0, 0.1, 35.0, 1.0);
        noise(1, Shape::High, 5000.0, 0.7, 0.05, 8.0, 0.6);
        tone(500.0, 0.0);
        break;
      }
      // --- TR-808 ------------------------------------------------------------------------------------------
      case kTR808Kick: {
        // A bridged-T network rung by the trigger pulse; the pulse also briefly shorts part of its
        // resistance, so the note starts sharp and falls in the first tens of milliseconds.
        static constexpr double kR[] = {1.0};
        static constexpr double kT[] = {0.9};
        static constexpr double kG[] = {1.0};
        table(0, 52.0, kR, kT, kG, 1, p, 0.10);
        noise(0, Shape::Low, 1200.0, 0.7, 0.1, 6.0, 0.0, Drive::Force, 0.15);
        break;
      }
      case kTR808Snare: {
        static constexpr double kR[] = {1.0, 2.0};
        static constexpr double kT[] = {0.16, 0.10};
        static constexpr double kG[] = {1.0, 0.55};
        table(0, 238.0, kR, kT, kG, 2, p, 0.02);
        noise(0, Shape::High, 1800.0, 0.7, 0.2, 240.0, 0.9, Drive::Trigger, 1.0, true);
        break;
      }
      case kTR808Clap: case kTR909Clap: {
        // White noise through a band-pass, its envelope a sawtooth that fires three times in quick
        // succession and then a longer decay: the circuit's imitation of several people clapping.
        const bool nine = sound_ == kTR909Clap;
        noise(0, Shape::Band, nine ? 1250.0 : 1000.0, 1.4, 0.1, 22.0, 1.0);
        noise(1, Shape::Band, nine ? 1250.0 : 1000.0, 1.2, 1.0, nine ? 280.0 : 230.0, 0.35);
        every(0.0105, false, 3, 0.0, 0.0105);
        tone(400.0, 0.0);
        break;
      }
      case kTR808HatClosed: case kTR808HatOpen: case kTR808Cymbal: case kTR808Cowbell: case kCR78Hat: {
        static constexpr double kMetal[] = {205.3, 304.4, 369.6, 522.7, 540.0, 800.0};
        static constexpr double kBell[] = {540.0, 800.0};
        squares_on_ = true;
        if (sound_ == kTR808Cowbell) {
          squares_.set(kBell, 2, p.tune);
          sq_filter_[0].set(620.0 * p.tune, 1.2);
          sq_mode_ = 1; sq_decay_ = decay_coef(0.06); sq_decay2_ = decay_coef(0.45 * p.decay); sq_mix_ = 0.25f;
        } else {
          squares_.set(kMetal, 6, p.tune);
          const bool cymbal = sound_ == kTR808Cymbal;
          sq_filter_[0].set(cymbal ? 7100.0 : 7100.0, cymbal ? 1.2 : 1.8);
          sq_filter_[1].set(cymbal ? 7100.0 : 10000.0, 0.8);
          sq_mode_ = cymbal ? 2 : 0;
          const double t = sound_ == kTR808HatOpen ? 0.55 : cymbal ? 1.4 : sound_ == kCR78Hat ? 0.07 : 0.06;
          sq_decay_ = decay_coef(t * p.decay); sq_decay2_ = decay_coef(t * 0.3 * p.decay); sq_mix_ = 0.6f;
        }
        tone(sound_ == kTR808Cowbell ? 300.0 : 700.0, 0.0);
        break;
      }
      case kTR808Tom: case kTR808Conga: case kCR78Kick: {
        static constexpr double kR[] = {1.0};
        static constexpr double kT[] = {0.0};
        double t60 = 0.5, hz = 120.0;
        if (sound_ == kTR808Conga) { t60 = 0.22; hz = 250.0; }
        if (sound_ == kCR78Kick) { t60 = 0.28; hz = 70.0; }
        const double t[] = {t60};
        (void)kT;
        static constexpr double kG[] = {1.0};
        table(0, hz, kR, t, kG, 1, p, sound_ == kTR808Tom ? 0.06 : 0.02);
        if (sound_ == kTR808Tom) noise(0, Shape::Low, 2500.0, 0.7, 0.5, 120.0, 0.1);
        break;
      }
      case kTR808Rim: case kTR808Claves: case kTR909Rim: {
        static constexpr double kRim[] = {1.0, 3.66};
        static constexpr double kRimT[] = {0.025, 0.02};
        static constexpr double kRimG[] = {1.0, 0.8};
        static constexpr double kClave[] = {1.0};
        static constexpr double kClaveT[] = {0.12};
        static constexpr double kClaveG[] = {1.0};
        if (sound_ == kTR808Claves) table(0, 2500.0, kClave, kClaveT, kClaveG, 1, p);
        else if (sound_ == kTR909Rim) { static constexpr double kR9[] = {1.0, 1.9}; table(0, 320.0, kR9, kRimT, kRimG, 2, p); }
        else table(0, 455.0, kRim, kRimT, kRimG, 2, p);
        if (sound_ != kTR808Claves) { tone(sound_ == kTR909Rim ? 120.0 : 300.0, 0.0); drive_ = 2.5f; noise(0, Shape::High, 3000.0, 0.7, 0.05, 8.0, 0.1); }
        break;
      }
      case kTR808Maracas: {
        noise(0, Shape::High, 5000.0, 0.7, 0.5, 45.0, 1.0, Drive::Trigger, 1.0, true);
        break;
      }
      // --- TR-909 ------------------------------------------------------------------------------------------
      case kTR909Kick: {
        osc_[0].trigger(320.0 * p.tune, 54.0 * p.tune, 22.0, 520.0 * p.decay);
        osc_gain_[0] = 1.0f;
        noise(0, Shape::Low, 6000.0, 0.7, 0.05, 7.0, 0.6);  // the attack click
        drive_ = 1.6f;
        break;
      }
      case kTR909Snare: case kCR78Snare: {
        const bool nine = sound_ == kTR909Snare;
        osc_[0].trigger((nine ? 300.0 : 360.0) * p.tune, (nine ? 186.0 : 340.0) * p.tune, 9.0, (nine ? 160.0 : 110.0) * p.decay);
        osc_[1].trigger((nine ? 530.0 : 0.0) * p.tune + 1.0, (nine ? 333.0 : 1.0) * p.tune + 1.0, 9.0, nine ? 100.0 * p.decay : 0.1);
        osc_gain_ = {1.0f, nine ? 0.5f : 0.0f};
        noise(0, Shape::Band, nine ? 5500.0 : 4000.0, 0.5, 0.3, (nine ? 260.0 : 120.0), nine ? 0.8 : 0.6);
        break;
      }
      case kTR909HatClosed: case kTR909HatOpen: case kTR909Crash: case kTR909Ride: {
        // The 909's cymbals were recordings of real ones held in 6-bit memory: a real cymbal here,
        // through six bits.
        Plate pl;
        const bool hat = sound_ == kTR909HatClosed || sound_ == kTR909HatOpen;
        if (hat) { pl.radius = 0.178; pl.thickness = 0.0011; pl.curvature_hz = 520.0; pl.low_hz = 300.0; pl.t60 = sound_ == kTR909HatOpen ? 0.9 : 0.08; }
        else if (sound_ == kTR909Crash) { pl.radius = 0.229; pl.t60 = 2.2; }
        else { pl.radius = 0.254; pl.thickness = 0.0016; pl.t60 = 1.5; }
        plate(0, pl, p, body_seed);
        noise(0, Shape::High, 7500.0, 0.7, 0.1, hat ? 6.0 : 20.0, 0.0, Drive::Cascade, hat ? 0.06 : 0.08, true);
        bits_ = 6;
        tone(hat || sound_ == kTR909Ride ? 1200.0 : 300.0, 0.0);
        break;
      }
      case kTR909Tom: {
        osc_[0].trigger(180.0 * p.tune, 120.0 * p.tune, 18.0, 420.0 * p.decay);
        osc_gain_[0] = 1.0f;
        noise(0, Shape::Low, 1200.0, 0.7, 0.3, 90.0, 0.15);
        break;
      }
      case kCR78Guiro: {
        noise(0, Shape::Band, 2500.0 * p.tune, 2.0, 0.2, 12.0, 0.8);
        every(0.022, false, 4, 0.0, 0.022);
        break;
      }
      // --- the junkyard --------------------------------------------------------------------------------------
      case kTrashCan: {
        // Galvanised steel 0.5 mm thick, 0.25 m in radius, 0.6 m tall, struck on its side.
        shell(0, 0.25, 0.6, 0.0005, modal::kSteel, 0.9, 90.0, 9000.0, p, body_seed);
        noise(0, Shape::Band, 2200.0, 0.6, 0.05, 30.0, 0.0, Drive::Force, 0.5);
        tone(60.0, 0.0);
        break;
      }
      case kTrashLid: {
        Plate pl; pl.radius = 0.27; pl.thickness = 0.0006; pl.material = modal::kSteel; pl.curvature_hz = 140.0; pl.low_hz = 80.0; pl.t60 = 1.3; pl.tilt = 0.2; pl.irregularity = 0.08;
        plate(0, pl, p, body_seed);
        noise(0, Shape::Band, 3000.0, 0.6, 0.1, 20.0, 0.0, Drive::Cascade, 0.06);
        every(0.11, false, 6, 0.2);  // it settles, rattling, on its rim
        pattern_.shrink = 0.72; pattern_.amp_step = 0.6;
        break;
      }
      case kOilDrum: {
        shell(0, 0.286, 0.88, 0.0012, modal::kSteel, 1.6, 70.0, 7000.0, p, body_seed);
        Membrane head; head.diameter = 0.572; head.tension = 9000.0; head.sigma = 9.4; head.t60 = 0.9; head.position = 0.4; head.beta = 0.0; head.max_ratio = 8.0;
        membrane(1, head, p);  // the 1.2 mm steel head behaves as a stiff, heavy membrane at the bottom of its range
        couple_ = true; couple_gain_ = 1.0f; bank_out_[1] = 1.4f;
        noise(0, Shape::Band, 1800.0, 0.6, 0.1, 25.0, 0.0, Drive::Force, 0.3);
        break;
      }
      case kMetalPipe: {
        // A steel pipe 1 m long, 33.7 mm across with 3.2 mm walls, dropped on concrete: it rings
        // its bending modes for seconds, and bounces with a restitution of about 0.45.
        const double ro = 0.0168, ri = 0.0136;
        const double f1 = modal::free_bar_hz(1, 1.0, modal::bar_wave_constant(modal::kSteel, 0.5 * std::sqrt(ro * ro + ri * ri)));
        bar(0, f1, 14, 3.5, 0.3, 0.1, 0.8, p, 1.006);
        noise(0, Shape::Band, 2500.0, 0.6, 0.1, 25.0, 0.0, Drive::Force, 0.3);
        every(0.32, false, 7, 0.08);
        pattern_.shrink = 0.62; pattern_.amp_step = 0.5;
        break;
      }
      case kGlassSmash: {
        // A 4 mm pane: its plate modes ring for an instant before it breaks, then the shards
        // fall and strike, each a small, high, bright piece.
        Plate pl; pl.radius = 0.3; pl.thickness = 0.004; pl.material = modal::kGlass; pl.curvature_hz = 0.0; pl.low_hz = 200.0; pl.t60 = 0.12; pl.tilt = 0.1;
        plate(0, pl, p, body_seed);
        src::Particles::Setup s;
        s.objects = 120; s.system_t60 = 1.4 * p.decay; s.sound_t60 = 0.03; s.hz = {3400.0, 5200.0, 7600.0, 10300.0}; s.q = {40.0, 50.0, 60.0, 60.0}; s.gain = {1.0, 0.9, 0.8, 0.6}; s.jitter = 0.35;
        particles_.configure(s);
        particles_on_ = true; particle_gain_ = 1.5f;
        noise(0, Shape::High, 2000.0, 0.7, 0.1, 60.0, 1.0);  // the crack
        tone(150.0, 0.0);
        break;
      }
      // --- machines and the environment ------------------------------------------------------------------------
      case kTelephone: {
        // The ringer: an armature swinging at the 20 Hz ringing current throws a steel clapper
        // between two gongs of slightly different pitch; the cadence is 2 s on, 4 s off.
        // A thin steel dome: its partials climb far up, and a hard clapper rings them all.
        static constexpr double kR[] = {1.0, 2.32, 4.25, 6.63, 9.38, 12.5, 15.9, 19.6};
        static constexpr double kT[] = {1.6, 1.2, 1.0, 0.8, 0.6, 0.5, 0.4, 0.3};
        static constexpr double kG[] = {0.5, 0.7, 1.0, 1.0, 1.0, 0.9, 0.8, 0.6};
        table(0, 1080.0, kR, kT, kG, 8, p);
        table(1, 1290.0, kR, kT, kG, 8, p);
        noise(0, Shape::High, 7000.0, 0.7, 0.02, 8.0, 0.0, Drive::Force, 3.0);
        every(1.0 / 40.0, true, 0, 0.02);
        break;
      }
      case kWashingMachine: {
        // The cabinet's steel panels (low, thuddy) take the wet load landing each turn of the
        // drum; the stainless tub (bright) takes the buttons and zips.
        Plate panel; panel.radius = 0.35; panel.thickness = 0.0007; panel.material = modal::kSteel; panel.curvature_hz = 0.0; panel.low_hz = 35.0; panel.high_hz = 1500.0; panel.t60 = 0.35; panel.tilt = 0.8; panel.modes = 30;
        plate(0, panel, p, body_seed);
        shell(1, 0.25, 0.35, 0.0005, modal::kSteel, 0.8, 300.0, 9000.0, p, body_seed + 1);
        every(60.0 / 52.0, true, 0, 0.06);  // tumbling at 52 rpm
        tone(30.0, 0.0);
        break;
      }
      case kJetFlyby: {
        flyby_.start(140.0, 90.0, 2450.0 * p.tune, 6.0 * p.decay, p.celsius, p.humidity);
        flyby_on_ = true;
        tone(25.0, 0.0);
        break;
      }
      case kVuvuzela: {
        // About 0.65 m of plastic cone, its lowest playable resonance near B-flat 3.
        horn_.reset();
        horn_.tune(233.0 * p.tune);
        horn_.blow(0.55 + 0.6 * p.velocity);
        horn_on_ = true;
        tone(120.0, 0.0);
        break;
      }
      case kSiren: {
        // A rotor of 10 ports chopping the air from a stator of 10: a pulse each time ports pass,
        // the motor spinning up to 45 turns a second while held and coasting down after.
        siren_on_ = true; siren_rps_ = 0.0; siren_target_ = 45.0 * p.tune; siren_ports_ = 10;
        sq_filter_[0].set(900.0, 1.2); sq_filter_[1].set(1800.0, 2.0);
        tone(80.0, 0.0);
        break;
      }
      case kFireworkLaunch: {
        blast(0.02, 25.0, 2, 1.0);
        // The mortar tube, 0.6 m, closed at the bottom: a quarter-wave pipe.
        static constexpr double kR[] = {1.0, 3.0, 5.0};
        static constexpr double kT[] = {0.15, 0.08, 0.05};
        static constexpr double kG[] = {1.0, 0.4, 0.2};
        table(0, sound_speed() / (4.0 * 0.6), kR, kT, kG, 3, p);
        noise(0, Shape::Band, 2500.0, 0.5, 30.0, 700.0, 0.12);  // the lift charge's hiss as the shell climbs
        break;
      }
      case kFireworkBurst: {
        blast(0.15, 150.0, 6, 2.2, 1.0, 0.0, 2.5, 500.0, 0.8);
        every(0.012, false, 70, 1.0, 0.25);  // the stars crackle as they burn out
        break;
      }
      case kFireworkCrackle: {
        every(0.008, false, 160, 1.0, 0.0);
        break;
      }
      case kFireworkWhistle: {
        // The whistle composition burns down its tube, so the air column shortens and the
        // whistle rises from about 1.1 to 2.6 kHz.
        osc_[0].trigger(1100.0 * p.tune, 2600.0 * p.tune, 520.0, 1600.0 * p.decay);
        osc_gain_[0] = 0.3f;
        noise(0, Shape::Band, 1800.0, 2.0, 50.0, 1500.0, 0.25);
        break;
      }
      case kCannon: blast(0.45, 80.0, 6, 2.5, 1.0, 0.0, 2.5, 250.0, 0.8); break;
      case kHowitzer: blast(9.0, 250.0, 8, 3.5, 1.0, 0.0, 3.0, 350.0, 0.8); break;
      case kMortar: {
        blast(0.25, 40.0, 4, 2.0, 1.0, 0.0, 2.0, 300.0, 0.7);
        static constexpr double kR[] = {1.0, 3.0};
        static constexpr double kT[] = {0.3, 0.12};
        static constexpr double kG[] = {1.0, 0.3};
        table(0, sound_speed() / (4.0 * 1.2), kR, kT, kG, 2, p);
        break;
      }
      case kRifle: case kMachineGun: {
        // 7.62 mm at Mach 2.5, passing 5 m off; the muzzle 8 m away.
        blast(0.004, 8.0, 3, 1.2, 1.0, 0.0, 1.4, 1500.0, 0.35);
        nwave(src::whitham_duration_s(2.5, 0.00762, 0.03, 5.0, sound_speed()), 3.0, 0.0);
        if (sound_ == kMachineGun) every(1.0 / 11.0, true, 0, 0.05, 1.0 / 11.0);
        break;
      }
      case kGrenade: {
        blast(0.18, 25.0, 4, 1.5, 1.0, 0.0, 2.0, 350.0, 0.7);
        src::Particles::Setup s;
        s.objects = 40; s.system_t60 = 1.2; s.sound_t60 = 0.01; s.hz = {900.0, 2300.0, 0.0, 0.0}; s.q = {2.0, 3.0, 1.0, 1.0}; s.gain = {1.0, 0.6, 0.0, 0.0};
        particles_.configure(s);
        particles_on_ = true; particle_gain_ = 0.4f;
        break;
      }
      case kExplosion: blast(250.0, 900.0, 10, 5.0, 1.0, 0.0, 3.5, 200.0, 0.9); break;
      case kThunder: {
        // A lightning channel some kilometres long, kinked every few metres: each segment sends
        // its own N-wave, and they arrive over seconds from 1 to 4 km away (Few, 1969).
        air_path(0, 1500.0); air_path(1, 2600.0); air_path(2, 4000.0);
        shocks_on_ = true;
        nwave(0.006, 2.0, 0.0, 0);
        every(0.006, false, 420, 1.0, 0.0);
        break;
      }
      case kSonicBoom: {
        // A fighter at Mach 1.4 at 9 km: a 120 ms N-wave and its ground reflection, the rise
        // smoothed by the turbulent air it came through.
        air_path(0, 400.0); air_path(1, 400.0); air_path(2, 3000.0);
        nwave(0.12 / p.tune, 1.0);
        nwave(0.12 / p.tune, 0.4, 0.004, 1);
        for (int i = 0; i < 4; ++i) nwave(0.2, 0.06, 0.15 + 0.4 * i, 2);
        break;
      }
      case kWhipCrack: {
        // The tip passes Mach 1 (about 1.2 times the speed of sound at the crack): a tiny N-wave,
        // tens of microseconds long, after the leather's swish.
        air_path(0, 2.0); air_path(1, 2.0); air_path(2, 30.0);
        nwave(std::max(src::whitham_duration_s(1.2, 0.002, 0.02, 2.0, sound_speed()), 3.0 / rate_), 4.0, 0.06);
        noise(0, Shape::Band, 1200.0, 0.8, 50.0, 80.0, 0.12);
        break;
      }
      default: break;
    }
  }

  // The stroke: what the hand, stick, beater or trigger does at the note.
  void strike(const Play& p, bool first) {
    const double v = p.velocity;
    switch (sound_) {
      case kKickStudio: case kKickVintage: case kKickJazz: case kKickDeep: {
        modal::Striker s = striker_of(play_.striker == kAsPlayed ? kHardMallet : play_.striker);
        if (play_.striker == kAsPlayed) { s.alpha = 2.5; s.contact_ms = 2.2; }
        s.contact_ms *= play_.hardness;
        hit(0, s);
        break;
      }
      case kSnareMaple: case kSnareBrass: case kSnareFat: case kSnareRimshot: case kTomHigh: case kTomMid: case kTomFloor: case kTomFloorLow:
        hit(0, striker(kStick, 1.0));
        break;
      case kSnareCrossStick: hit(0, striker(kStick, 0.3), 0.25); break;
      case kSnareBrush: hit(0, striker(kBrush, 1.0)); break;
      case kSnareBrushSweep: if (first) noise_[0].trigger(0.3 + 0.7 * v); break;
      case kHatClosed: case kHatOpen: case kHatHalf: case kCrash16: case kCrash18: case kRide: case kChina: case kSplash:
        hit(0, striker(kStick, 0.1));
        if (sound_ == kHatClosed || sound_ == kHatOpen || sound_ == kHatHalf) { noise_[1].trigger(v); if (sound_ == kHatHalf) noise_[2].trigger(v); }
        break;
      case kHatPedal: hit(0, striker(kStick, 0.05), 0.3); noise_[1].trigger(0.5 + 0.5 * v); noise_[2].trigger(0.5 + 0.5 * v); break;  // the plates meet
      case kRideBell: hit(0, striker(kStick, 0.08)); break;
      case kCymbalSwell: if (first) { pattern_.amp = 0.08; pattern_.amp_step = 1.06; } hit(0, striker(kFeltMallet, 0.3), 0.08); break;
      case kTimpani: case kBassDrum: case kTamTam: case kOperaGong: hit(0, striker(kFeltMallet, sound_ == kOperaGong ? 0.4 : 1.0)); break;
      case kTriangle: case kTriangleMuted: hit(0, striker(kStick, 0.08)); break;
      case kTubularBell: hit(0, striker(kHardMallet, 0.25)); break;
      case kClashCymbals: hit(0, striker(kHand, 0.4)); noise_[2].trigger(v); break;
      case kTambourineShake: case kSleighBells: case kMaracas: case kCabasa: case kShaker: case kRainStick:
        particles_.shake(0.4 + 1.2 * v);
        if (sound_ == kMaracas || sound_ == kShaker || sound_ == kCabasa) every(0.11, false, 1);  // the shake's return stroke
        break;
      case kTambourineHit: hit(0, striker(kHand, 0.6)); particles_.shake(0.6 + 1.2 * v); break;
      case kCastanets: case kCastanetRoll: hit(0, striker(kHardMallet, 0.1)); break;
      case kWoodBlock: case kTempleBlock: case kClaves: case kSlapstick: case kTaikoRim:
        hit(0, striker(sound_ == kClaves ? kHardMallet : kStick, 0.15));
        if (sound_ == kSlapstick) noise_[0].trigger(v);
        break;
      case kRatchet: case kGuiroShort: case kGuiroLong: case kVibraslap: case kAnvil: case kMetalPipe: case kTrashLid:
        hit(0, striker(kHardMallet, sound_ == kAnvil ? 0.05 : 0.15));
        break;
      case kBongoHigh: case kBongoLow: case kCongaOpen: case kCongaMute: case kTumba: case kDjembeTone:
        hit(0, striker(kHand, 0.6));
        break;
      case kCongaSlap: case kDjembeSlap: hit(0, striker(kHand, 0.25)); break;
      case kDjembeBass: hit(0, striker(kHand, 1.0)); break;
      case kFrameDrum: hit(0, striker(kStick, 1.4)); break;
      case kTabla: hit(0, striker(kHand, 0.3)); break;
      case kCowbell: case kAgogoHigh: case kAgogoLow: hit(0, striker(kStick, 0.12)); break;
      case kODaiko: case kNagado: case kShime: {
        modal::Striker s = striker(kStick, sound_ == kShime ? 1.0 : 2.0);  // bachi on thick hide
        hit(0, s);
        break;
      }
      case kClap: case kGroupClap: case kSnap: case kTR808Snare: case kTR808Maracas:
        noise_[0].trigger(0.3 + 0.7 * v); noise_[1].trigger(0.3 + 0.7 * v);
        if (sound_ == kTR808Snare) hit(0, striker(kStick, 0.5));
        break;
      case kTR808Clap: case kTR909Clap: noise_[0].trigger(0.3 + 0.7 * v); noise_[1].trigger(0.3 + 0.7 * v); break;
      case kTR808Kick: case kTR808Tom: case kTR808Conga: case kCR78Kick: case kTR808Rim: case kTR808Claves: case kTR909Rim: {
        // The trigger pulse: about a millisecond, its height the accent.
        modal::Striker s; s.alpha = 1.0; s.contact_ms = sound_ == kTR808Kick ? 1.0 : 0.3; s.restitution = 0.6;
        hit(0, s);
        if (sound_ == kTR808Tom || sound_ == kTR909Rim || sound_ == kTR808Rim) noise_[0].trigger(0.3 + 0.7 * v);
        break;
      }
      case kTR808HatClosed: case kTR808HatOpen: case kTR808Cymbal: case kTR808Cowbell: case kCR78Hat:
        sq_env_ = 0.3 + 0.7 * v; sq_env2_ = sq_env_;
        break;
      case kTR909Kick: case kTR909Snare: case kCR78Snare: case kTR909Tom:
        noise_[0].trigger(0.3 + 0.7 * v);
        break;
      case kTR909HatClosed: case kTR909HatOpen: case kTR909Crash: case kTR909Ride: hit(0, striker(kStick, 0.1)); break;
      case kCR78Guiro: noise_[0].trigger(0.3 + 0.7 * v); break;
      case kTrashCan: case kOilDrum: hit(0, striker(kStick, 0.4)); break;
      case kGlassSmash: hit(0, striker(kHardMallet, 0.08)); particles_.shake(1.0 + v); noise_[0].trigger(v); break;
      case kTelephone: case kWashingMachine: pattern_.next = 0.0; break;
      case kFireworkLaunch: noise_[0].trigger(0.5 + 0.5 * v); break;
      case kFireworkWhistle: noise_[0].trigger(0.4 + 0.6 * v); break;
      case kWhipCrack: noise_[0].trigger(0.5 + 0.5 * v); break;
      case kGrenade: particles_.shake(1.0 + v); break;
      default: break;
    }
  }

  // One event of a repeating gesture.
  void fire() {
    const double a = pattern_.amp;
    const int n = pattern_.count;
    switch (sound_) {
      case kSnareBrushSweep: {
        modal::Striker s = striker_of(kBrush); s.bristles = 1; s.contact_ms = 0.4;
        contact_[0].add(impulse(s, 0.01 * (0.4 + rng_.unipolar())), 0.4);
        break;
      }
      case kCymbalSwell: hit(0, striker(kFeltMallet, 0.3), std::min(1.0, a)); break;
      case kCastanetRoll: hit(0, striker(kHardMallet, 0.1), 0.5 + 0.3 * rng_.unipolar()); break;
      case kRatchet: hit(0, striker(kHardMallet, 0.15), 0.6 + 0.3 * rng_.unipolar()); break;
      case kAnvil: hit(0, striker(kHardMallet, 0.05), 0.15); break;
      case kGuiroShort: case kGuiroLong: hit(0, striker(kHardMallet, 0.15), 0.25 + 0.2 * rng_.unipolar()); break;
      case kVibraslap: hit(0, striker(kHardMallet, 0.1), 0.9 * a * (0.6 + 0.8 * rng_.unipolar())); break;
      case kMetalPipe: case kTrashLid: {
        // Each bounce lands it somewhere else along its length.
        for (int i = 0; i < bank_[0].count(); ++i) bank_[0].set_drive(i, (rng_.bipolar()) * 0.3);
        hit(0, striker(kHardMallet, 0.12), a);
        break;
      }
      case kMaracas: case kShaker: case kCabasa: particles_.shake(0.3 + 0.9 * play_.velocity); break;
      case kGroupClap: {
        const size_t i = static_cast<size_t>(n % 2);
        const double q = 2.0 + 4.0 * rng_.unipolar();
        const double hz = (900.0 + 900.0 * rng_.unipolar()) * play_.tune;
        noise_[i].set(src::NoiseBand::Shape::Band, hz, q, 0.3, 60.0 * play_.decay, 1.0);
        noise_[i].trigger(0.4 + 0.5 * rng_.unipolar());
        break;
      }
      case kTR808Clap: case kTR909Clap: noise_[0].trigger(0.3 + 0.7 * play_.velocity); if (n == 0) noise_[1].trigger(0.3 + 0.7 * play_.velocity); break;
      case kCR78Guiro: noise_[0].trigger(0.3 + 0.7 * play_.velocity); break;
      case kTelephone: {
        // The cadence: 2 s ringing, 4 s silent, while held.
        const double cycle = std::fmod(pattern_.t / rate_, 6.0);
        if (cycle < 2.0) {
          modal::Striker s; s.alpha = 1.5; s.contact_ms = 0.08; s.restitution = 0.7;
          hit(n % 2, s, 0.5);
        }
        break;
      }
      case kWashingMachine: {
        modal::Striker load; load.alpha = 3.0; load.contact_ms = 14.0; load.restitution = 0.1;
        hit(0, load, 1.4);
        modal::Striker button; button.alpha = 1.5; button.contact_ms = 0.05; button.restitution = 0.7;
        const int clinks = 1 + static_cast<int>(rng_.next() % 3);
        for (int i = 0; i < clinks; ++i) {
          const double when = (0.05 + 0.35 * rng_.unipolar()) * rate_;
          const double force = 0.12 * (0.5 + rng_.unipolar());
          contact_[1].add(impulse(button, force), 0.05, when);
        }
        break;
      }
      case kFireworkBurst: case kFireworkCrackle: {
        src::Shock s;
        s.length = (0.15 + 0.3 * rng_.unipolar()) * 0.001 * rate_; s.amp = (0.3 + rng_.unipolar()) * (sound_ == kFireworkBurst ? 0.25 : 0.6); s.b = 1.5; s.path = 0;
        shocks_.add(s);
        shocks_on_ = true;
        break;
      }
      case kMachineGun: {
        blast(0.004, 8.0, 1, 1.0, 1.0, 0.0, 1.4, 1500.0, 0.35);
        nwave(src::whitham_duration_s(2.5, 0.00762, 0.03, 5.0, sound_speed()), 3.0, 0.0);
        break;
      }
      case kThunder: {
        // Arrivals bunch where the channel runs across the line of sight (the claps) and thin
        // where it runs along it (the rumble).
        const double t = pattern_.t / rate_;
        const double clap = std::exp(-std::pow((t - 0.35) / 0.25, 2.0)) + 0.6 * std::exp(-std::pow((t - 1.4) / 0.4, 2.0));
        const int path = t < 0.6 ? 0 : t < 1.8 ? 1 : 2;
        const double amp = (0.15 + clap) * (0.4 + rng_.unipolar()) * std::exp(-t * 0.5);
        const double length = (0.004 + 0.01 * rng_.unipolar()) * (1.0 + t);
        nwave(length, amp, 0.0, path);
        pattern_.interval = std::max(1.0, rate_ * 0.006 * (1.0 + t * 1.2));
        break;
      }
      default: break;
    }
  }

  void run_pattern() {
    pattern_.t += 1.0;
    if (--pattern_.next > 0.0) return;
    fire();
    ++pattern_.count;
    pattern_.amp *= pattern_.amp_step;
    pattern_.interval *= pattern_.shrink;
    pattern_.next = pattern_.interval * (1.0 + pattern_.jitter * rng_.bipolar());
    if (pattern_.pending > 0 && --pattern_.pending == 0 && !pattern_.running) pattern_.next = 0.0;
  }

  // The 808's metal: squares through band-passes, under one or two decaying envelopes.
  float metal() {
    if (sq_env_ < 1e-6 && sq_env2_ < 1e-6) return 0.0f;
    const float x = squares_.process();
    sq_env_ *= sq_decay_; sq_env2_ *= sq_decay2_;
    float y = 0.0f;
    if (sq_mode_ == 1) {  // the cowbell: a sharp first decay into a long tail
      y = sq_filter_[0].band(x) * static_cast<float>(sq_env2_ * sq_mix_ + sq_env_ * (1.0f - sq_mix_));
    } else if (sq_mode_ == 2) {  // the cymbal: low band long, high band short
      y = sq_filter_[0].band(x) * static_cast<float>(sq_env_) + sq_filter_[1].band(x) * static_cast<float>(sq_env2_) * 0.6f;
    } else {
      const float b = sq_filter_[0].band(x);
      y = (b - sq_filter_[1].low(b)) * static_cast<float>(sq_env_) * 1.6f;
    }
    return y;
  }
  double decay_coef(double t60) const { return std::exp(-6.907755 / std::max(1.0, t60 * rate_)); }

  float siren() {
    siren_rps_ += (siren_target_ - siren_rps_) * (siren_target_ > siren_rps_ ? 0.6 : 0.25) / rate_;
    if (siren_rps_ < 0.05 && siren_target_ <= 0.0) { siren_on_ = false; return 0.0f; }
    siren_phase_ += siren_rps_ * siren_ports_ / rate_;
    if (siren_phase_ >= 1.0) siren_phase_ -= 1.0;
    // The ports open and close: the flow is a rounded pulse, its strength the rotor's speed.
    const double open = std::max(0.0, std::sin(kTau * siren_phase_));
    const double flow = open * open * std::min(1.0, siren_rps_ / 20.0);
    const float x = static_cast<float>(flow - 0.5 * std::min(1.0, siren_rps_ / 20.0));
    return (sq_filter_[0].band(x) * 1.2f + sq_filter_[1].band(x) * 0.5f + x * 0.3f) * 0.8f;
  }

  double rate_ = 48000.0, speed_ = 2.0;
  int sound_ = -1;
  bool awake_ = false, held_ = false;
  Play play_{};
  Rng rng_;
  std::array<modal::Contact, 2> contact_{};
  std::array<modal::ModeBank, 2> bank_{};
  std::array<float, 2> bank_out_{1.0f, 1.0f};
  bool couple_ = false;
  float couple_gain_ = 1.0f;
  std::array<src::NoiseBand, 3> noise_{};
  std::array<Drive, 3> noise_drive_{};
  std::array<double, 3> noise_k_{}, follow_{};
  src::Particles particles_;
  src::SnareWires wires_;
  src::SquareBank squares_;
  std::array<StateVariable, 2> sq_filter_{};
  std::array<StateVariable, 2> tone_{};
  int sq_mode_ = 0;
  double sq_env_ = 0.0, sq_env2_ = 0.0, sq_decay_ = 0.999, sq_decay2_ = 0.999;
  float sq_mix_ = 0.5f;
  std::array<src::SweptOsc, 2> osc_{};
  std::array<float, 2> osc_gain_{};
  src::ShockField shocks_;
  src::LipHorn horn_;
  src::Flyby flyby_;
  bool wires_on_ = false, particles_on_ = false, squares_on_ = false, shocks_on_ = false, horn_on_ = false, flyby_on_ = false, siren_on_ = false;
  int wire_bank_ = 0;
  double wire_scale_ = 1.0;
  float particle_gain_ = 1.0f, shock_gain_ = 1.0f;
  float hp_pole_ = 0.0f, lp_pole_ = 0.0f, hp_state_ = 0.0f, lp_state_ = 0.0f, lp2_state_ = 0.0f, drive_ = 0.0f;
  int bits_ = 0;
  float tail_air_pole_ = 0.0f;
  std::array<float, 2> tail_lp_{};
  Pattern pattern_{};
  double siren_rps_ = 0.0, siren_target_ = 0.0, siren_phase_ = 0.0;
  int siren_ports_ = 10;
  float level_ = 0.0f;
  int quiet_ = 0;
  uint64_t age_ = 0;
};

}  // namespace bonkr::dsp
