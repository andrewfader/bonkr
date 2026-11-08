// The instrument: a kit of physically modelled drums (or one sound across the keyboard), struck
// by the notes, through a drum machine's converter, the studio's compressor and gate, the room
// and the air, the master level and an output limiter. The engine knows nothing about CLAP: it
// takes a snapshot of parameter values per block and note events between render calls, and it
// never allocates while rendering.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>

#include "dsp/air.hpp"
#include "dsp/core.hpp"
#include "dsp/kits.hpp"
#include "dsp/machine.hpp"
#include "dsp/voice.hpp"
#include "params.hpp"

namespace bonkr {

// A block's worth of parameter values, copied out of the plug-in's atomics once per process
// call so everything in the block sees the same settings.
struct Settings {
  std::array<double, params::kParamCount> v{};
  double operator[](params::ParamId id) const { return v[static_cast<size_t>(id)]; }
  int option(params::ParamId id) const { return static_cast<int>(std::lround(v[static_cast<size_t>(id)])); }
  bool on(params::ParamId id) const { return v[static_cast<size_t>(id)] >= 0.5; }
  int kit() const { return std::clamp(option(params::kKit), 0, params::kKitCount - 1); }
  int sound() const { return std::clamp(option(params::kSound), 0, params::kSoundCount - 1); }
  bool chromatic() const { return option(params::kLayout) == params::kLayoutChromatic; }
  static Settings defaults() {
    Settings s;
    for (size_t i = 0; i < params::kParamCount; ++i) s.v[i] = params::kSpecs[i].def;
    return s;
  }
};

// Level trims that put every sound on the same footing: one stroke at velocity 0.8 with the
// default settings peaks at -9 dBFS (docs/QUALITY.md; held there by engine_tests). They are
// measured from the models themselves once per sample rate, at activation, so a change to a
// model's physics re-levels it.
namespace levels {
constexpr double kTargetPeak = 0.355;  // -9 dBFS
constexpr double kReferenceVelocity = 0.8;
// How long to listen to a sound for its peak: long for the ones that arrive late.
inline double window_s(int s) {
  switch (s) {
    case dsp::kJetFlyby: return 6.5;
    case dsp::kThunder: return 4.0;
    case dsp::kExplosion: case dsp::kSiren: return 3.0;
    case dsp::kHowitzer: case dsp::kFireworkBurst: case dsp::kRainStick: case dsp::kFireworkWhistle: return 2.0;
    case dsp::kCannon: case dsp::kFireworkCrackle: case dsp::kWashingMachine: case dsp::kSonicBoom: case dsp::kCymbalSwell: return 1.5;
    case dsp::kGuiroLong: case dsp::kVuvuzela: case dsp::kTelephone: case dsp::kMetalPipe: case dsp::kGroupClap: return 0.8;
    default: return 0.4;
  }
}
inline dsp::Play reference_play() {
  dsp::Play p;
  p.velocity = kReferenceVelocity;
  return p;
}
// One sound's peak at the reference stroke, dry, at this rate.
inline double measure_peak(int sound, double rate) {
  auto v = std::make_unique<dsp::Voice>();
  v->prepare(rate);
  v->start(sound, reference_play());
  const int n = static_cast<int>(window_s(sound) * rate);
  double peak = 0.0;
  for (int i = 0; i < n; ++i) peak = std::max(peak, static_cast<double>(std::abs(v->process())));
  return peak;
}
inline const std::array<float, dsp::kSoundCount>& trims(double rate) {
  static std::mutex lock;
  static std::map<long, std::unique_ptr<std::array<float, dsp::kSoundCount>>> cache;
  std::lock_guard<std::mutex> guard(lock);
  auto& slot = cache[std::lround(rate)];
  if (!slot) {
    slot = std::make_unique<std::array<float, dsp::kSoundCount>>();
    for (int s = 0; s < dsp::kSoundCount; ++s) {
      const double peak = measure_peak(s, rate);
      (*slot)[static_cast<size_t>(s)] = static_cast<float>(std::clamp(kTargetPeak / std::max(peak, 1e-9), 1e-4, 1e5));
    }
  }
  return *slot;
}
}  // namespace levels

// The output limiter: nothing leaves above the ceiling. Instant attack on one detector for both
// channels, recovery over about 50 ms: a peak catcher, not a compressor.
class Limiter {
 public:
  static constexpr float kCeiling = 0.794f;  // -2 dBFS
  static constexpr double kClipDb = 9.0;     // reduction beyond which the squash is plainly audible
  void prepare(double rate) { release_ = std::exp(-1.0 / (0.050 * rate)); reset(); }
  void reset() { gain_ = 1.0; held_db_ = 0.0; }
  float gain(float l, float r) {
    const double peak = std::max(std::abs(l), std::abs(r));
    const double wanted = peak > kCeiling ? kCeiling / peak : 1.0;
    if (wanted < gain_) gain_ = wanted;
    else gain_ += (wanted - gain_) * (1.0 - release_);
    if (gain_ < 1.0) held_db_ = std::max(held_db_, -20.0 * std::log10(gain_));
    return static_cast<float>(gain_);
  }
  double held_db() const { return held_db_; }
  void reset_meter() { held_db_ = 0.0; }
 private:
  double release_ = 0.9998, gain_ = 1.0, held_db_ = 0.0;
};

class Engine {
 public:
  static constexpr int kVoices = 32;
  static constexpr int kPendingMax = 64;

  void prepare(double rate) {
    rate_ = rate;
    trims_ = &levels::trims(rate);
    for (auto& v : voices_) v.prepare(rate);
    room_.prepare(rate);
    limiter_.prepare(rate);
    converter_.prepare(rate);
    compressor_.prepare(rate);
    gate_.prepare(rate);
    master_.set_time(10.0, rate);
    master_.snap(dsp::db_to_gain(params::kSpecs[params::kMasterDb].def));
    reset();
  }
  void reset() {
    for (auto& v : voices_) v.kill();
    room_.reset(); limiter_.reset(); converter_.reset(); compressor_.reset(); gate_.reset();
    pending_count_ = 0; held_.fill(false); repeat_left_.fill(-1.0); key_delay_.fill(0.0);
    peak_l_ = peak_r_ = 0.0f; clipped_ = false; frame_ = 0;
  }

  // --- the transport ---
  // Called at the start of each block: whether the host is playing, and the beat at the block's
  // first sample. Swing needs both; note repeat needs the tempo.
  void set_transport(bool playing, double beat) { playing_ = playing; beat_ = beat; frame_ = 0; }
  // Beats rendered since the block began (a transport event mid-block states the position at its
  // own sample; this turns it back into the block's).
  double beats_rendered() const { return static_cast<double>(frame_) / rate_ * tempo_ / 60.0; }
  void set_tempo(double bpm) { if (std::isfinite(bpm) && bpm > 0.0) tempo_ = bpm; }
  double tempo() const { return tempo_; }

  // --- events ---
  void note_on(const Settings& s, int16_t channel, int key, int32_t note_id, double velocity) {
    (void)channel; (void)note_id;
    if (key < 0 || key > 127) return;
    velocity = std::clamp(velocity, 0.0, 1.0);
    held_[static_cast<size_t>(key)] = true;
    held_velocity_[static_cast<size_t>(key)] = velocity;
    const double interval = repeat_interval(s);
    repeat_left_[static_cast<size_t>(key)] = interval > 0.0 ? interval : -1.0;
    const double delay = swing_delay(s);
    key_delay_[static_cast<size_t>(key)] = delay;
    if (delay >= 1.0) { queue(delay, key, velocity, false); return; }
    strike(s, key, velocity);
  }
  void note_off(const Settings& s, int16_t channel, int key, int32_t note_id) {
    (void)channel; (void)note_id; (void)s;
    if (key < 0) { all_notes_off(); return; }
    if (key > 127) return;
    held_[static_cast<size_t>(key)] = false;
    repeat_left_[static_cast<size_t>(key)] = -1.0;
    const double delay = key_delay_[static_cast<size_t>(key)];
    if (delay >= 1.0) { queue(delay, key, 0.0, true); return; }
    release(key);
  }
  void all_notes_off() {
    for (auto& v : voices_) v.release();
    held_.fill(false); repeat_left_.fill(-1.0); pending_count_ = 0;
  }
  void choke_key(int key) { for (auto& v : voices_) if (v.awake() && v.key == key) v.choke(); }
  void all_sound_off() { for (auto& v : voices_) v.kill(); pending_count_ = 0; held_.fill(false); repeat_left_.fill(-1.0); }

  // The sound, tuning, level, place and choke group a key strikes under these settings.
  struct Target { int sound = -1; double semitones = 0.0; double level_db = 0.0; double pan = 0.0; int choke = 0; };
  static Target target(const Settings& s, int key) {
    Target t;
    if (key < 0 || key > 127) return t;
    if (s.chromatic()) {
      t.sound = s.sound();
      t.semitones = (key - params::kChromaticRoot) + s[params::kTune];
      return t;
    }
    const dsp::kits::Slot& slot = dsp::kits::all()[static_cast<size_t>(s.kit())][static_cast<size_t>(key)];
    t.sound = slot.sound;
    t.semitones = slot.tune + s[params::kTune];
    t.level_db = slot.level;
    t.pan = slot.pan;
    t.choke = s.on(params::kChoke) ? slot.choke : 0;
    return t;
  }
  static dsp::Play play_for(const Settings& s, double velocity, double semitones, uint32_t seed) {
    dsp::Play p;
    p.velocity = velocity;
    p.dynamics = s[params::kDynamics];
    p.tune = std::exp2(semitones / 12.0);
    p.decay = s[params::kDecay];
    p.damping = s[params::kDamping];
    p.position = s[params::kPosition];
    p.striker = s.option(params::kStriker);
    p.hardness = std::pow(4.0, 1.0 - 2.0 * s[params::kHardness]);
    p.tension = s[params::kTension];
    p.snares = s[params::kSnares];
    p.snare_tension = s[params::kSnareTension];
    p.resonance = s[params::kResonance];
    p.celsius = s[params::kTemperature];
    p.humidity = s[params::kHumidity];
    p.seed = seed;
    return p;
  }

  // --- rendering ---
  void render(const Settings& s, float* left, float* right, uint32_t frames) {
    configure(s);
    master_.target(dsp::db_to_gain(s[params::kMasterDb]));
    const float width = static_cast<float>(s[params::kStereoWidth]);
    const float pan = static_cast<float>(s[params::kPan]);
    const float drive = static_cast<float>(s[params::kDrive]);
    const float drive_gain = 1.0f + 15.0f * drive * drive;
    for (uint32_t i = 0; i < frames; ++i) {
      if (pending_count_ > 0) run_pending(s);
      run_repeats(s);
      float l = 0.0f, r = 0.0f;
      for (auto& v : voices_) {
        if (!v.awake()) continue;
        const float y = v.process() * v.gain;
        const float place = std::clamp(v.pan * width + pan, -1.0f, 1.0f);
        const float angle = (place + 1.0f) * 0.25f * static_cast<float>(dsp::kPi);
        l += y * std::cos(angle) * 1.41421356f;
        r += y * std::sin(angle) * 1.41421356f;
      }
      if (drive > 0.0f) {
        l = std::tanh(l * drive_gain) / std::sqrt(drive_gain);
        r = std::tanh(r * drive_gain) / std::sqrt(drive_gain);
      }
      converter_.process(l, r);
      const float key = 0.5f * (l + r);
      compressor_.process(l, r);
      room_.process(l, r);
      gate_.process(key, l, r);
      const float gain = static_cast<float>(master_.next());
      l *= gain; r *= gain;
      if (!std::isfinite(l)) l = 0.0f;
      if (!std::isfinite(r)) r = 0.0f;
      const float limit = limiter_.gain(l, r);
      l *= limit; r *= limit;
      if (limiter_.held_db() > Limiter::kClipDb) clipped_ = true;
      peak_l_ = std::max(peak_l_, std::abs(l)); peak_r_ = std::max(peak_r_, std::abs(r));
      left[i] = l; right[i] = r;
      ++frame_;
    }
  }

  // How long after the last note the output can still be sounding, in samples.
  uint32_t tail_samples(const Settings& s) const {
    // A tam-tam rings for half a minute at 60 dB; 50 dB is down within about 18 s.
    double seconds = 18.0 * std::max(1.0, s[params::kDecay]);
    if (s.option(params::kRoomSize) >= 1) {
      dsp::air::Room probe;
      probe.prepare(1000.0);
      probe.configure(room_settings(s));
      seconds += probe.rt60_low() + 0.6;
    }
    seconds += s[params::kMicDistance] / 300.0;
    return static_cast<uint32_t>(std::min(seconds * rate_, 2.0e9));
  }

  void configure(const Settings& s) {
    int conv = s.option(params::kConverter);
    if (conv == dsp::kits::kConvKit) conv = s.chromatic() ? dsp::kits::kConvClean : dsp::kits::kKitConverter[static_cast<size_t>(s.kit())];
    converter_.set(conv);
    compressor_.set(s[params::kCrush]);
    gate_.set(s[params::kGate], s[params::kGateHold]);
    room_.configure(room_settings(s));
  }
  static dsp::air::Room::Settings room_settings(const Settings& s) {
    dsp::air::Room::Settings r;
    r.volume_m3 = params::kRoomVolumes[static_cast<size_t>(std::clamp(s.option(params::kRoomSize), 0, params::kRoomSizeCount - 1))];
    r.absorption = s[params::kRoomAbsorption];
    r.distance_m = s[params::kMicDistance];
    r.humidity = s[params::kHumidity];
    r.celsius = s[params::kTemperature];
    r.pattern = s.option(params::kMicPattern);
    r.mono = s[params::kMicSetup] >= 0.5;
    return r;
  }

  // --- what the editor and the tests look at ---
  int active_voices() const { int n = 0; for (const auto& v : voices_) n += v.awake(); return n; }
  bool silent() const { return active_voices() == 0 && pending_count_ == 0; }
  float peak_left() const { return peak_l_; }
  float peak_right() const { return peak_r_; }
  bool clipped() const { return clipped_; }
  double limited_db() const { return limiter_.held_db(); }
  void reset_meters() { peak_l_ = peak_r_ = 0.0f; clipped_ = false; limiter_.reset_meter(); }
  int sounding_keys(bool* out) const {
    bool keys[128] = {};
    for (const auto& v : voices_) if (v.awake() && v.key >= 0 && v.level() > 1e-4f) keys[v.key] = true;
    int n = 0;
    for (int k = 0; k < 128; ++k) { if (out) out[k] = keys[k]; n += keys[k]; }
    return n;
  }
  // The most recent stroke, for the editor.
  struct Last { int key = -1, sound = -1; double speed = 0.0, contact_ms = 0.0, fundamental_hz = 0.0, semitones = 0.0; };
  const Last& last() const { return last_; }
  const dsp::Voice* voice_for_key(int key) const {
    const dsp::Voice* best = nullptr;
    for (const auto& v : voices_) if (v.awake() && v.key == key && (!best || v.age() < best->age())) best = &v;
    return best;
  }
  const dsp::machine::Converter& converter() const { return converter_; }
  double gate_gain() const { return gate_.gain(); }
  double compressor_reduction_db() const { return compressor_.reduction_db(); }
  float trim(int sound) const { return (*trims_)[static_cast<size_t>(std::clamp(sound, 0, dsp::kSoundCount - 1))]; }

 private:
  struct Pending { double left = 0.0; int key = 0; double velocity = 0.0; bool off = false; };

  void strike(const Settings& s, int key, double velocity) {
    const Target t = target(s, key);
    if (t.sound < 0) return;
    const uint32_t seed = static_cast<uint32_t>(++counter_ * 2654435761u) ^ 0x9e3779b9u;
    const dsp::Play p = play_for(s, velocity, t.semitones, seed);
    if (t.choke != 0)
      for (auto& v : voices_) if (v.awake() && v.choke_group == t.choke && v.key != key) v.choke();
    dsp::Voice* voice = nullptr;
    if (dsp::Voice::restrikes(t.sound))
      for (auto& v : voices_) if (v.awake() && v.key == key && v.sound() == t.sound) { voice = &v; break; }
    if (voice) {
      voice->restrike(p);
    } else {
      voice = allocate();
      voice->start(t.sound, p);
      voice->key = key;
    }
    voice->choke_group = t.choke;
    voice->pan = static_cast<float>(t.pan);
    voice->gain = trim(t.sound) * static_cast<float>(dsp::db_to_gain(t.level_db));
    last_.key = key; last_.sound = t.sound; last_.semitones = t.semitones;
    last_.speed = dsp::stroke_speed(velocity, p.dynamics);
    last_.contact_ms = dsp::modal::Contact::contact_ms(dsp::striker_of(p.striker == dsp::kAsPlayed ? dsp::kStick : p.striker), last_.speed) * p.hardness;
    last_.fundamental_hz = voice->sounding_hz();
  }
  void release(int key) { for (auto& v : voices_) if (v.awake() && v.key == key) v.release(); }

  dsp::Voice* allocate() {
    for (auto& v : voices_) if (!v.awake()) return &v;
    // Steal the quietest of the oldest half.
    dsp::Voice* best = &voices_[0];
    for (auto& v : voices_) if (!v.held() && v.level() < best->level()) best = &v;
    return best;
  }

  // MPC swing: a note on the second sixteenth of each eighth moves later, to `swing` of the way
  // through the eighth (50% straight, 66% a triplet feel, 75% dotted).
  double swing_delay(const Settings& s) const {
    const double swing = s[params::kSwing];
    if (!playing_ || swing <= 0.5005) return 0.0;
    const double beat = beat_ + static_cast<double>(frame_) / rate_ * tempo_ / 60.0;
    const double sixteenth = beat * 4.0;
    const double nearest = std::round(sixteenth);
    if (std::abs(sixteenth - nearest) > 0.25) return 0.0;
    const long index = static_cast<long>(nearest);
    if ((index & 1) == 0) return 0.0;
    const double eighth_s = 60.0 / tempo_ * 0.5;
    return (swing - 0.5) * eighth_s * rate_;
  }
  double repeat_interval(const Settings& s) const {
    const int r = std::clamp(s.option(params::kNoteRepeat), 0, params::kRepeatCount - 1);
    if (r == 0) return 0.0;
    return params::kRepeatBeats[static_cast<size_t>(r)] * 60.0 / tempo_ * rate_;
  }
  void queue(double delay, int key, double velocity, bool off) {
    if (pending_count_ >= kPendingMax) { if (off) release(key); return; }
    pending_[static_cast<size_t>(pending_count_++)] = Pending{delay, key, velocity, off};
  }
  void run_pending(const Settings& s) {
    for (int i = 0; i < pending_count_;) {
      Pending& p = pending_[static_cast<size_t>(i)];
      if (--p.left > 0.0) { ++i; continue; }
      const Pending fire = p;
      pending_[static_cast<size_t>(i)] = pending_[static_cast<size_t>(--pending_count_)];
      if (fire.off) release(fire.key); else strike(s, fire.key, fire.velocity);
    }
  }
  void run_repeats(const Settings& s) {
    for (int k = 0; k < 128; ++k) {
      double& left = repeat_left_[static_cast<size_t>(k)];
      if (left < 0.0) continue;
      if (--left > 0.0) continue;
      const double interval = repeat_interval(s);
      if (interval <= 0.0 || !held_[static_cast<size_t>(k)]) { left = -1.0; continue; }
      left = interval;
      strike(s, k, held_velocity_[static_cast<size_t>(k)]);
    }
  }

  double rate_ = 48000.0, tempo_ = 120.0, beat_ = 0.0;
  bool playing_ = false;
  uint64_t frame_ = 0;
  const std::array<float, dsp::kSoundCount>* trims_ = nullptr;
  std::array<dsp::Voice, kVoices> voices_{};
  dsp::machine::Converter converter_;
  dsp::machine::Compressor compressor_;
  dsp::machine::Gate gate_;
  dsp::air::Room room_;
  Limiter limiter_;
  dsp::Smoother master_;
  std::array<Pending, kPendingMax> pending_{};
  int pending_count_ = 0;
  std::array<bool, 128> held_{};
  std::array<double, 128> held_velocity_{}, repeat_left_{}, key_delay_{};
  uint64_t counter_ = 0;
  Last last_{};
  float peak_l_ = 0.0f, peak_r_ = 0.0f;
  bool clipped_ = false;
};

}  // namespace bonkr
