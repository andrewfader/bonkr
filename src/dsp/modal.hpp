// The struck body and what strikes it, from their mechanics.
//
// - A stick, mallet, beater or hand meets a surface through Hertzian contact: the force grows as
//   the compression to the power alpha (1.5 for two hard elastic bodies, 2.5 to 3.5 for felt and
//   flesh). Solving the collision for a mass m arriving at speed v gives a contact time that falls
//   as v^-((alpha - 1) / (alpha + 1)) and an impulse m v (1 + e): a harder stroke is shorter, so
//   brighter, and felt brightens more with force than wood does. The force is the half-sine of
//   that duration carrying that impulse (Hunt and Crossley's shape without the hysteresis, which
//   the restitution e stands for).
// - The body is a sum of its normal modes (Adrien's modal synthesis): each a damped oscillator at
//   its own frequency, driven by the force times the mode's shape where it is struck and heard
//   through its shape where it radiates. The frequencies come from the body's geometry and
//   material, below.
//   * Circular membrane (Rayleigh): f_mn = j_mn / (2 pi a) sqrt(T / sigma), j_mn the n-th zero of
//     the Bessel function J_m, the shape J_m(j_mn r / a) cos(m theta).
//   * Free-edged circular plate of thickness h (Leissa): f = lambda^2 / (2 pi a^2) sqrt(D / rho h),
//     D = E h^3 / 12 (1 - nu^2); for the higher modes Chladni's law f ~ (m + 2n)^2.
//   * Free-free bar (Euler-Bernoulli): f_n = (beta_n L)^2 / (2 pi L^2) sqrt(E I / rho A), beta_n L
//     = 4.730, 7.853, 10.996, ... -> (2n + 1) pi / 2.
//   * Thin cylindrical shell (Donnell-Mushtari): Omega^2 = (1 - nu^2) lambda^4 / (lambda^2 + n^2)^2
//     + beta^2 (lambda^2 + n^2)^2, lambda = m pi a / L, beta^2 = h^2 / 12 a^2, f = Omega c_p / 2 pi a.
// - Large vibration stretches a membrane and raises its tension (Kirchhoff-Carrier): the pitch of
//   a hard-hit drum starts high and falls as the vibration dies, f = f0 sqrt(1 + beta A^2). The
//   bank tracks its own amplitude A (1 at the hardest stroke) and glides every mode by that factor:
//   a beta of 0.16 starts a fortissimo tom about 8% (a semitone and a half) sharp.
//
// Every oscillator is a complex one-pole (a rotating phasor): unconditionally stable, exact in
// frequency and decay, and cheap to retune.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "core.hpp"

namespace bonkr::dsp::modal {

// --- Bessel functions and their zeros -------------------------------------------------------------

// J_m(x) by Bessel's integral (1/pi) int_0^pi cos(m t - x sin t) dt; the integrand is smooth and
// periodic, so the trapezoid rule converges geometrically. Exact to 1e-12 for x < 40.
inline double bessel_j(int m, double x) {
  constexpr int kSteps = 96;
  double sum = 0.0;
  for (int i = 0; i <= kSteps; ++i) {
    const double t = kPi * i / kSteps;
    const double w = (i == 0 || i == kSteps) ? 0.5 : 1.0;
    sum += w * std::cos(m * t - x * std::sin(t));
  }
  return sum / kSteps;
}
inline double bessel_j_prime(int m, double x) { return 0.5 * (bessel_j(m - 1, x) - bessel_j(m + 1, x)); }

// The n-th positive zero of J_m: McMahon's asymptotic estimate refined by Newton's method.
inline double bessel_zero(int m, int n) {
  // Start from the table where McMahon is poor (n = 1, larger m), from McMahon elsewhere.
  const double b = (n + 0.5 * m - 0.25) * kPi;
  const double mu = 4.0 * m * m;
  double x = b - (mu - 1.0) / (8.0 * b) - 4.0 * (mu - 1.0) * (7.0 * mu - 31.0) / (3.0 * std::pow(8.0 * b, 3));
  if (n == 1 && m > 0) x = m + 1.8557 * std::cbrt(static_cast<double>(m)) + 1.033 / std::cbrt(static_cast<double>(m));
  for (int i = 0; i < 40; ++i) {
    const double f = bessel_j(m, x), d = bessel_j_prime(m, x);
    if (d == 0.0) break;
    const double step = f / d;
    x -= std::clamp(step, -0.5, 0.5);
    if (std::abs(step) < 1e-12) break;
  }
  return x;
}

struct MembraneMode { int m, n; double j; };

// The membrane's modes in order of frequency, up to j = 40 (about 16 times the fundamental).
// Computed once, the first time it is asked for, never on the audio thread (prepare calls it).
inline const std::vector<MembraneMode>& membrane_modes() {
  static const std::vector<MembraneMode> table = [] {
    std::vector<MembraneMode> t;
    for (int m = 0; m <= 30; ++m)
      for (int n = 1; n <= 12; ++n) {
        const double j = bessel_zero(m, n);
        if (j > 40.0) break;
        t.push_back({m, n, j});
      }
    std::sort(t.begin(), t.end(), [](const MembraneMode& a, const MembraneMode& b) { return a.j < b.j; });
    return t;
  }();
  return table;
}

// --- materials --------------------------------------------------------------------------------------

struct Material { double youngs_pa, density, poisson; };
inline constexpr Material kBronzeB20{1.05e11, 8600.0, 0.34};  // cymbal bronze, 80% Cu 20% Sn
inline constexpr Material kBrass{1.0e11, 8500.0, 0.34};
inline constexpr Material kSteel{2.0e11, 7850.0, 0.29};
inline constexpr Material kAluminium{6.9e10, 2700.0, 0.33};
inline constexpr Material kRosewood{1.6e10, 1100.0, 0.30};
inline constexpr Material kMaple{1.26e10, 705.0, 0.30};
inline constexpr Material kGlass{6.3e10, 2500.0, 0.23};
inline constexpr Material kCastIron{1.1e11, 7200.0, 0.26};

// Drum heads: areal density in kg/m^2 (Mylar 1.39 g/cm^3 times thickness; calfskin and cowhide
// as measured by Fletcher and Rossing).
inline constexpr double kHeadSingle10mil = 0.353;   // 0.254 mm Mylar
inline constexpr double kHeadTwoPly7mil = 0.494;    // two plies of 0.178 mm
inline constexpr double kHeadThin = 0.26;           // 7.5 mil, resonant heads and snare sides
inline constexpr double kHeadSnareSide = 0.10;      // 3 mil snare-side head
inline constexpr double kCalfskin = 0.38;
inline constexpr double kCowhide = 1.20;            // taiko: thick cowhide
inline constexpr double kGoatskin = 0.30;

inline double membrane_f01(double diameter_m, double tension_n_per_m, double sigma) {
  return 2.404825557695773 / (kPi * diameter_m) * std::sqrt(tension_n_per_m / sigma);
}
inline double bar_wave_constant(const Material& m, double radius_of_gyration_m) {
  return std::sqrt(m.youngs_pa / m.density) * radius_of_gyration_m;
}
// Free-free bar mode n (from 1), Hz, for length L and the constant sqrt(EI / rho A).
inline double free_bar_hz(int n, double length_m, double wave_constant) {
  static constexpr double kBeta[] = {4.730040745, 7.853204624, 10.995607838, 14.137165491, 17.278759657};
  const double bl = n <= 5 ? kBeta[n - 1] : (2.0 * n + 1.0) * kPi / 2.0;
  return bl * bl / (kTau * length_m * length_m) * wave_constant;
}
// Free circular plate: lambda^2 for the low modes (Leissa, nu = 0.33), Chladni's law beyond.
inline double free_plate_lambda2(int m, int n) {
  static constexpr double kLow[4][3] = {{0.0, 9.084, 38.55}, {0.0, 20.52, 59.86}, {5.253, 35.25, 84.37}, {12.23, 52.91, 111.0}};
  if (m < 4 && n < 3 && kLow[m][n] > 0.0) return kLow[m][n];
  const double k = (m + 2.0 * n) * kPi / 2.0 * 0.92;
  return k * k;
}
inline double plate_hz(double lambda2, double radius_m, double thickness_m, const Material& mat) {
  const double d = mat.youngs_pa * thickness_m * thickness_m * thickness_m / (12.0 * (1.0 - mat.poisson * mat.poisson));
  return lambda2 / (kTau * radius_m * radius_m) * std::sqrt(d / (mat.density * thickness_m));
}
// Thin cylindrical shell (Donnell-Mushtari, the lower root), Hz.
inline double shell_hz(int axial, int circumferential, double radius_m, double length_m, double thickness_m, const Material& mat) {
  const double lambda = axial * kPi * radius_m / length_m;
  const double n = circumferential;
  const double s = lambda * lambda + n * n;
  const double beta2 = thickness_m * thickness_m / (12.0 * radius_m * radius_m);
  const double omega2 = (1.0 - mat.poisson * mat.poisson) * lambda * lambda * lambda * lambda / (s * s) + beta2 * s * s;
  const double cp = std::sqrt(mat.youngs_pa / (mat.density * (1.0 - mat.poisson * mat.poisson)));
  return std::sqrt(omega2) * cp / (kTau * radius_m);
}

// --- the strike -------------------------------------------------------------------------------------

// What strikes: its Hertz exponent, its mass, how long it stays on at the reference speed, and its
// coefficient of restitution.
struct Striker {
  double alpha = 1.5;       // Hertz exponent
  double contact_ms = 1.0;  // contact time at the reference speed of 3 m/s, on this body
  double restitution = 0.6; // e
  int bristles = 1;         // a brush: so many small contacts spread over spread_ms
  double spread_ms = 0.0;
};
inline constexpr double kReferenceSpeed = 3.0;  // m/s

// The force of one contact, sample by sample: a half-sine of the Hertzian duration whose area is
// the impulse. Several overlapping contacts (bristles, flams, a ringer's clapper) are queued.
class Contact {
 public:
  static constexpr int kMax = 48;
  void prepare(double rate) { rate_ = rate; reset(); }
  void reset() { count_ = 0; }
  // The contact time the striker makes at this speed: tau0 (v / v0)^-((alpha - 1) / (alpha + 1)).
  static double contact_ms(const Striker& s, double speed) {
    const double exponent = (s.alpha - 1.0) / (s.alpha + 1.0);
    return s.contact_ms * std::pow(std::max(speed, 0.05) / kReferenceSpeed, -exponent);
  }
  // Starts a contact `delay` samples from now carrying impulse `impulse` over `ms`.
  void add(double impulse, double ms, double delay = 0.0) {
    if (count_ >= kMax) return;
    Pulse& p = pulses_[static_cast<size_t>(count_++)];
    // Sampled at the middle of each sample's interval, so even a one-sample contact carries its
    // impulse, and scaled so the samples sum to it exactly.
    p.length = std::max(1.0, std::round(ms * 0.001 * rate_));
    p.t = -std::max(0.0, std::round(delay));
    double sum = 0.0;
    for (int k = 1; k <= static_cast<int>(p.length); ++k) sum += std::sin(kPi * (k - 0.5) / p.length);
    p.amp = impulse / sum;
  }
  // One strike of a striker at a speed: a brush spreads its bristles, each a small contact.
  void strike(const Striker& s, double speed, double impulse, uint32_t seed, double delay = 0.0) {
    const double ms = contact_ms(s, speed);
    if (s.bristles <= 1) { add(impulse, ms, delay); return; }
    Rng rng(seed);
    for (int i = 0; i < s.bristles; ++i) {
      const double when = delay + rng.unipolar() * s.spread_ms * 0.001 * rate_;
      add(impulse / s.bristles * (0.5 + rng.unipolar()), ms * (0.6 + 0.8 * rng.unipolar()), when);
    }
  }
  float next() {
    double f = 0.0;
    for (int i = 0; i < count_;) {
      Pulse& p = pulses_[static_cast<size_t>(i)];
      p.t += 1.0;
      if (p.t > 0.0 && p.t <= p.length) f += p.amp * std::sin(kPi * (p.t - 0.5) / p.length);
      if (p.t >= p.length) { pulses_[static_cast<size_t>(i)] = pulses_[static_cast<size_t>(--count_)]; continue; }
      ++i;
    }
    return static_cast<float>(f);
  }
  bool active() const { return count_ > 0; }
 private:
  struct Pulse { double t = 0.0, length = 1.0, amp = 0.0; };
  std::array<Pulse, kMax> pulses_{};
  int count_ = 0;
  double rate_ = 48000.0;
};

// --- the body ----------------------------------------------------------------------------------------

struct ModeSpec {
  double hz = 100.0;
  double t60 = 1.0;      // seconds
  double drive = 1.0;    // the mode's shape where it is struck
  double radiate = 1.0;  // the mode's share of what is heard
};

class ModeBank {
 public:
  static constexpr int kMaxModes = 72;
  void prepare(double rate) { rate_ = rate; clear(); }
  void clear() {
    count_ = 0; glide_ = 1.0; amplitude_ = 0.0; damp_ = 1.0;
    for (auto& m : modes_) m = Mode{};
  }
  void silence() { for (int i = 0; i < count_; ++i) { modes_[static_cast<size_t>(i)].re = modes_[static_cast<size_t>(i)].im = 0.0f; } amplitude_ = 0.0; }
  int count() const { return count_; }
  // Replaces the modes, keeping nothing of the old vibration. Modes at or above 0.47 of the
  // sample rate are left out (they cannot be represented).
  void set(const ModeSpec* specs, int n) {
    count_ = 0;
    for (int i = 0; i < n && count_ < kMaxModes; ++i) {
      if (!(specs[i].hz > 0.0) || specs[i].hz >= 0.47 * rate_ || specs[i].t60 <= 0.0) continue;
      Mode& m = modes_[static_cast<size_t>(count_++)];
      m = Mode{};
      m.w = kTau * specs[i].hz / rate_;
      m.decay = std::exp(-6.907755 / (specs[i].t60 * rate_));
      m.drive = static_cast<float>(specs[i].drive);
      m.out = static_cast<float>(specs[i].radiate);
      m.base_decay = m.decay;
    }
    retune(1.0);
  }
  // Changes where the next strike lands (each mode's shape there), keeping the vibration.
  void set_drive(int i, double drive) { if (i >= 0 && i < count_) modes_[static_cast<size_t>(i)].drive = static_cast<float>(drive); }
  double hz(int i) const { return modes_[static_cast<size_t>(i)].w * rate_ / kTau; }
  double t60(int i) const { return -6.907755 / (std::log(modes_[static_cast<size_t>(i)].base_decay) * rate_); }
  // Tension modulation: the pitch rises as sqrt(1 + beta A^2) with the bank's amplitude A (1 for
  // the hardest stroke), and a negative beta lowers it (a gong's swoop up as it dies away is a
  // negative beta seen from the end).
  void set_nonlinearity(double beta) { beta_ = beta; }
  // A hand or a felt on the body, or the pedal closing a hi-hat: multiplies every mode's decay
  // time by `factor` (< 1 shortens), the higher modes more as a damping layer does.
  void damp(double factor) {
    damp_ = std::clamp(factor, 0.001, 1.0);
    for (int i = 0; i < count_; ++i) {
      Mode& m = modes_[static_cast<size_t>(i)];
      const double t = -6.907755 / (std::log(m.base_decay) * rate_);
      const double f = m.w * rate_ / kTau;
      const double scale = damp_ / (1.0 + (1.0 - damp_) * f / 2000.0);
      m.decay = std::exp(-6.907755 / (std::max(t * scale, 0.002) * rate_));
    }
    retune(glide_);
  }
  float process(float force) {
    if (++tick_ >= 16) { tick_ = 0; update_glide(); }
    float out = 0.0f;
    double a = 0.0;
    for (int i = 0; i < count_; ++i) {
      Mode& m = modes_[static_cast<size_t>(i)];
      const float re = m.re * m.cr - m.im * m.ci + force * m.drive;
      const float im = m.re * m.ci + m.im * m.cr;
      m.re = re; m.im = im;
      out += im * m.out;
      if (i < 4) a += std::abs(re) + std::abs(im);
    }
    amplitude_ = a;
    return out;
  }
  // Each mode's displacement at the struck point, for the snare wires.
  float displacement() const {
    float d = 0.0f;
    for (int i = 0; i < count_; ++i) d += modes_[static_cast<size_t>(i)].im * modes_[static_cast<size_t>(i)].drive;
    return d;
  }
  // Displacement of the modes with no nodal diameter (the drum's breathing modes), which push
  // the air inside and so the other head.
  double amplitude() const { return amplitude_; }
  double glide() const { return glide_; }
  double energy() const {
    double e = 0.0;
    for (int i = 0; i < count_; ++i) e += static_cast<double>(modes_[static_cast<size_t>(i)].re) * modes_[static_cast<size_t>(i)].re + static_cast<double>(modes_[static_cast<size_t>(i)].im) * modes_[static_cast<size_t>(i)].im;
    return e;
  }
 private:
  struct Mode {
    float re = 0.0f, im = 0.0f, cr = 1.0f, ci = 0.0f, drive = 0.0f, out = 0.0f;
    double w = 0.0, decay = 0.0, base_decay = 0.0;
  };
  void update_glide() {
    if (beta_ == 0.0) return;
    // The bank's amplitude in units of the hardest stroke (about 4 at 12 m/s).
    const double a = std::min(amplitude_ * 0.25, 1.5);
    const double g = std::sqrt(std::max(0.05, 1.0 + beta_ * a * a));
    if (std::abs(g - glide_) > 1e-5) retune(g);
  }
  void retune(double g) {
    glide_ = g;
    for (int i = 0; i < count_; ++i) {
      Mode& m = modes_[static_cast<size_t>(i)];
      const double w = std::min(m.w * g, kPi * 0.94);
      m.cr = static_cast<float>(m.decay * std::cos(w));
      m.ci = static_cast<float>(m.decay * std::sin(w));
    }
  }
  std::array<Mode, kMaxModes> modes_{};
  int count_ = 0, tick_ = 0;
  double rate_ = 48000.0, beta_ = 0.0, glide_ = 1.0, amplitude_ = 0.0, damp_ = 1.0;
};

}  // namespace bonkr::dsp::modal
