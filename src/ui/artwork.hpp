// The per-page artwork above the cards: the keyboard with the kit's keys and the last stroke, and
// the output scope; the striker's contact force; the struck body's modes; the converter's steps
// and the swing; the gate on the room; the room. Drawn from
// parameter values plus a LiveState snapshot, so the plug-in and the offscreen preview tool
// render exactly the same pictures.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "../dsp/core.hpp"
#include "../dsp/kits.hpp"
#include "../dsp/machine.hpp"
#include "../dsp/voice.hpp"
#include "../dsp/air.hpp"
#include "../params.hpp"
#include "editor.hpp"
#include "layout.hpp"

namespace bonkr::ui {

// How much of the output the plug-in keeps for the scope: every sample, so the scope shows the
// real waveform rather than an aliased decimation of it.
constexpr size_t kScopeSamples = 4096;

// What the artwork needs from the running instrument.
struct LiveState {
  std::array<float, kScopeSamples> scope{};  // recent output samples, oldest first
  double sample_rate = 48000.0;
  float energy = 0.0f;
  std::array<bool, 128> sounding{};
  int voices = 0;
  // The most recent stroke: its key and sound, the stick's speed, its contact time and the
  // struck body's fundamental; the gate's gain and the compressor's reduction now.
  int key = -1, sound = -1;
  double speed = 0.0, contact_ms = 0.0, hz = 0.0, gate = 1.0, reduction_db = 0.0;
};

// The scope's picture of the output: 20 ms starting at the latest rising zero crossing, so a
// steady note holds still, amplified so its peak fills most of the frame (up to +24 dB).
struct ScopeTrace {
  std::vector<float> points;
  double gain = 1.0;
};

inline ScopeTrace scope_trace(const std::array<float, kScopeSamples>& s, double sample_rate, int count) {
  ScopeTrace out;
  float peak = 0.0f;
  for (float x : s) peak = std::max(peak, std::abs(x));
  out.gain = peak > 1e-4f ? std::clamp(0.9 / peak, 1.0, 16.0) : 1.0;
  const size_t n = s.size();
  const size_t window = std::clamp<size_t>(static_cast<size_t>(sample_rate * 0.02), 64, n / 2);
  size_t start = n - window;
  for (size_t i = n - window; i > n - window - n / 2 && i > 0; --i)
    if (s[i - 1] < 0.0f && s[i] >= 0.0f) { start = i; break; }
  out.points.resize(static_cast<size_t>(std::max(2, count)));
  for (size_t k = 0; k < out.points.size(); ++k) {
    const double pos = static_cast<double>(start) + static_cast<double>(k) * static_cast<double>(window - 1) / static_cast<double>(out.points.size() - 1);
    const size_t i = std::min(static_cast<size_t>(pos), n - 2);
    const double frac = pos - static_cast<double>(i);
    out.points[k] = static_cast<float>(std::clamp((s[i] + (s[i + 1] - s[i]) * frac) * out.gain, -1.0, 1.0));
  }
  return out;
}

class Artwork {
 public:
  Artwork(const EditorHost& host, const LiveState& live) : host_(host), live_(live) {}

  void draw(int page, Painter& g, const Rect& r) const {
    switch (page) {
      case 0: draw_keyboard(g, r); break;
      case 1: draw_contact(g, r); break;
      case 2: draw_modes(g, r); break;
      case 3: draw_machine(g, r); break;
      case 4: draw_gate(g, r); break;
      case 5: draw_room(g, r); break;
      default: break;
    }
  }

  // The sound the body pictures describe: the last one struck, or the chosen sound, or the
  // kit's snare.
  int shown_sound() const {
    if (live_.sound >= 0) return live_.sound;
    if (static_cast<int>(std::lround(value(params::kLayout))) == params::kLayoutChromatic) return std::clamp(static_cast<int>(std::lround(value(params::kSound))), 0, dsp::kSoundCount - 1);
    const auto& map = dsp::kits::all()[static_cast<size_t>(std::clamp(static_cast<int>(std::lround(value(params::kKit))), 0, dsp::kits::kKitCount - 1))];
    return map[38].sound >= 0 ? map[38].sound : dsp::kSnareMaple;
  }

 private:
  double value(params::ParamId id) const { return host_.value(static_cast<int>(id)); }
  bool chromatic() const { return static_cast<int>(std::lround(value(params::kLayout))) == params::kLayoutChromatic; }
  int kit() const { return std::clamp(static_cast<int>(std::lround(value(params::kKit))), 0, dsp::kits::kKitCount - 1); }
  static const char* key_name(int key) {
    static const char* const kKeys[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"};
    return kKeys[((key % 12) + 12) % 12];
  }
  bool key_used(int k) const {
    if (chromatic()) return true;
    return dsp::kits::all()[static_cast<size_t>(kit())][static_cast<size_t>(k)].sound >= 0;
  }

  // The keyboard across the strip, the kit's keys in ivory and ebony and the empty ones dark,
  // keys lit while they sound; the last stroke; the output scope below.
  void draw_keyboard(Painter& g, const Rect& r) const {
    using namespace theme;
    char name[96]{};
    if (chromatic()) params::format_value(params::kSound, value(params::kSound), name, sizeof(name));
    else params::format_value(params::kKit, value(params::kKit), name, sizeof(name));
    g.text(r.x + 20, r.y + 24, name, amber, 340, TextSize::Normal);
    char line[200]{};
    if (live_.key >= 0 && live_.sound >= 0) {
      if (live_.hz > 0.0)
        std::snprintf(line, sizeof(line), "%s%d  •  %s  •  %.1f m/s  •  contact %.2f ms  •  %.0f Hz", key_name(live_.key), live_.key / 12 - 1, dsp::kSoundNames[live_.sound], live_.speed, live_.contact_ms, live_.hz);
      else
        std::snprintf(line, sizeof(line), "%s%d  •  %s  •  %.1f m/s", key_name(live_.key), live_.key / 12 - 1, dsp::kSoundNames[live_.sound], live_.speed);
    } else {
      std::snprintf(line, sizeof(line), chromatic() ? "C4 plays it at its own pitch" : "General MIDI drum map: kick on C1, snare on D1, hats on F#1, G#1 and A#1");
    }
    g.text(r.x + 380, r.y + 24, line, soft, r.w - 640, TextSize::Small);
    const int first = 21, last = 108;
    int whites = 0;
    for (int k = first; k <= last; ++k) whites += !is_black(k);
    const int kx = r.x + 20, kw = r.w - 40, key_w = kw / whites, ky = r.y + 36, key_h = 72;
    int wi = 0;
    for (int k = first; k <= last; ++k) {
      if (is_black(k)) continue;
      const bool in = key_used(k), on = live_.sounding[static_cast<size_t>(k)];
      g.rect(kx + wi * key_w + 1, ky, key_w - 2, key_h, on ? amber : (in ? text : faint));
      ++wi;
    }
    wi = 0;
    for (int k = first; k <= last; ++k) {
      if (!is_black(k)) { ++wi; continue; }
      const bool in = key_used(k), on = live_.sounding[static_cast<size_t>(k)];
      g.rect(kx + wi * key_w - key_w / 3, ky, 2 * key_w / 3, key_h * 6 / 10, on ? orange : (in ? bg : border));
    }
    const int top = ky + key_h + 10, mid = top + (r.bottom() - top) / 2, amp = (r.bottom() - top) / 2 - 6;
    if (amp > 6) {
      const ScopeTrace trace = scope_trace(live_.scope, live_.sample_rate, 384);
      std::vector<Point> scope;
      const int last_point = static_cast<int>(trace.points.size()) - 1;
      for (int i = 0; i <= last_point; ++i)
        scope.push_back({kx + i * kw / last_point, mid - static_cast<int>(trace.points[static_cast<size_t>(i)] * static_cast<float>(amp))});
      g.polyline(scope, soft, 2);
      char gain[48]{};
      std::snprintf(gain, sizeof(gain), "scope \xc3\x97%.1f  (+%.0f dB)", trace.gain, 20.0 * std::log10(trace.gain));
      g.text_right(r.right() - 20, r.y + 24, gain, muted, 220, TextSize::Small);
    }
  }
  static bool is_black(int key) { const int pc = key % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }

  // The striker's force against time at three stroke speeds (Hertzian contact: a harder stroke
  // is shorter and taller), from the same formulas the voices use.
  void draw_contact(Painter& g, const Rect& r) const {
    using namespace theme;
    const int kind = std::clamp(static_cast<int>(std::lround(value(params::kStriker))), 0, dsp::kStrikerCount - 1);
    const dsp::modal::Striker s = dsp::striker_of(kind == dsp::kAsPlayed ? dsp::kStick : kind);
    const double hardness = std::pow(4.0, 1.0 - 2.0 * value(params::kHardness));
    const double dynamics = value(params::kDynamics);
    g.text(r.x + 20, r.y + 24, "THE CONTACT  •  force against time, pp mf ff", muted, 520, TextSize::Small);
    const double vel[3] = {0.25, 0.6, 0.95};
    const uint32_t colours[3] = {faint, soft, orange};
    const int px = r.x + 40, pw = r.w - 380, py = r.bottom() - 18, ph = r.h - 64;
    const double span_ms = std::max(2.0, 2.2 * dsp::modal::Contact::contact_ms(s, dsp::stroke_speed(0.25, dynamics)) * hardness);
    double peak = 0.0;
    for (double v : vel) {
      const double speed = dsp::stroke_speed(v, dynamics);
      const double ms = dsp::modal::Contact::contact_ms(s, speed) * hardness;
      peak = std::max(peak, speed / ms);
    }
    g.line(px, py, px + pw, py, border);
    for (int i = 0; i < 3; ++i) {
      const double speed = dsp::stroke_speed(vel[i], dynamics);
      const double ms = dsp::modal::Contact::contact_ms(s, speed) * hardness;
      const double height = speed / ms / peak;
      std::vector<Point> pts;
      for (int k = 0; k <= 80; ++k) {
        const double t = span_ms * k / 80.0;
        const double f = t < ms ? std::sin(dsp::kPi * t / ms) * height : 0.0;
        pts.push_back({px + static_cast<int>(pw * t / span_ms), py - static_cast<int>(f * ph)});
      }
      g.polyline(pts, colours[i], 2);
    }
    char line[200]{};
    std::snprintf(line, sizeof(line), "%s  •  Hertz exponent %.1f  •  %.2f ms at 3 m/s  •  %.1f to %.1f m/s", params::kStrikerNames[kind], s.alpha,
                  s.contact_ms * hardness, dsp::stroke_speed(0.0, dynamics), dsp::stroke_speed(1.0, dynamics));
    g.text_right(r.right() - 20, r.y + 24, line, soft, r.w - 560, TextSize::Small);
    char span[48]{};
    std::snprintf(span, sizeof(span), "%.1f ms", span_ms);
    g.text_right(px + pw, py - 6, span, muted, 120, TextSize::Small);
  }

  // The struck body's modes on a log frequency axis: height for how strongly the stroke rings
  // each, brightness for how long it lasts.
  void draw_modes(Painter& g, const Rect& r) const {
    using namespace theme;
    const int sound = shown_sound();
    auto voice = std::make_unique<dsp::Voice>();
    voice->prepare(48000.0);
    dsp::Play p;
    p.decay = value(params::kDecay); p.damping = value(params::kDamping); p.position = value(params::kPosition);
    p.tension = value(params::kTension); p.resonance = value(params::kResonance);
    p.tune = std::exp2(value(params::kTune) / 12.0);
    voice->start(sound, p);
    const auto& bank = voice->bank(0);
    char title[160]{};
    std::snprintf(title, sizeof(title), "THE BODY  •  %s  •  %d modes", dsp::kSoundNames[sound], bank.count());
    g.text(r.x + 20, r.y + 24, title, muted, r.w - 40, TextSize::Small);
    const int px = r.x + 40, pw = r.w - 80, py = r.bottom() - 26, ph = r.h - 70;
    const double lo = std::log(20.0), hi = std::log(20000.0);
    g.line(px, py, px + pw, py, border);
    for (double f : {50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0}) {
      const int x = px + static_cast<int>(pw * (std::log(f) - lo) / (hi - lo));
      g.line(x, py, x, py + 5, faint);
      char label[16]{};
      std::snprintf(label, sizeof(label), f >= 1000.0 ? "%.0fk" : "%.0f", f >= 1000.0 ? f / 1000.0 : f);
      g.text(x - 10, py + 20, label, faint, 40, TextSize::Small);
    }
    if (bank.count() == 0) {
      g.text(px + 20, r.y + r.h / 2, "not a ringing body: noise, particles, a circuit or a shock wave", soft, pw - 40, TextSize::Small);
      return;
    }
    for (int i = 0; i < bank.count(); ++i) {
      const double hz = bank.hz(i);
      const int x = px + static_cast<int>(pw * (std::log(std::clamp(hz, 20.0, 20000.0)) - lo) / (hi - lo));
      const double t60 = bank.t60(i);
      const double h = std::clamp(0.25 + 0.25 * std::log10(1.0 + 10.0 * t60), 0.08, 1.0);
      g.line(x, py, x, py - static_cast<int>(h * ph), i == 0 ? green : mix(faint, green, std::clamp(t60 / 2.0, 0.0, 1.0)), 2);
    }
    char line[96]{};
    std::snprintf(line, sizeof(line), "lowest %.1f Hz, rings %.2f s", bank.hz(0), bank.t60(0));
    g.text_right(r.right() - 20, r.y + 24, line, soft, 320, TextSize::Small);
  }

  // The converter: a decaying tone held and quantised at the machine's clock, and the swing
  // grid: sixteen steps, every second one late by the swing.
  void draw_machine(Painter& g, const Rect& r) const {
    using namespace theme;
    int which = static_cast<int>(std::lround(value(params::kConverter)));
    if (which == dsp::kits::kConvKit) which = chromatic() ? dsp::kits::kConvClean : dsp::kits::kKitConverter[static_cast<size_t>(kit())];
    dsp::machine::Converter conv;
    conv.prepare(48000.0);
    conv.set(which);
    const auto& spec = dsp::machine::kConverters[static_cast<size_t>(which)];
    char title[160]{};
    if (spec.clock_hz > 0.0) std::snprintf(title, sizeof(title), "THE CONVERTER  •  %s  •  %.2f kHz, %d bits%s", spec.name, spec.clock_hz / 1000.0, spec.bits, spec.companded ? ", companded" : "");
    else std::snprintf(title, sizeof(title), "THE CONVERTER  •  none: the sound as struck");
    g.text(r.x + 20, r.y + 24, title, muted, r.w / 2, TextSize::Small);
    const int px = r.x + 30, pw = r.w / 2 - 50, mid = r.y + r.h / 2 + 12, amp = r.h / 2 - 34;
    std::vector<Point> in, out;
    for (int i = 0; i < 240; ++i) {
      // A quiet decaying tone, so the steps show.
      const float x = static_cast<float>(0.12 * std::exp(-i / 160.0) * std::sin(i * 0.11));
      float l = x, rr = x;
      conv.process(l, rr);
      in.push_back({px + i * pw / 240, mid - static_cast<int>(x / 0.12f * static_cast<float>(amp))});
      out.push_back({px + i * pw / 240, mid - static_cast<int>(l / 0.12f * static_cast<float>(amp))});
    }
    g.polyline(in, faint, 1);
    g.polyline(out, red, 2);
    // The swing grid.
    const double swing = value(params::kSwing);
    const int gx = r.x + r.w / 2 + 20, gw = r.w / 2 - 50, gy = r.y + r.h / 2 - 10;
    char groove[120]{};
    const int repeat = std::clamp(static_cast<int>(std::lround(value(params::kNoteRepeat))), 0, params::kRepeatCount - 1);
    std::snprintf(groove, sizeof(groove), "SWING %.0f%%  •  NOTE REPEAT %s", swing * 100.0, params::kRepeatNames[repeat]);
    g.text(gx, r.y + 24, groove, muted, gw, TextSize::Small);
    g.line(gx, gy + 30, gx + gw, gy + 30, border);
    for (int step = 0; step < 16; ++step) {
      const double at = (step + ((step & 1) ? (swing - 0.5) * 2.0 : 0.0)) / 16.0;
      const int x = gx + static_cast<int>(gw * at);
      g.rect(x - 4, gy + ((step & 3) == 0 ? 0 : 12), 8, (step & 3) == 0 ? 30 : 18, (step & 1) ? red : soft);
    }
  }

  // The gate on the room: a stroke's level against time, the room's tail after it, and what the
  // compressor and the gate leave.
  void draw_gate(Painter& g, const Rect& r) const {
    using namespace theme;
    const double gate = value(params::kGate), hold = value(params::kGateHold), crush = value(params::kCrush);
    const int size = std::clamp(static_cast<int>(std::lround(value(params::kRoomSize))), 0, params::kRoomSizeCount - 1);
    dsp::air::Room room;
    room.prepare(1000.0);
    dsp::air::Room::Settings rs;
    rs.volume_m3 = params::kRoomVolumes[static_cast<size_t>(size)]; rs.absorption = value(params::kRoomAbsorption); rs.distance_m = std::max(1.0, value(params::kMicDistance));
    room.configure(rs);
    const double rt60 = size > 0 ? room.rt60_low() : 0.25;
    char title[200]{};
    std::snprintf(title, sizeof(title), "THE GATE  •  room tail %.2f s  •  %s  •  listen-mic %.0f%%", rt60, gate > 0.0 ? "gate on" : "gate off", crush * 100.0);
    g.text(r.x + 20, r.y + 24, title, muted, r.w - 340, TextSize::Small);
    if (gate > 0.0) {
      char right[64]{};
      std::snprintf(right, sizeof(right), "holds %.0f ms", hold);
      g.text_right(r.right() - 20, r.y + 24, right, soft, 300, TextSize::Small);
    }
    const int px = r.x + 40, pw = r.w - 80, py = r.bottom() - 20, ph = r.h - 64;
    const double span = std::max(1.2 * hold / 1000.0, std::min(2.0, 1.2 * rt60 + 0.2));
    g.line(px, py, px + pw, py, border);
    std::vector<Point> raw, shaped;
    const double threshold_db = -50.0 + 40.0 * gate;
    for (int k = 0; k <= 200; ++k) {
      const double t = span * k / 200.0;
      // The level in dB: the stroke's own decay, then the room's.
      const double direct_db = -60.0 * t / 0.3;
      const double room_db = -12.0 - 60.0 * t / std::max(rt60, 0.05);
      const double level_db = std::max(direct_db, room_db);
      double out_db = level_db;
      if (crush > 0.0) { const double thr = -40.0 * crush; if (out_db > thr) out_db = thr + (out_db - thr) / 20.0; out_db += 0.5 * (-(thr + (0.0 - thr) / 20.0)); }
      const bool open = gate <= 0.0 || t * 1000.0 < hold || direct_db > threshold_db;
      if (!open) out_db = -80.0;
      const auto y = [&](double db) { return py - static_cast<int>(ph * std::clamp((db + 60.0) / 60.0, 0.0, 1.0)); };
      raw.push_back({px + pw * k / 200, y(level_db)});
      shaped.push_back({px + pw * k / 200, y(out_db)});
    }
    g.polyline(raw, faint, 1);
    g.polyline(shaped, violet, 2);
  }

  // The room from above: its box, the instrument, the microphones at their distance.
  void draw_room(Painter& g, const Rect& r) const {
    using namespace theme;
    const int size = std::clamp(static_cast<int>(value(params::kRoomSize)), 0, params::kRoomSizeCount - 1);
    const double volume = params::kRoomVolumes[static_cast<size_t>(size)];
    g.text(r.x + 20, r.y + 24, "THE ROOM FROM ABOVE", muted, 300, TextSize::Small);
    const double a = volume > 0.0 ? std::cbrt(volume / (1.0 * 1.4 * 1.9)) : 0.0;
    const double length = 1.9 * a, width = 1.4 * a;
    const double scale = volume > 0.0 ? std::min((r.w - 60) / length, (r.h - 60) / width) : 0.0;
    const int bx = r.x + 30, by = r.y + 40;
    const int bw = volume > 0.0 ? static_cast<int>(length * scale) : r.w - 60, bh = volume > 0.0 ? static_cast<int>(width * scale) : r.h - 60;
    g.round_rect(Rect{bx, by, bw, bh}, 6, volume > 0.0 ? panel_hi : panel);
    // The instrument a quarter of the way along; the microphones down the room.
    const int ix = bx + bw / 4, iy = by + bh * 43 / 100;
    g.circle(ix - 8, iy - 8, 16, amber);
    const double distance = value(params::kMicDistance);
    const double reach = volume > 0.0 ? std::min(distance, 0.9 * (length - length / 4) - 0.3) : distance;
    const int mx = ix + static_cast<int>((volume > 0.0 ? reach * scale : std::min(reach / 12.0, 1.0) * (bw * 0.7)));
    const bool mono = value(params::kMicSetup) >= 0.5;
    if (mono) g.circle(mx - 6, iy - 6, 12, cyan);
    else { g.circle(mx - 6, iy - 16, 12, cyan); g.circle(mx - 6, iy + 4, 12, cyan); }
    g.line(ix, iy, mx, iy, border);
    char line[160]{};
    std::snprintf(line, sizeof(line), "%s  •  %.0f m\xc2\xb3  •  microphones %.2f m away  •  %.0f%% humidity, %.0f C", params::kRoomSizeNames[size], volume, distance,
                  value(params::kHumidity) * 100.0, value(params::kTemperature));
    g.text(r.x + 330, r.y + 24, line, soft, r.w - 360, TextSize::Small);
  }

  const EditorHost& host_;
  const LiveState& live_;
};

}  // namespace bonkr::ui
