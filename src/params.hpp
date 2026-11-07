// The parameter table: every automatable control of BONKR, with its range, default, unit and
// value names. Shared by the plug-in, the editor and the tests, so the editor's page layout and
// the per-parameter proofs are checked against the one real list.
//
// Parameter IDs are the host's automation handles and index the saved state, so a parameter is
// only ever appended just before kParamCount, never reordered or removed.
#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

#include "dsp/catalog.hpp"
#include "dsp/kits.hpp"
#include "dsp/machine.hpp"
#include "dsp/voice.hpp"

namespace bonkr::params {

enum ParamId : uint32_t {
  // --- the kit -------------------------------------------------------------------------------------
  kLayout, kKit, kSound, kMasterDb, kTune,
  // --- the stroke ----------------------------------------------------------------------------------
  kStriker, kHardness, kPosition, kDynamics,
  // --- the drum ------------------------------------------------------------------------------------
  kDecay, kDamping, kTension, kSnares, kSnareTension, kResonance,
  // --- the machine ---------------------------------------------------------------------------------
  kConverter, kSwing, kNoteRepeat, kDrive, kChoke,
  // --- the studio ----------------------------------------------------------------------------------
  kCrush, kGate, kGateHold,
  // --- the room and the stereo picture -------------------------------------------------------------
  kRoomSize, kRoomAbsorption, kMicDistance, kHumidity, kTemperature, kMicPattern, kMicSetup, kStereoWidth, kPan,
  // --- presets -------------------------------------------------------------------------------------
  kPreset,
  kParamCount
};

// How a value is shown to the musician. Stepped parameters with names draw from an option table.
enum class Unit : uint8_t { Plain, Ms, Seconds, Percent, Db, Hz, Semitones, Cents, Count, Options, Metres, Celsius, Ratio };

struct ParamSpec {
  const char* name;
  const char* module;
  double min, max, def;
  bool stepped;
  Unit unit;
  const char* const* options;  // Unit::Options: one name per integer value from min to max
};

// --- option tables -------------------------------------------------------------------------------

constexpr int kSoundCount = dsp::kSoundCount;
constexpr int kKitCount = dsp::kits::kKitCount;
constexpr const char* kLayoutNames[] = {"Kit (GM map)", "One sound, chromatic"};
enum Layout : int { kLayoutKit, kLayoutChromatic };
// In chromatic play this key sounds the sound at its own pitch.
constexpr int kChromaticRoot = 60;
constexpr const char* kStrikerNames[dsp::kStrikerCount] = {"As played", "Stick", "Felt mallet", "Hand", "Brush", "Rods", "Hard mallet"};
constexpr int kConverterCount = dsp::kits::kConverterCount;
constexpr int kRepeatCount = 7;
constexpr const char* kRepeatNames[kRepeatCount] = {"Off", "1/4", "1/8", "1/8 triplet", "1/16", "1/16 triplet", "1/32"};
// Each repeat division's length in beats.
constexpr std::array<double, kRepeatCount> kRepeatBeats{0.0, 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125};
// The room (src/dsp/air.hpp): its volume, in cubic metres, for each named size.
constexpr int kRoomSizeCount = 7;
constexpr const char* kRoomSizeNames[kRoomSizeCount] = {"Off", "Booth", "Bedroom", "Studio", "Club", "Hall", "Church"};
constexpr std::array<double, kRoomSizeCount> kRoomVolumes{0.0, 10.0, 35.0, 150.0, 600.0, 3000.0, 12000.0};
constexpr const char* kMicPatternNames[] = {"Omni", "Cardioid", "Figure-8"};
constexpr const char* kMicSetupNames[] = {"Stereo pair", "Mono"};
constexpr int kPresetCount = 47;  // The names live in presets.hpp; the table only needs the size.

constexpr std::array<ParamSpec, kParamCount> kSpecs{{
    // the kit
    {"Layout", "Kit", 0, 1, 0, true, Unit::Options, kLayoutNames},
    {"Kit", "Kit", 0, kKitCount - 1, 0, true, Unit::Options, dsp::kits::kKitNames},
    {"Sound", "Kit", 0, kSoundCount - 1, dsp::kSnareMaple, true, Unit::Options, dsp::kSoundNames},
    {"Master", "Kit", -36, 24, -3, false, Unit::Db, nullptr},
    {"Tune", "Kit", -24, 24, 0, false, Unit::Semitones, nullptr},
    // the stroke
    {"Striker", "Stroke", 0, dsp::kStrikerCount - 1, 0, true, Unit::Options, kStrikerNames},
    {"Hardness", "Stroke", 0, 1, 0.5, false, Unit::Percent, nullptr},
    {"Position", "Stroke", -1, 1, 0, false, Unit::Plain, nullptr},
    {"Dynamics", "Stroke", 0, 1, 1, false, Unit::Percent, nullptr},
    // the drum
    {"Decay", "Drum", 0.25, 4, 1, false, Unit::Ratio, nullptr},
    {"Damping", "Drum", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Tension glide", "Drum", 0, 3, 1, false, Unit::Ratio, nullptr},
    {"Snares", "Drum", 0, 1, 1, false, Unit::Percent, nullptr},
    {"Snare tension", "Drum", 0, 1, 0.5, false, Unit::Percent, nullptr},
    {"Resonance", "Drum", 0, 1, 0.5, false, Unit::Percent, nullptr},
    // the machine
    {"Converter", "Machine", 0, kConverterCount - 1, 0, true, Unit::Options, dsp::machine::kConverterNames},
    {"Swing", "Machine", 0.5, 0.75, 0.5, false, Unit::Percent, nullptr},
    {"Note repeat", "Machine", 0, kRepeatCount - 1, 0, true, Unit::Options, kRepeatNames},
    {"Drive", "Machine", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Hi-hat choke", "Machine", 0, 1, 1, true, Unit::Plain, nullptr},
    // the studio
    {"Crush", "Studio", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Gate", "Studio", 0, 1, 0, false, Unit::Percent, nullptr},
    {"Gate hold", "Studio", 30, 1000, 280, false, Unit::Ms, nullptr},
    // the room and the air
    {"Room", "Room", 0, kRoomSizeCount - 1, 0, true, Unit::Options, kRoomSizeNames},
    {"Absorption", "Room", 0.02, 0.9, 0.25, false, Unit::Percent, nullptr},
    {"Mic distance", "Room", 0, 12, 0, false, Unit::Metres, nullptr},
    {"Humidity", "Room/Air", 0.05, 0.95, 0.45, false, Unit::Percent, nullptr},
    {"Temperature", "Room/Air", -10, 40, 20, false, Unit::Celsius, nullptr},
    {"Mic pattern", "Room", 0, 2, 0, true, Unit::Options, kMicPatternNames},
    {"Mic setup", "Room", 0, 1, 0, true, Unit::Options, kMicSetupNames},
    {"Stereo width", "Room/Stereo", 0, 1, 0.7, false, Unit::Percent, nullptr},
    {"Pan", "Room/Stereo", -1, 1, 0, false, Unit::Plain, nullptr},
    // presets
    {"Preset", "Presets", 0, kPresetCount - 1, 0, true, Unit::Plain, nullptr},
}};

// The preset parameter's value names come from the preset table, which lives above this header
// in the dependency order; the plug-in installs them here once.
inline const char* const* g_preset_names = nullptr;

inline const ParamSpec& spec(uint32_t id) { return kSpecs[static_cast<size_t>(id)]; }

inline double clamp_to_spec(uint32_t id, double value) {
  const ParamSpec& s = spec(id);
  if (!std::isfinite(value)) return s.def;
  value = std::clamp(value, s.min, s.max);
  return s.stepped ? std::round(value) : value;
}

// The option name for a stepped parameter's value, or null if it has none.
inline const char* option_name(uint32_t id, double value) {
  const ParamSpec& s = spec(id);
  const int index = static_cast<int>(std::lround(value - s.min));
  const int count = static_cast<int>(std::lround(s.max - s.min)) + 1;
  if (index < 0 || index >= count) return nullptr;
  if (id == kPreset) return g_preset_names ? g_preset_names[index] : nullptr;
  if (s.unit != Unit::Options || !s.options) return nullptr;
  return s.options[index];
}

inline bool format_value(uint32_t id, double value, char* out, uint32_t capacity) {
  if (id >= kParamCount || !out || capacity == 0) return false;
  const ParamSpec& s = spec(id);
  if (const char* name = option_name(id, value)) { std::snprintf(out, capacity, "%s", name); return true; }
  if (id == kPan) {
    if (std::abs(value) < 0.005) std::snprintf(out, capacity, "Centre");
    else std::snprintf(out, capacity, "%s %.0f", value < 0 ? "L" : "R", std::abs(value) * 100.0);
    return true;
  }
  switch (s.unit) {
    case Unit::Ms: std::snprintf(out, capacity, value < 100.0 ? "%.1f ms" : "%.0f ms", value); break;
    case Unit::Seconds: std::snprintf(out, capacity, "%.2f s", value); break;
    case Unit::Percent: std::snprintf(out, capacity, "%.0f%%", value * 100.0); break;
    case Unit::Db: std::snprintf(out, capacity, "%+.1f dB", value); break;
    case Unit::Hz: std::snprintf(out, capacity, value < 100.0 ? "%.2f Hz" : "%.1f Hz", value); break;
    case Unit::Semitones: std::snprintf(out, capacity, s.stepped ? "%+.0f st" : "%+.2f st", value); break;
    case Unit::Cents: std::snprintf(out, capacity, s.min < 0 ? "%+.1f ct" : "%.1f ct", value); break;
    case Unit::Count: std::snprintf(out, capacity, "%.0f", value); break;
    case Unit::Metres: std::snprintf(out, capacity, value < 1.0 ? "%.2f m" : "%.1f m", value); break;
    case Unit::Celsius: std::snprintf(out, capacity, "%.0f C", value); break;
    case Unit::Ratio: std::snprintf(out, capacity, "x%.2f", value); break;

    case Unit::Plain:
      if (s.stepped && s.min == 0 && s.max == 1) std::snprintf(out, capacity, value >= 0.5 ? "On" : "Off");
      else std::snprintf(out, capacity, "%g", value);
      break;
    case Unit::Options: std::snprintf(out, capacity, "%g", value); break;
  }
  return true;
}

// The reverse of format_value: accepts the formatted text, a bare number, an option name
// (case-insensitively), or "on"/"off" for a toggle.
inline bool parse_value(uint32_t id, const char* text, double* value) {
  if (id >= kParamCount || !text || !value) return false;
  const ParamSpec& s = spec(id);
  std::string_view in(text);
  while (!in.empty() && (in.front() == ' ' || in.front() == '\t')) in.remove_prefix(1);
  while (!in.empty() && (in.back() == ' ' || in.back() == '\t')) in.remove_suffix(1);
  if (in.empty()) return false;
  const auto equals_ci = [](std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
      if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
    return true;
  };
  const int count = static_cast<int>(std::lround(s.max - s.min)) + 1;
  if (s.stepped) {
    for (int i = 0; i < count; ++i)
      if (const char* name = option_name(id, s.min + i); name && equals_ci(in, name)) { *value = s.min + i; return true; }
    if (s.min == 0 && s.max == 1 && s.unit == Unit::Plain) {
      if (equals_ci(in, "on") || equals_ci(in, "yes")) { *value = 1; return true; }
      if (equals_ci(in, "off") || equals_ci(in, "no")) { *value = 0; return true; }
    }
  }
  if (id == kPan && (in.front() == 'L' || in.front() == 'l' || in.front() == 'R' || in.front() == 'r' || equals_ci(in, "centre") || equals_ci(in, "center") || equals_ci(in, "c"))) {
    if (equals_ci(in, "centre") || equals_ci(in, "center") || equals_ci(in, "c")) { *value = 0.0; return true; }
    const double amount = std::strtod(std::string(in.substr(1)).c_str(), nullptr) / 100.0;
    *value = clamp_to_spec(id, (in.front() == 'L' || in.front() == 'l') ? -amount : amount);
    return true;
  }
  if (s.unit == Unit::Ratio && (in.front() == 'x' || in.front() == 'X')) in.remove_prefix(1);
  char* end = nullptr;
  std::string copy(in);
  const double parsed = std::strtod(copy.c_str(), &end);
  if (end == copy.c_str() || !std::isfinite(parsed)) return false;
  *value = clamp_to_spec(id, s.unit == Unit::Percent ? parsed / 100.0 : parsed);
  // A bare number without the percent sign inside the range is taken literally.
  if (s.unit == Unit::Percent && std::strchr(copy.c_str(), '%') == nullptr && parsed >= s.min && parsed <= s.max)
    *value = clamp_to_spec(id, parsed);
  return true;
}

}  // namespace bonkr::params
