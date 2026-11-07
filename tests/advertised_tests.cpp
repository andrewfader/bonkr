// Every parameter, proved against what it advertises, from audio, through the built plug-in.
//
// The plug-in advertises each parameter as a name, a unit and a range, and the editor's help
// text says what it does. This suite holds one proof per parameter ID: a musical context (a kit
// or one sound, a few settings, the keys struck), two values of the parameter, and a measurement
// in the advertised unit comparing the two renders: cents for pitch, dB for level, milliseconds
// for times, a ratio of ring times, a count of strokes, or the change of a measured quantity (a
// spectral centroid, a band's energy) for the rest.
//
// The table is indexed by ID and the suite fails if any parameter has no proof, so a new
// parameter cannot ship without one.
//
//   advertised_tests <BONKR.clap> [name-filter]

#include "../src/dsp/air.hpp"
#include "../src/dsp/core.hpp"
#include "../src/dsp/catalog.hpp"
#include "../src/dsp/kits.hpp"
#include "../src/presets.hpp"
#include "../src/params.hpp"
#include "audio_measure.hpp"
#include "clap_harness.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace harness;
using measure::Span;
namespace P = bonkr::params;

namespace {

// One sample rate throughout: the plug-in calibrates its levels once per rate per process.
constexpr double kRate = 48000.0;
constexpr uint32_t kBlock = 256;
// The plug-in stays loaded for the whole run, so that the calibration is done only once.
std::unique_ptr<Library> g_library;
double g_note_hz = 261.63;

struct Setting { uint32_t id; double value; };

// Something played at a time: a key, or a MIDI message (a controller, pressure, the bend).
struct Timed {
  double at_s;
  enum Kind { On, Off, Midi } kind;
  int key = 0;
  double velocity = 0.7;
  uint8_t status = 0, data1 = 0, data2 = 0;
};
Timed key_on(double at, int key, double velocity = 0.7) { return {at, Timed::On, key, velocity}; }
Timed key_off(double at, int key) { return {at, Timed::Off, key}; }

// The musical situation a parameter is proved in: one key held from the start for hold_s, then
// a tail, and whatever else is played meanwhile.
struct Context {
  int sound = -1;  // one sound across the keyboard, or -1 for the kit below
  int kit = 0;
  std::vector<Setting> settings;
  int key = 60;
  double velocity = 0.7;
  double hold_s = 1.0, tail_s = 0.5;
  std::vector<Timed> extra;
  double hz = 0.0;     // the pitch the measurements look near (0: the key's)
  double tempo = 0.0;  // a playing transport at this tempo
};

struct Render { std::vector<float> mono, left, right; size_t note_off = 0; };

size_t at(double seconds) { return static_cast<size_t>(seconds * kRate); }

Render render(const Context& c, uint32_t id, double value) {
  const clap_plugin_t* plugin = g_library->create();
  if (c.sound >= 0) { set_param(plugin, P::kLayout, P::kLayoutChromatic); set_param(plugin, P::kSound, c.sound); }
  else set_param(plugin, P::kKit, c.kit);
  for (const Setting& s : c.settings) set_param(plugin, s.id, s.value);
  set_param(plugin, id, value);
  Runner runner(plugin, kRate, kBlock);
  runner.tempo = c.tempo;
  Render out;
  out.note_off = at(c.hold_s);
  const size_t total = at(c.hold_s + c.tail_s);
  std::vector<Timed> timeline;
  timeline.push_back(key_on(0.0, c.key, c.velocity));
  timeline.push_back(key_off(c.hold_s, c.key));
  timeline.insert(timeline.end(), c.extra.begin(), c.extra.end());
  // Controllers before notes at the same instant, so a note starts with them already set.
  std::stable_sort(timeline.begin(), timeline.end(), [](const Timed& a, const Timed& b) {
    if (at(a.at_s) != at(b.at_s)) return at(a.at_s) < at(b.at_s);
    return a.kind == Timed::Midi && b.kind != Timed::Midi;
  });
  size_t next = 0;
  Events events;
  for (size_t rendered = 0; rendered < total; rendered += kBlock) {
    events.clear();
    while (next < timeline.size() && at(timeline[next].at_s) < rendered + kBlock) {
      const Timed& t = timeline[next++];
      const uint32_t offset = static_cast<uint32_t>(std::max(at(t.at_s), rendered) - rendered);
      if (t.kind == Timed::Midi) {
        clap_event_midi_t e = midi_event(t.status, t.data1, t.data2);
        e.header.time = offset;
        events.push(e);
      } else {
        clap_event_note_t e = note_event(t.kind == Timed::On ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF, 0, static_cast<int16_t>(t.key), -1, t.kind == Timed::On ? t.velocity : 0.0);
        e.header.time = offset;
        events.push(e);
      }
    }
    runner.run(&events);
    for (uint32_t i = 0; i < kBlock; ++i) {
      out.left.push_back(runner.left()[i]); out.right.push_back(runner.right()[i]);
      out.mono.push_back(0.5f * (runner.left()[i] + runner.right()[i]));
    }
  }
  runner.destroy();
  return out;
}

// --- measurements --------------------------------------------------------------------------------

Span window(const Render& r, double from_s, double to_s) { return Span(r.mono, at(from_s), at(to_s) - at(from_s)); }
double level_db(const Span& s) { return measure::db(measure::rms(s)); }
double centroid(const Span& s) { return measure::spectral_centroid(s, kRate, 13); }
// A tuner's reading of the strongest component near a frequency: its amplitude, and its pitch.
double peak_hz(const Span& s, double around, double span_cents = 60.0, double step = 0.5) {
  double best = -1.0, best_hz = around;
  for (double c = -span_cents; c <= span_cents; c += step) {
    const double f = around * std::exp2(c / 1200.0);
    const double a = measure::tone_amplitude(s, kRate, f);
    if (a > best) { best = a; best_hz = f; }
  }
  // Refined to a tenth of a cent around the coarse peak.
  const double coarse = best_hz;
  for (double c = -step; c <= step; c += 0.1) {
    const double f = coarse * std::exp2(c / 1200.0);
    const double a = measure::tone_amplitude(s, kRate, f);
    if (a > best) { best = a; best_hz = f; }
  }
  return best_hz;
}
double tone_near(const Span& s, double hz, double span_cents = 30.0) {
  double best = 0.0;
  for (double c = -span_cents; c <= span_cents; c += 2.0) best = std::max(best, measure::tone_amplitude(s, kRate, hz * std::exp2(c / 1200.0)));
  return best;
}
double tone_db(const Span& s, double hz, double span_cents = 30.0) { return measure::db(tone_near(s, hz, span_cents)); }
// The energy in a band, in dB: its share of the spectrum times the mean square.
double band_db(const Span& s, double lo, double hi) {
  const double rms = measure::rms(s);
  return 10.0 * std::log10(std::max(measure::band_fraction(s, kRate, lo, hi) * rms * rms, 1e-24));
}
// Level contour every 2 ms over 4 ms windows.
std::vector<double> contour(const Render& r) { return measure::contour_db(r.mono, kRate, 0.002, 0.004); }
// How much later, in seconds, the note starts in b than in a: the start of a (from just before
// it is first heard to 10 ms in, where nothing is periodic yet) found in b by normalised
// cross-correlation.
double lag_s(const Render& a, const Render& b, double max_s) {
  double top = 0.0;
  for (size_t i = 0; i < at(0.5); ++i) top = std::max(top, static_cast<double>(std::abs(a.mono[i])));
  size_t start = 0;
  while (start < at(0.5) && std::abs(a.mono[start]) < 1e-3 * top) ++start;
  const size_t from = start > at(0.002) ? start - at(0.002) : 0, n = at(0.012), max_lag = at(max_s);
  double best = -1e30; size_t best_lag = 0;
  std::vector<double> r(max_lag + 1, 0.0);
  for (size_t lag = 0; lag <= max_lag; ++lag) {
    double num = 0.0, ea = 0.0, eb = 0.0;
    for (size_t i = from; i < from + n; ++i) {
      const double x = a.mono[i], y = b.mono[i + lag];
      num += x * y; ea += x * x; eb += y * y;
    }
    r[lag] = num / std::sqrt(std::max(ea * eb, 1e-30));
    if (r[lag] > best) { best = r[lag]; best_lag = lag; }
  }
  double shift = 0.0;
  if (best_lag > 0 && best_lag < max_lag) {
    const double d = r[best_lag - 1] - 2.0 * r[best_lag] + r[best_lag + 1];
    if (std::abs(d) > 1e-12) shift = std::clamp(0.5 * (r[best_lag - 1] - r[best_lag + 1]) / d, -0.5, 0.5);
  }
  return (static_cast<double>(best_lag) + shift) / kRate;
}

// --- verdicts --------------------------------------------------------------------------------------

struct Verdict { bool ok; std::string detail; };
Verdict verdict(bool ok, const char* format, double a = 0.0, double b = 0.0, double c = 0.0, double d = 0.0) {
  char out[320]; std::snprintf(out, sizeof(out), format, a, b, c, d); return {ok, out};
}
using Check = std::function<Verdict(const Render&, const Render&)>;

struct Proof {
  Context context;
  double low, high;
  Check check;
};

// The tail after release, 50 to 600 ms after the key is let go, rises by at least min_db.
Check LongerTail(double min_db) {
  return [=](const Render& a, const Render& b) {
    const double d = level_db(Span(b.mono, b.note_off + at(0.05), at(0.55))) - level_db(Span(a.mono, a.note_off + at(0.05), at(0.55)));
    return verdict(d >= min_db, "tail level moved %+.1f dB (expected at least %+.1f)", d, min_db);
  };
}
// The time the level contour takes to fall `drop_db` below its peak, in ms.
double ring_ms(const Render& r, double drop_db = 40.0) {
  const auto c = contour(r);
  size_t top = 0;
  for (size_t i = 0; i < c.size(); ++i) if (c[i] > c[top]) top = i;
  for (size_t i = top; i < c.size(); ++i) if (c[i] < c[top] - drop_db) return static_cast<double>(i - top) * 2.0;
  return static_cast<double>(c.size() - top) * 2.0;
}
// Strokes heard: level jumps of more than 12 dB in 2 ms, at least 40 ms apart.
int strokes(const Render& r) {
  const auto c = contour(r);
  int n = c.empty() || c[0] < -70.0 ? 0 : 1;  // a stroke at the very start has no rise before it
  for (size_t i = 1; i < c.size(); ++i) if (c[i] > c[i - 1] + 12.0 && c[i] > -70.0) { ++n; i += 20; }
  return n;
}
double onset_s(const Render& r, size_t from = 0) {
  double top = 0.0;
  for (float v : r.mono) top = std::max(top, static_cast<double>(std::abs(v)));
  for (size_t i = from; i < r.mono.size(); ++i) if (std::abs(r.mono[i]) > 1e-3 * top) return static_cast<double>(i) / kRate;
  return 1e9;
}

Context sound(int s, std::vector<Setting> settings = {}, int key = 60) { Context c; c.sound = s; c.settings = std::move(settings); c.key = key; return c; }
Context kit(int k, std::vector<Setting> settings = {}, int key = 38) { Context c; c.kit = k; c.settings = std::move(settings); c.key = key; return c; }
// The vuvuzela held: a steady 233 Hz tone with harmonics to the top, for the room and the air.
Context horn(std::vector<Setting> settings = {}) { Context c = sound(bonkr::dsp::kVuvuzela, std::move(settings)); c.hz = 233.0; return c; }

std::array<Proof, P::kParamCount> proofs() {
  using namespace P;
  namespace D = bonkr::dsp;
  std::array<Proof, kParamCount> t{};
  const auto set = [&](ParamId id, Context c, double low, double high, Check check) { t[id] = Proof{std::move(c), low, high, std::move(check)}; };

  // --- the kit --------------------------------------------------------------------------------------
  // Key 60 is the high bongo in the kit; played as one sound, the timpani, at its D3.
  {
    Context c = kit(D::kits::kStudioRock, {{kSound, D::kTimpani}}, 60); c.hz = 146.8;
    set(kLayout, c, kLayoutKit, kLayoutChromatic, [](const Render& a, const Render& b) {
      const double d = tone_db(window(b, 0.05, 0.6), 146.8) - tone_db(window(a, 0.05, 0.6), 146.8);
      return verdict(d >= 15.0, "the timpani's D3 rose %+.1f dB (expected at least 15)", d);
    });
  }
  // The cowbell key: a real cowbell at 562 Hz, then the 808's squares at 540 and 800 Hz.
  set(kKit, kit(D::kits::kStudioRock, {}, 56), D::kits::kStudioRock, D::kits::kKit808, [](const Render& a, const Render& b) {
    const double d = tone_db(window(b, 0.0, 0.2), 800.0, 10.0) - tone_db(window(a, 0.0, 0.2), 800.0, 10.0);
    return verdict(d >= 10.0, "800 Hz rose %+.1f dB (expected at least 10)", d);
  });
  set(kSound, sound(D::kTimpani), D::kTimpani, D::kClaves, [](const Render& a, const Render& b) {
    const double ca = centroid(window(a, 0.0, 0.1)), cb = centroid(window(b, 0.0, 0.1));
    return verdict(cb > 5.0 * ca, "centroid %.0f Hz -> %.0f Hz (expected five times higher)", ca, cb);
  });
  set(kMasterDb, horn(), -30, -6, [](const Render& a, const Render& b) {
    const double d = level_db(window(b, 0.4, 0.8)) - level_db(window(a, 0.4, 0.8));
    return verdict(std::abs(d - 24.0) <= 0.3, "level moved %+.2f dB (expected 24 +- 0.3)", d);
  });
  set(kTune, horn(), 0, 12, [](const Render& a, const Render& b) {
    const double pa = peak_hz(window(a, 0.4, 0.8), 233.0, 60.0), pb = peak_hz(window(b, 0.4, 0.8), 466.0, 60.0);
    const double cents = measure::cents(pb, pa);
    return verdict(std::abs(cents - 1200.0) <= 3.0, "pitch moved %+.1f cents (expected 1200 +- 3)", cents);
  });

  // --- the stroke -----------------------------------------------------------------------------------
  // The timpani's own felt mallet, then a stick: a stick's contact is a quarter as long.
  const Check brighter = [](const Render& a, const Render& b) {
    const double d = band_db(window(b, 0.0, 0.1), 1000.0, 8000.0) - band_db(window(a, 0.0, 0.1), 1000.0, 8000.0);
    return verdict(d >= 6.0, "the stroke's energy from 1 to 8 kHz moved %+.1f dB (expected at least 6)", d);
  };
  // The triangle: a felt mallet's long contact leaves its high bending modes still; its own steel
  // beater, or a hard face, rings them.
  set(kStriker, sound(D::kTriangle), D::kFeltMallet, D::kAsPlayed, brighter);
  set(kHardness, sound(D::kTriangle), 0.0, 1.0, brighter);
  // A frame drum struck near its centre rings almost only its breathing modes; toward the edge
  // its (1,1) mode, at 1.594 f01 = 87.2 Hz, speaks.
  set(kPosition, sound(D::kFrameDrum, {{kTension, 0}}), -1.0, 0.3, [](const Render& a, const Render& b) {
    const double d = tone_db(window(b, 0.02, 0.5), 87.2, 20.0) - tone_db(window(a, 0.02, 0.5), 87.2, 20.0);
    return verdict(d >= 6.0, "the (1,1) mode rose %+.1f dB (expected at least 6)", d);
  });
  // A soft stroke (velocity 0.3) with dynamics off is played at the mezzo-forte 2.2 m/s; with it
  // on, at its own 1.1 m/s: about 6 dB quieter.
  {
    Context c = kit(D::kits::kStudioRock, {}, 45); c.velocity = 0.3;
    set(kDynamics, c, 0.0, 1.0, [](const Render& a, const Render& b) {
      const double d = measure::db(measure::peak(Span(b.mono))) - measure::db(measure::peak(Span(a.mono)));
      return verdict(d <= -3.0 && d >= -12.0, "peak moved %+.1f dB (expected -3 to -12)", d);
    });
  }

  // --- the drum -------------------------------------------------------------------------------------
  {
    Context c = sound(D::kTomMid); c.hold_s = 0.1; c.tail_s = 3.0;
    set(kDecay, c, 1.0, 2.0, [](const Render& a, const Render& b) {
      const double ra = ring_ms(a), rb = ring_ms(b);
      return verdict(rb / ra > 1.6 && rb / ra < 3.0, "rings %.0f ms -> %.0f ms (expected about twice as long)", ra, rb);
    });
    set(kDamping, c, 0.0, 0.8, [](const Render& a, const Render& b) {
      const double ra = ring_ms(a), rb = ring_ms(b);
      return verdict(rb < 0.7 * ra, "rings %.0f ms -> %.0f ms (expected under 70%%)", ra, rb);
    });
  }
  // The floor tom struck as hard as possible: the drop of its lowest mode from the first 60 ms to
  // half a second later, with no tension modulation and with three times the default.
  {
    Context c = sound(D::kTomFloor); c.velocity = 1.0; c.hold_s = 0.1; c.tail_s = 0.8; c.hz = 85.0;
    set(kTension, c, 0.0, 3.0, [](const Render& a, const Render& b) {
      const auto glide = [](const Render& r) { return measure::cents(peak_hz(window(r, 0.0, 0.06), 85.0, 400.0, 2.0), peak_hz(window(r, 0.5, 0.75), 85.0, 400.0, 2.0)); };
      const double ga = glide(a), gb = glide(b);
      return verdict(std::abs(ga) < 25.0 && gb > ga + 60.0, "glide %.0f cents -> %.0f cents (expected none, then at least 60 more)", ga, gb);
    });
  }
  set(kSnares, kit(D::kits::kStudioRock), 0.0, 1.0, [](const Render& a, const Render& b) {
    const double d = band_db(window(b, 0.015, 0.15), 3000.0, 16000.0) - band_db(window(a, 0.015, 0.15), 3000.0, 16000.0);
    return verdict(d >= 6.0, "energy above 3 kHz after the stick moved %+.1f dB (expected at least 6)", d);
  });
  set(kSnareTension, kit(D::kits::kStudioRock), 0.0, 1.0, [](const Render& a, const Render& b) {
    const double d = level_db(window(b, 0.06, 0.16)) - level_db(window(a, 0.06, 0.16));
    return verdict(d <= -1.0, "the buzz after 60 ms moved %+.1f dB (expected tighter wires to buzz less)", d);
  });
  // The kick's resonant head: the pair of breathing modes rings longer with it.
  set(kResonance, kit(D::kits::kStudioRock, {}, 36), 0.0, 1.0, [](const Render& a, const Render& b) {
    const double d = level_db(window(b, 0.15, 0.3)) - level_db(window(a, 0.15, 0.3));
    return verdict(d >= 2.0, "the boom at 150-300 ms moved %+.1f dB (expected at least 2)", d);
  });

  // --- the machine ----------------------------------------------------------------------------------
  // The SP-1200's converter holds each sample for 1 / 26.04 kHz: the output changes 26040 times a
  // second instead of every sample.
  set(kConverter, kit(D::kits::kStudioRock, {}, 45), D::kits::kConvClean, D::kits::kConvSp1200, [](const Render& a, const Render& b) {
    // Over the stroke's first 20 ms, while it is loud enough that every clock tick holds a new step.
    const auto changes = [](const Render& r) { int n = 0; for (size_t i = 1; i < at(0.02); ++i) n += r.left[i] != r.left[i - 1]; return n / 0.02; };
    const double ca = changes(a), cb = changes(b);
    return verdict(ca > 40000.0 && std::abs(cb - 26040.0) < 1300.0, "the output changed %.0f -> %.0f times a second (expected every sample, then 26040)", ca, cb);
  });
  // The closed hat on the second sixteenth of the bar at 120 bpm (a key struck at 125 ms), at 66%
  // swing: (0.66 - 0.5) of an eighth (250 ms) later, 40 ms.
  {
    Context c = kit(D::kits::kStudioRock, {}, 0); c.tempo = 120.0; c.hold_s = 0.05; c.tail_s = 0.4;  // key 0 is silent
    c.extra.push_back(key_on(0.125, 42, 0.8)); c.extra.push_back(key_off(0.18, 42));
    set(kSwing, c, 0.5, 0.66, [](const Render& a, const Render& b) {
      const double oa = onset_s(a, at(0.1)), ob = onset_s(b, at(0.1));
      const double ms = 1000.0 * (ob - oa);
      return verdict(std::abs(ms - 40.0) <= 1.0, "the off-beat hat moved %+.1f ms (expected 40 +- 1)", ms);
    });
  }
  // Held for a second at 120 bpm: off, one stroke; at 1/16, one every 125 ms.
  {
    Context c = kit(D::kits::kStudioRock, {}, 38); c.tempo = 120.0; c.hold_s = 0.99; c.tail_s = 0.2;
    set(kNoteRepeat, c, 0, 4, [](const Render& a, const Render& b) {
      const int sa = strokes(a), sb = strokes(b);
      return verdict(sa == 1 && sb == 8, "%.0f -> %.0f strokes (expected 1, then 8)", sa, sb);
    });
  }
  set(kDrive, kit(D::kits::kKit808, {}, 36), 0.0, 1.0, [](const Render& a, const Render& b) {
    const auto crest = [](const Render& r) { return measure::db(measure::peak(Span(r.mono, 0, at(0.4)))) - level_db(Span(r.mono, 0, at(0.4))); };
    const double ca = crest(a), cb = crest(b);
    return verdict(cb < ca - 2.0, "crest factor %.1f dB -> %.1f dB (expected at least 2 dB flatter)", ca, cb);
  });
  // The open hat, then the pedal closed at 150 ms.
  {
    Context c = kit(D::kits::kStudioRock, {}, 46); c.hold_s = 0.1; c.tail_s = 0.6;
    c.extra.push_back(key_on(0.15, 44, 0.1)); c.extra.push_back(key_off(0.2, 44));
    set(kChoke, c, 0.0, 1.0, [](const Render& a, const Render& b) {
      const double d = level_db(window(b, 0.35, 0.45)) - level_db(window(a, 0.35, 0.45));
      return verdict(d <= -20.0, "the open hat at 350 ms moved %+.1f dB (expected at least 20 dB down)", d);
    });
  }

  // --- the studio -----------------------------------------------------------------------------------
  {
    Context c = kit(D::kits::kStudioRock, {{kRoomSize, 4}, {kMicDistance, 2.0}}, 43); c.velocity = 0.9; c.hold_s = 0.1; c.tail_s = 0.6;
    set(kCrush, c, 0.0, 0.8, [](const Render& a, const Render& b) {
      const auto ratio = [](const Render& r) { return level_db(window(r, 0.3, 0.4)) - measure::db(measure::peak(Span(r.mono, 0, at(0.1)))); };
      const double ra = ratio(a), rb = ratio(b);
      return verdict(rb > ra + 10.0, "tail against the hit %.1f dB -> %.1f dB (expected at least 10 dB closer)", ra, rb);
    });
  }
  {
    Context c = kit(D::kits::kStudioRock, {{kRoomSize, 5}, {kMicDistance, 3.0}, {kRoomAbsorption, 0.1}, {kGateHold, 250}}); c.velocity = 0.9; c.hold_s = 0.1; c.tail_s = 1.0;
    set(kGate, c, 0.0, 0.6, [](const Render& a, const Render& b) {
      const double d = level_db(window(b, 0.32, 0.42)) - level_db(window(a, 0.32, 0.42));
      return verdict(d <= -30.0, "the room at 320 ms moved %+.1f dB (expected at least 30 dB down)", d);
    });
    Context h = c;
    h.settings = {{kRoomSize, 5}, {kMicDistance, 3.0}, {kRoomAbsorption, 0.1}, {kGate, 0.6}};
    set(kGateHold, h, 100.0, 400.0, [](const Render& a, const Render& b) {
      const double d = level_db(window(b, 0.2, 0.3)) - level_db(window(a, 0.2, 0.3));
      return verdict(d >= 20.0, "the room at 200 ms moved %+.1f dB (expected at least 20 dB up: still open)", d);
    });
  }
  // The hi-hat sits right of centre; at full width it is heard there, at none in the middle.
  set(kStereoWidth, kit(D::kits::kStudioRock, {}, 42), 0.0, 1.0, [](const Render& a, const Render& b) {
    const auto balance = [](const Render& r) { return level_db(Span(r.right, 0, at(0.2))) - level_db(Span(r.left, 0, at(0.2))); };
    const double ba = balance(a), bb = balance(b);
    return verdict(std::abs(ba) < 0.1 && bb > 3.0, "right over left %+.2f dB -> %+.2f dB (expected 0, then at least 3)", ba, bb);
  });
  // Equal-power panning of a centred sound: at R 50 the right channel is tan(3 pi / 8), 7.66 dB,
  // over the left; at L 50 the other way round.
  set(kPan, horn(), -0.5, 0.5, [](const Render& a, const Render& b) {
    const auto balance = [](const Render& r) { return level_db(Span(r.right, at(0.3), at(0.5))) - level_db(Span(r.left, at(0.3), at(0.5))); };
    const double ba = balance(a), bb = balance(b), expected = measure::db(std::tan(3.0 * bonkr::dsp::kPi / 8.0));
    return verdict(std::abs(ba + expected) <= 0.3 && std::abs(bb - expected) <= 0.3, "right over left %+.2f dB -> %+.2f dB (expected -+%.2f +- 0.3)", ba, bb, expected);
  });

  // --- the room and the air ----------------------------------------------------------------------------
  { Context c = horn({{kMicDistance, 2.0}}); c.tail_s = 0.8; set(kRoomSize, c, 0, 3, LongerTail(10.0)); }
  {
    Context c = horn({{kRoomSize, 5}, {kMicDistance, 3.0}}); c.hold_s = 1.5; c.tail_s = 1.0;
    set(kRoomAbsorption, c, 0.08, 0.6, [](const Render& a, const Render& b) {
      const double d = level_db(Span(b.mono, b.note_off + at(0.15), at(0.55))) - level_db(Span(a.mono, a.note_off + at(0.15), at(0.55)));
      return verdict(d <= -6.0, "tail level moved %+.1f dB (expected at most -6)", d);
    });
  }
  // The microphone 10 m from the claves in 20 C air: the stroke arrives 10 / 343.2 s = 29.1 ms later.
  set(kMicDistance, sound(D::kClaves), 0, 10, [](const Render& a, const Render& b) {
    const double ms = 1000.0 * lag_s(a, b, 0.05), expected = 1000.0 * 10.0 / (331.3 * std::sqrt(1.0 + 20.0 / 273.15));
    return verdict(std::abs(ms - expected) <= 0.2, "arrival %+.2f ms later (expected %+.2f +- 0.2)", ms, expected);
  });
  // Twelve metres of open air from the vuvuzela: its 30th harmonic near 7 kHz loses what ISO
  // 9613-1 gives at 10% and at 70% humidity, humid air taking less.
  set(kHumidity, horn({{kMicDistance, 12.0}}), 0.1, 0.7, [](const Render& a, const Render& b) {
    const double hz = peak_hz(window(a, 0.4, 0.9), 30.0 * 233.0, 30.0);
    const double expected = 12.0 * (bonkr::dsp::air::absorption_db_per_m(hz, 20.0, 0.1) - bonkr::dsp::air::absorption_db_per_m(hz, 20.0, 0.7));
    const double d = tone_db(window(b, 0.4, 0.9), hz, 10.0) - tone_db(window(a, 0.4, 0.9), hz, 10.0);
    return verdict(std::abs(d - expected) <= 0.8, "30th harmonic (%.0f Hz) moved %+.2f dB (expected %+.2f +- 0.8)", hz, d, expected);
  });
  // Sound is faster in warm air: 10 m takes 30.75 ms at -10 C and 28.19 ms at 40 C.
  set(kTemperature, sound(D::kClaves, {{kMicDistance, 10.0}}), -10, 40, [](const Render& a, const Render& b) {
    const auto travel = [](double celsius) { return 10.0 / (331.3 * std::sqrt(1.0 + celsius / 273.15)); };
    const double expected = 1000.0 * (travel(-10.0) - travel(40.0));
    const double ms = 1000.0 * lag_s(b, a, 0.01);
    return verdict(std::abs(ms - expected) <= 0.1, "the warm air's stroke arrives %.2f ms sooner (expected %.2f +- 0.1)", ms, expected);
  });
  // A cardioid 5 cm from the vuvuzela against an omni there: the proximity effect lifts its
  // 233 Hz fundamental by |1 + c / (2 j w d)| (half the pattern is the pressure gradient).
  set(kMicPattern, horn({{kMicDistance, 0.05}}), 0, 1, [](const Render& a, const Render& b) {
    const double f0 = peak_hz(window(a, 0.4, 0.9), 233.0, 60.0);
    const double c = 331.3 * std::sqrt(1.0 + 20.0 / 273.15), x = 0.5 * c / (bonkr::dsp::kTau * f0 * 0.05);
    const double expected = 10.0 * std::log10(1.0 + x * x);
    const double d = tone_db(window(b, 0.4, 0.9), f0, 5.0) - tone_db(window(a, 0.4, 0.9), f0, 5.0);
    return verdict(std::abs(d - expected) <= 1.5, "fundamental (%.1f Hz) moved %+.1f dB (expected %+.1f +- 1.5)", f0, d, expected);
  });
  set(kMicSetup, horn({{kRoomSize, 3}, {kMicDistance, 2.0}}), 0, 1, [](const Render& a, const Render& b) {
    const double ca = measure::correlation(Span(a.left, at(0.3), at(0.6)), Span(a.right, at(0.3), at(0.6)));
    const double cb = measure::correlation(Span(b.left, at(0.3), at(0.6)), Span(b.right, at(0.3), at(0.6)));
    return verdict(ca <= 0.95 && cb >= 0.999, "L/R correlation %.3f (stereo pair) -> %.3f (mono) (expected under 0.95, then 1)", ca, cb);
  });
  // "Studio rock kit" to "Timpani": key 60 becomes the timpani tuned two semitones down (130.8 Hz).
  {
    double timpani = 0.0;
    for (size_t i = 0; i < bonkr::presets::count(); ++i) if (std::string(bonkr::presets::all()[i].name) == "Timpani") timpani = static_cast<double>(i);
    Context c = kit(D::kits::kStudioRock, {}, 60); c.hz = 130.8;
    set(kPreset, c, 0, timpani, [](const Render& a, const Render& b) {
      const double d = tone_db(window(b, 0.05, 0.6), 130.8) - tone_db(window(a, 0.05, 0.6), 130.8);
      return verdict(d >= 15.0, "130.8 Hz rose %+.1f dB (expected at least 15: the timpani on D3 less a tone)", d);
    });
  }
  return t;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: advertised_tests <BONKR.clap> [filter]\n"); return 2; }
  g_library = std::make_unique<Library>(argv[1]);
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  const char* filter = argc > 2 ? argv[2] : nullptr;
  const auto table = proofs();
  int missing = 0, failed = 0, passed = 0;
  for (uint32_t id = 0; id < P::kParamCount; ++id) {
    const Proof& proof = table[id];
    const char* name = P::kSpecs[id].name;
    if (!proof.check) { std::printf("MISSING  %s has no proof\n", name); ++missing; continue; }
    if (filter && !std::strstr(name, filter)) continue;
    g_note_hz = proof.context.hz > 0.0 ? proof.context.hz : bonkr::dsp::midi_to_hz(proof.context.key);
    const Render low = render(proof.context, id, proof.low), high = render(proof.context, id, proof.high);
    bool finite = true;
    for (const Render* r : {&low, &high}) for (float v : r->mono) finite = finite && std::isfinite(v);
    const Verdict v = finite ? proof.check(low, high) : Verdict{false, "output is not finite"};
    char lo[64], hi[64];
    P::format_value(id, proof.low, lo, sizeof(lo)); P::format_value(id, proof.high, hi, sizeof(hi));
    if (v.ok) ++passed; else ++failed;
    std::printf("%-8s %-16s %s -> %s: %s\n", v.ok ? "ok" : "FAILED", name, lo, hi, v.detail.c_str());
  }
  std::printf("%d proved, %d failed, %d missing of %u parameters\n", passed, failed, missing, static_cast<unsigned>(P::kParamCount));
  g_library.reset();
  return failed == 0 && missing == 0 ? 0 : 1;
}
