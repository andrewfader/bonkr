// The drum-stem features of the song corpus (song-corpus/scripts/features.py), measured exactly as
// it measures a separated drum stem, so a render of a kit and the drums of a record compare number
// for number:
//
//   spectral: the long-term power spectrum (STFT n_fft 8192, hop 2048, Hann, centred frames padded
//             with zeros, as librosa 1.0 computes it) averaged over frames within 40 dB of the
//             loudest; the centroid, the 85% roll-off and the share below 250 Hz, all from 20 Hz.
//   crest:    the median over 100 ms blocks (within 20 dB of the loudest) of peak over RMS, dB.
//   stereo:   the zero-lag correlation of the two channels over the whole band.
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

#include "audio_measure.hpp"

namespace stem {

struct Features { double centroid_hz = 0.0, rolloff85_hz = 0.0, low_share_db = 0.0, crest100_db = 0.0, corr = 1.0; };

inline double median(std::vector<double> v) {
  if (v.empty()) return 0.0;
  std::sort(v.begin(), v.end());
  const size_t n = v.size();
  return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

inline Features measure(const std::vector<float>& left, const std::vector<float>& right, double rate) {
  Features f;
  std::vector<double> y(left.size());
  for (size_t i = 0; i < y.size(); ++i) y[i] = 0.5 * (static_cast<double>(left[i]) + right[i]);
  {
    constexpr long kFft = 8192, kHop = 2048;
    const long n = static_cast<long>(y.size()), frames = 1 + n / kHop;
    std::vector<double> window(kFft);
    for (long i = 0; i < kFft; ++i) window[static_cast<size_t>(i)] = 0.5 - 0.5 * std::cos(2.0 * measure::kPi * static_cast<double>(i) / kFft);
    std::vector<std::vector<double>> S;
    std::vector<double> energy;
    std::vector<std::complex<double>> a(kFft);
    for (long t = 0; t < frames; ++t) {
      const long start = t * kHop - kFft / 2;
      for (long i = 0; i < kFft; ++i) {
        const long j = start + i;
        a[static_cast<size_t>(i)] = (j >= 0 && j < n ? y[static_cast<size_t>(j)] : 0.0) * window[static_cast<size_t>(i)];
      }
      measure::fft(a);
      std::vector<double> p(kFft / 2 + 1);
      double e = 0.0;
      for (size_t k = 0; k < p.size(); ++k) { p[k] = std::norm(a[k]); e += p[k]; }
      S.push_back(std::move(p));
      energy.push_back(e);
    }
    const double loudest = *std::max_element(energy.begin(), energy.end());
    std::vector<double> P(kFft / 2 + 1, 0.0);
    for (size_t t = 0; t < S.size(); ++t)
      if (energy[t] > loudest * 1e-4) for (size_t k = 0; k < P.size(); ++k) P[k] += S[t][k];
    const auto hz = [&](size_t k) { return static_cast<double>(k) * rate / kFft; };
    double sum = 0.0, weighted = 0.0, low = 0.0;
    for (size_t k = 0; k < P.size(); ++k) {
      if (hz(k) < 20.0) continue;
      sum += P[k]; weighted += P[k] * hz(k);
      if (hz(k) < 250.0) low += P[k];
    }
    f.centroid_hz = weighted / sum;
    f.low_share_db = 10.0 * std::log10(std::max(low / sum, 1e-20));
    double running = 0.0;
    for (size_t k = 0; k < P.size(); ++k) {
      if (hz(k) < 20.0) continue;
      running += P[k];
      if (running / sum >= 0.85) { f.rolloff85_hz = hz(k); break; }
    }
  }
  {
    const size_t hop = static_cast<size_t>(0.1 * rate), blocks = y.size() / hop;
    std::vector<double> rms(blocks), peak(blocks);
    for (size_t b = 0; b < blocks; ++b) {
      double acc = 0.0;
      for (size_t i = b * hop; i < (b + 1) * hop; ++i) { acc += y[i] * y[i]; peak[b] = std::max(peak[b], std::abs(y[i])); }
      rms[b] = std::sqrt(acc / static_cast<double>(hop)) + 1e-12;
    }
    const double loudest = blocks ? *std::max_element(rms.begin(), rms.end()) : 0.0;
    std::vector<double> crest;
    for (size_t b = 0; b < blocks; ++b) if (rms[b] > loudest * 0.1) crest.push_back(20.0 * std::log10(peak[b] / rms[b]));
    f.crest100_db = median(crest);
  }
  {
    double ll = 0.0, rr = 0.0, lr = 0.0;
    for (size_t i = 0; i < left.size(); ++i) {
      ll += static_cast<double>(left[i]) * left[i]; rr += static_cast<double>(right[i]) * right[i]; lr += static_cast<double>(left[i]) * right[i];
    }
    f.corr = lr / (std::sqrt(ll * rr) + 1e-20);
  }
  return f;
}

}  // namespace stem
