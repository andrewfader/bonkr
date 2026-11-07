// Which parameters the editor shows, where, and as what kind of control. Grouped by what a
// drummer reaches for rather than by parameter ID; the tests check that every parameter is
// reachable from exactly one place.
#pragma once

#include <vector>

#include "../params.hpp"
#include "editor.hpp"

namespace bonkr::ui {

inline std::vector<PageSpec> bonkr_pages() {
  using namespace bonkr::params;
  using W = Widget;
  const auto knob = [](int id) { return ControlSpec{W::Knob, id}; };
  const auto toggle = [](int id) { return ControlSpec{W::Toggle, id}; };
  const auto segments = [](int id, int span) { return ControlSpec{W::Segments, id, span}; };
  const auto menu = [](int id, int span) { return ControlSpec{W::Menu, id, span}; };
  return {
      {"KIT", "Choose a kit for the GM drum map, or one sound to play across the keyboard  •  hover any control for help", theme::amber, 3, 190,
       {{"Kit", 0, {segments(kLayout, 3), menu(kKit, 3)}},
        {"Sound", 1, {menu(kSound, 3), knob(kTune), knob(kMasterDb)}}}},
      {"STROKE", "What strikes the drum, how hard it is, where it lands and how the velocity reaches it", theme::orange, 3, 168,
       {{"Striker", 0, {menu(kStriker, 2), knob(kHardness)}},
        {"Stroke", 1, {knob(kPosition), knob(kDynamics)}}}},
      {"DRUM", "The struck body: how long it rings, what damps it, how its pitch falls after a hard stroke, the snares and the shell", theme::green, 3, 168,
       {{"Body", 0, {knob(kDecay), knob(kDamping), knob(kTension)}},
        {"Snares", 1, {knob(kSnares), knob(kSnareTension)}},
        {"Shell", 2, {knob(kResonance)}}}},
      {"MACHINE", "The drum machines' converters and their feel: swing, note repeat, input drive, the hi-hat's choke", theme::red, 3, 168,
       {{"Converter", 0, {menu(kConverter, 3), knob(kDrive)}},
        {"Groove", 1, {knob(kSwing), menu(kNoteRepeat, 2)}},
        {"Hi-hat", 2, {toggle(kChoke)}}}},
      {"STUDIO", "The listen-mic compressor and the gate on the room: the gated drums of the early 1980s; the stereo picture", theme::violet, 3, 168,
       {{"Listen mic", 0, {knob(kCrush)}},
        {"Gate", 1, {knob(kGate), knob(kGateHold)}},
        {"Stereo", 2, {knob(kStereoWidth), knob(kPan)}}}},
      {"ROOM", "The room the kit stands in, the air, and the microphones", theme::cyan, 3, 150,
       {{"Room", 0, {menu(kRoomSize, 2), knob(kRoomAbsorption), knob(kMicDistance)}},
        {"Microphones", 1, {menu(kMicPattern, 2), segments(kMicSetup, 2)}},
        {"Air", 2, {knob(kHumidity), knob(kTemperature)}}}},
  };
}

// Names for the knob cells, where the section title already supplies the context. Null means
// the full parameter name fits.
inline const char* bonkr_short_name(int id) {
  using namespace bonkr::params;
  switch (id) {
    case kMasterDb: return "Level";
    case kTension: return "Glide";
    case kSnares: return "Amount";
    case kSnareTension: return "Tension";
    case kNoteRepeat: return "Repeat";
    case kChoke: return "Choke";
    case kGateHold: return "Hold";
    case kGate: return "Threshold";
    case kStereoWidth: return "Width";
    case kRoomAbsorption: return "Absorption";
    case kMicDistance: return "Distance";
    case kMicPattern: return "Pattern";
    case kMicSetup: return "Setup";
    default: return nullptr;
  }
}

// The preset menu in the header is reachable from every page.
inline ControlSpec bonkr_header_control() { return ControlSpec{Widget::Menu, bonkr::params::kPreset}; }

// Parameters edited through page artwork rather than a card control (none at present).
inline std::vector<int> bonkr_strip_params() { return {}; }

// Whether a parameter does anything with this layout, kit and sound. The rules are the sounds':
// snares for snare drums, a shell for drums with one, glide for bodies whose tension changes.
inline bool param_relevant(int id, const double* values) {
  using namespace bonkr::params;
  const bool chromatic = static_cast<int>(values[kLayout]) == kLayoutChromatic;
  const int sound = static_cast<int>(values[kSound]);
  switch (id) {
    case kKit: return !chromatic;
    case kSound: return chromatic;
    case kSnares: case kSnareTension:
      return !chromatic || (sound >= dsp::kSnareMaple && sound <= dsp::kSnareBrushSweep);
    case kChoke: return !chromatic;
    case kRoomAbsorption: return values[kRoomSize] > 0.0;
    case kMicPattern: case kMicSetup: return values[kRoomSize] > 0.0 || values[kMicDistance] > 0.0;
    case kGateHold: return values[kGate] > 0.0;
    default: return true;
  }
}

}  // namespace bonkr::ui
