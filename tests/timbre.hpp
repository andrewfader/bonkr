// The features a percussive sound is compared on, measured the same way from a recording and
// from BONKR: from the onset (where the sound first comes within 30 dB of its peak) over a window
// long enough to hold what makes the sound (half a second for a drum, two for a cymbal or a
// passing jet):
//
// - profile: the energy in each octave band from 31.5 Hz to 16 kHz, in dB against the whole,
//   floored at -45 dB: where the sound's weight lies;
// - centroid: the spectral centroid, in octaves (log2 Hz): how bright it is;
// - decay: how long, in ms, its level (10 ms RMS) takes from the peak to fall 20 dB, capped at
//   the window: how long it rings;
// - attack: from the onset to the peak, in ms;
// - flatness: the spectrum's flatness from 100 Hz to 16 kHz (geometric over arithmetic mean of the
//   power, in dB), each bin floored 60 dB under the strongest so a recording's noise floor and a
//   model's silence between its modes count alike: 0 for white noise, about -40 for a few clean
//   modes. How noisy it is.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "audio_measure.hpp"

namespace timbre {

constexpr int kBands = 10;
constexpr double kFloorDb = -45.0;
inline double band_centre(int b) { return 31.5 * std::exp2(b); }

struct Features {
  std::array<double, kBands> profile{};
  double centroid = 0.0, decay_ms = 0.0, attack_ms = 0.0, flatness_db = 0.0;
};

inline Features extract(const std::vector<float>& x, double rate, double window_s) {
  Features f;
  double peak = 0.0;
  size_t peak_at = 0;
  for (size_t i = 0; i < x.size(); ++i) if (std::abs(x[i]) > peak) { peak = std::abs(x[i]); peak_at = i; }
  if (peak <= 0.0) return f;
  size_t onset = 0;
  while (onset < peak_at && std::abs(x[onset]) < peak * 0.0316) ++onset;  // -30 dB
  const size_t n = std::min(x.size() - onset, static_cast<size_t>(window_s * rate));
  const measure::Span span(x, onset, n);
  // The spectrum of the window, Hann-weighted frames averaged.
  int order = 12;
  while (order > 8 && (size_t{1} << order) > n) --order;
  const auto p = measure::power_spectrum(span, order);
  const double bin = rate / static_cast<double>(p.size() * 2);
  std::array<double, kBands> energy{};
  double total = 0.0, num = 0.0, logsum = 0.0, linsum = 0.0, strongest = 0.0;
  int flat_bins = 0;
  for (size_t k = 1; k < p.size(); ++k) strongest = std::max(strongest, p[k]);
  const double floor = strongest * 1e-6;
  for (size_t k = 1; k < p.size(); ++k) {
    const double hz = static_cast<double>(k) * bin;
    total += p[k];
    num += hz * p[k];
    for (int b = 0; b < kBands; ++b)
      if (hz >= band_centre(b) / std::sqrt(2.0) && hz < band_centre(b) * std::sqrt(2.0)) energy[static_cast<size_t>(b)] += p[k];
    if (hz >= 100.0 && hz <= 16000.0) { const double q = std::max(p[k], floor); logsum += std::log(q + 1e-30); linsum += q; ++flat_bins; }
  }
  for (int b = 0; b < kBands; ++b) f.profile[static_cast<size_t>(b)] = std::max(kFloorDb, 10.0 * std::log10(energy[static_cast<size_t>(b)] / std::max(total, 1e-30) + 1e-30));
  f.centroid = std::log2(std::max(num / std::max(total, 1e-30), 1.0));
  f.flatness_db = flat_bins ? 10.0 * std::log10(std::exp(logsum / flat_bins) / std::max(linsum / flat_bins, 1e-30) + 1e-30) : 0.0;
  // Decay: the 10 ms RMS contour from its peak to 20 dB under it.
  const auto c = measure::contour_db(std::vector<float>(x.begin() + static_cast<long>(onset), x.begin() + static_cast<long>(onset + n)), rate, 0.002, 0.010);
  // From the first moment within 1 dB of the loudest (a sustained sound's level wanders a little).
  size_t loudest = 0;
  for (size_t i = 0; i < c.size(); ++i) if (c[i] > c[loudest]) loudest = i;
  size_t top = 0;
  while (top < loudest && c[top] < c[loudest] - 1.0) ++top;
  size_t fall = c.size();
  for (size_t i = top; i < c.size(); ++i) if (c[i] < c[loudest] - 20.0) { fall = i; break; }
  f.decay_ms = static_cast<double>(fall - top) * 2.0;
  f.attack_ms = static_cast<double>(peak_at - onset) / rate * 1000.0;
  return f;
}

}  // namespace timbre
