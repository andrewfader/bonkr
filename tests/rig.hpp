// A test rig around the engine and around one voice: strike, render, measure. Shared by the
// engine-level suites.
#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "../src/engine.hpp"
#include "audio_measure.hpp"

namespace rig {

constexpr double kRate = 48000.0;
constexpr uint32_t kBlock = 256;

struct Engine {
  std::unique_ptr<bonkr::Engine> engine = std::make_unique<bonkr::Engine>();
  bonkr::Settings s = bonkr::Settings::defaults();
  std::vector<float> left, right;
  explicit Engine(double rate = kRate) : rate_(rate) { engine->prepare(rate); }
  // One sound across the keyboard, C4 at its own pitch.
  static Engine sound(int sound) {
    Engine e;
    e.s.v[bonkr::params::kLayout] = bonkr::params::kLayoutChromatic;
    e.s.v[bonkr::params::kSound] = sound;
    return e;
  }
  static Engine kit(int kit) {
    Engine e;
    e.s.v[bonkr::params::kKit] = kit;
    return e;
  }
  void render(double seconds) { render_frames(static_cast<uint32_t>(seconds * rate_)); }
  void render_frames(uint32_t frames) {
    const size_t at = left.size();
    left.resize(at + frames); right.resize(at + frames);
    for (uint32_t done = 0; done < frames; done += kBlock) {
      const uint32_t n = std::min(kBlock, frames - done);
      engine->render(s, left.data() + at + done, right.data() + at + done, n);
    }
  }
  std::vector<float> mono() const {
    std::vector<float> m(left.size());
    for (size_t i = 0; i < m.size(); ++i) m[i] = 0.5f * (left[i] + right[i]);
    return m;
  }
  void on(int key, double vel = 0.8) { engine->note_on(s, 0, key, -1, vel); }
  void off(int key) { engine->note_off(s, 0, key, -1); }
  // Strikes a key, holds it, lets go, and returns the whole render in mono.
  std::vector<float> stroke(int key, double vel, double hold_s, double tail_s) {
    on(key, vel);
    render(hold_s);
    off(key);
    render(tail_s);
    return mono();
  }
  double rate() const { return rate_; }
 private:
  double rate_;
};

// One voice on its own, dry and untrimmed: what a recipe's physics makes.
inline std::vector<float> voice(int sound, const bonkr::dsp::Play& play, double seconds, double rate = kRate, double hold_s = -1.0) {
  auto v = std::make_unique<bonkr::dsp::Voice>();
  v->prepare(rate);
  v->start(sound, play);
  std::vector<float> out(static_cast<size_t>(seconds * rate));
  const size_t release = hold_s < 0.0 ? out.size() : static_cast<size_t>(hold_s * rate);
  for (size_t i = 0; i < out.size(); ++i) {
    if (i == release) v->release();
    out[i] = v->process();
  }
  return out;
}
inline bonkr::dsp::Play play(double velocity = 0.8) {
  bonkr::dsp::Play p;
  p.velocity = velocity;
  return p;
}

inline bool finite(const std::vector<float>& x) {
  for (float v : x) if (!std::isfinite(v)) return false;
  return true;
}
inline double peak(const std::vector<float>& x) { return measure::peak(measure::Span(x)); }

// The strongest spectral peaks of a span: the frequencies of the local maxima of a long FFT,
// refined parabolically, strongest first.
inline std::vector<double> peaks_hz(const measure::Span& s, double rate, int count, double lo = 20.0, double hi = 20000.0, int order = 16) {
  const auto p = measure::power_spectrum(s, order);
  const double bin = rate / static_cast<double>(p.size() * 2);
  std::vector<std::pair<double, double>> found;
  for (size_t k = 2; k + 2 < p.size(); ++k) {
    const double hz = static_cast<double>(k) * bin;
    if (hz < lo || hz > hi) continue;
    if (p[k] > p[k - 1] && p[k] >= p[k + 1] && p[k] > p[k - 2] && p[k] >= p[k + 2]) {
      const double a = std::log(p[k - 1] + 1e-30), b = std::log(p[k] + 1e-30), c = std::log(p[k + 1] + 1e-30);
      const double d = 0.5 * (a - c) / (a - 2.0 * b + c);
      found.push_back({p[k], (static_cast<double>(k) + d) * bin});
    }
  }
  std::sort(found.begin(), found.end(), [](auto& x, auto& y) { return x.first > y.first; });
  std::vector<double> out;
  for (const auto& f : found) {
    bool near = false;
    for (double o : out) if (std::abs(std::log2(o / f.second)) < 0.01) near = true;
    if (!near) out.push_back(f.second);
    if (static_cast<int>(out.size()) >= count) break;
  }
  return out;
}
// The strongest peak within [lo, hi].
inline double strongest_hz(const measure::Span& s, double rate, double lo, double hi, int order = 16) {
  const auto p = peaks_hz(s, rate, 1, lo, hi, order);
  return p.empty() ? 0.0 : p[0];
}
// The time, in seconds, the RMS contour first falls 40 dB below its peak after it.
inline double ring_s(const std::vector<float>& x, double rate, double drop_db = 40.0) {
  const auto c = measure::contour_db(x, rate, 0.005, 0.010);
  if (c.empty()) return 0.0;
  size_t top = 0;
  for (size_t i = 0; i < c.size(); ++i) if (c[i] > c[top]) top = i;
  for (size_t i = top; i < c.size(); ++i) if (c[i] < c[top] - drop_db) return static_cast<double>(i - top) * 0.005;
  return static_cast<double>(c.size() - top) * 0.005;
}

}  // namespace rig
