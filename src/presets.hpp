// Factory presets: named starting points, a kit or one sound each, in the rooms their music is
// played in. A preset is the parameters it changes from their defaults, so it stays valid when a
// parameter is appended to the table. Every preset is measured against docs/QUALITY.md by
// `bonkr-preset-audit` and held to it by engine_tests. The order is append-only, because the
// preset index is an automatable parameter value.
#pragma once

#include <cstddef>
#include <vector>

#include "dsp/kits.hpp"
#include "params.hpp"

namespace bonkr::presets {

struct Setting { params::ParamId id; double value; };
struct Preset { const char* name; std::vector<Setting> settings; };

using namespace params;

inline const std::vector<Preset>& all() {
  static const std::vector<Preset> table{
      // --- acoustic kits ----------------------------------------------------------------------------
      {"Studio rock kit", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kStudioRock}, {kRoomSize, 3}, {kMicDistance, 1.5}, {kMasterDb, -1.3}}},
      {"Jazz brushes, club", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kJazz}, {kRoomSize, 4}, {kMicDistance, 2.0}, {kStereoWidth, 0.5}, {kMasterDb, -1.9}}},
      {"Funk, dry", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kFunk}, {kRoomSize, 1}, {kMicDistance, 0.6}, {kDamping, 0.25}, {kMasterDb, 0.2}}},
      {"Stairwell drums (Levee)", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kBigRoom}, {kRoomSize, 5}, {kRoomAbsorption, 0.06}, {kMicDistance, 7.0}, {kCrush, 0.35}, {kMasterDb, -10.3}}},
      {"In the Air Tonight toms", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kGated80s}, {kRoomSize, 4}, {kRoomAbsorption, 0.08}, {kMicDistance, 3.0}, {kCrush, 0.75}, {kGate, 0.55}, {kGateHold, 260}, {kMasterDb, -3.0}}},
      {"Gated snare, 1985", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kGated80s}, {kRoomSize, 5}, {kRoomAbsorption, 0.1}, {kMicDistance, 4.0}, {kCrush, 0.5}, {kGate, 0.6}, {kGateHold, 330}, {kMasterDb, -4.0}}},
      {"Orchestral percussion", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kOrchestral}, {kRoomSize, 5}, {kMicDistance, 6.0}, {kMasterDb, -10.3}}},
      {"Latin percussion", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kLatin}, {kRoomSize, 3}, {kMicDistance, 1.5}, {kMasterDb, -1.8}}},
      {"Taiko ensemble", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kTaiko}, {kRoomSize, 5}, {kMicDistance, 8.0}, {kStereoWidth, 0.9}, {kMasterDb, -9.5}}},
      {"World hand drums", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kWorld}, {kRoomSize, 3}, {kMicDistance, 1.2}, {kMasterDb, -2.7}}},
      // --- drum machines -----------------------------------------------------------------------------
      {"TR-808", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kKit808}, {kMasterDb, -1.1}}},
      {"808 boom, long and driven", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kKit808}, {kDecay, 3.0}, {kDrive, 0.45}, {kMasterDb, -6.3}}},
      {"TR-909", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kKit909}, {kCrush, 0.15}, {kMasterDb, -9.1}}},
      {"909 house, swung", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kKit909}, {kSwing, 0.58}, {kCrush, 0.2}, {kMasterDb, -9.7}}},
      {"CR-78", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kKitCr78}, {kMasterDb, 2.0}}},
      {"LinnDrum", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kLinn}, {kRoomSize, 3}, {kMicDistance, 0.8}, {kMasterDb, 3.8}}},
      {"Oberheim DMX", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kDmx}, {kDrive, 0.2}, {kMasterDb, 3.4}}},
      {"SP-1200 boom bap", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kSp1200}, {kSwing, 0.56}, {kDrive, 0.25}, {kDamping, 0.15}, {kMasterDb, -0.5}}},
      {"MPC60 swing", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kMpc60}, {kSwing, 0.62}, {kMasterDb, 3.4}}},
      {"Alesis HR-16", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kHr16}, {kRoomSize, 4}, {kMicDistance, 2.0}, {kMasterDb, 0.1}}},
      // --- found sounds and the environment -------------------------------------------------------------
      {"Junkyard", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kJunkyard}, {kRoomSize, 4}, {kMicDistance, 2.0}, {kMasterDb, -3.9}}},
      {"Foley and machines", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kFoley}, {kRoomSize, 3}, {kMicDistance, 1.5}, {kMasterDb, -4.3}}},
      {"Artillery and fireworks", {{kLayout, kLayoutKit}, {kKit, dsp::kits::kArtillery}, {kMasterDb, 4.4}}},
      {"1812 Overture cannons", {{kLayout, kLayoutChromatic}, {kSound, dsp::kCannon}, {kMasterDb, 13.3}}},
      {"Gated floor tom, chromatic", {{kLayout, kLayoutChromatic}, {kSound, dsp::kTomFloor}, {kRoomSize, 4}, {kRoomAbsorption, 0.08}, {kMicDistance, 3.0}, {kCrush, 0.75}, {kGate, 0.55}, {kGateHold, 260}, {kMasterDb, -1.0}}},
      // --- single instruments across the keyboard ---------------------------------------------------------
      {"Timpani", {{kLayout, kLayoutChromatic}, {kSound, dsp::kTimpani}, {kTune, -2}, {kRoomSize, 5}, {kMicDistance, 6.0}, {kMasterDb, -1.2}}},
      {"Tubular bells", {{kLayout, kLayoutChromatic}, {kSound, dsp::kTubularBell}, {kRoomSize, 5}, {kMicDistance, 5.0}, {kMasterDb, -0.5}}},
      {"Triangle", {{kLayout, kLayoutChromatic}, {kSound, dsp::kTriangle}, {kRoomSize, 5}, {kMicDistance, 4.0}, {kMasterDb, -2.1}}},
      {"Tam-tam", {{kLayout, kLayoutChromatic}, {kSound, dsp::kTamTam}, {kTune, 0}, {kRoomSize, 5}, {kMicDistance, 6.0}, {kMasterDb, -8.3}}},
      {"Opera gong", {{kLayout, kLayoutChromatic}, {kSound, dsp::kOperaGong}, {kRoomSize, 4}, {kMicDistance, 2.0}, {kMasterDb, -2.3}}},
      {"Trash can", {{kLayout, kLayoutChromatic}, {kSound, dsp::kTrashCan}, {kRoomSize, 3}, {kMicDistance, 1.0}, {kMasterDb, 4.0}}},
      {"Telephone bell", {{kLayout, kLayoutChromatic}, {kSound, dsp::kTelephone}, {kRoomSize, 2}, {kMicDistance, 1.5}, {kMasterDb, -3.7}}},
      {"Vuvuzela", {{kLayout, kLayoutChromatic}, {kSound, dsp::kVuvuzela}, {kMicDistance, 6.0}, {kMasterDb, -8.9}}},
      {"Air-raid siren", {{kLayout, kLayoutChromatic}, {kSound, dsp::kSiren}, {kMicDistance, 12.0}, {kMasterDb, -3.6}}},
      {"Jet flyby", {{kLayout, kLayoutChromatic}, {kSound, dsp::kJetFlyby}, {kMasterDb, -5.5}}},
      {"Thunder", {{kLayout, kLayoutChromatic}, {kSound, dsp::kThunder}, {kMasterDb, 1.2}}},
      {"Washing machine", {{kLayout, kLayoutChromatic}, {kSound, dsp::kWashingMachine}, {kRoomSize, 2}, {kMicDistance, 1.0}, {kMasterDb, -1.1}}},
      {"Metal pipe, dropped", {{kLayout, kLayoutChromatic}, {kSound, dsp::kMetalPipe}, {kRoomSize, 3}, {kMicDistance, 2.0}, {kMasterDb, -4.7}}},
      {"Glass smash", {{kLayout, kLayoutChromatic}, {kSound, dsp::kGlassSmash}, {kRoomSize, 3}, {kMicDistance, 1.5}, {kMasterDb, -3.0}}},
      {"Rifle range", {{kLayout, kLayoutChromatic}, {kSound, dsp::kRifle}, {kMasterDb, 13.3}}},
      {"Machine gun", {{kLayout, kLayoutChromatic}, {kSound, dsp::kMachineGun}, {kMasterDb, 12.8}}},
      {"Fireworks finale", {{kLayout, kLayoutChromatic}, {kSound, dsp::kFireworkBurst}, {kMasterDb, 13.3}}},
      {"Sonic boom", {{kLayout, kLayoutChromatic}, {kSound, dsp::kSonicBoom}, {kMasterDb, 13.3}}},
      {"Ratchet (grogger)", {{kLayout, kLayoutChromatic}, {kSound, dsp::kRatchet}, {kRoomSize, 3}, {kMicDistance, 1.0}, {kMasterDb, -0.4}}},
      {"Castanet roll", {{kLayout, kLayoutChromatic}, {kSound, dsp::kCastanetRoll}, {kRoomSize, 4}, {kMicDistance, 1.5}, {kMasterDb, 5.6}}},
      {"Hand claps, group", {{kLayout, kLayoutChromatic}, {kSound, dsp::kGroupClap}, {kRoomSize, 4}, {kMicDistance, 2.0}, {kMasterDb, 2.0}}},
      {"808 cowbell", {{kLayout, kLayoutChromatic}, {kSound, dsp::kTR808Cowbell}, {kMasterDb, 9.8}}},
  };
  return table;
}

inline size_t count() { return all().size(); }

inline void apply(size_t index, double* values) {
  for (size_t i = 0; i < params::kParamCount; ++i) values[i] = params::kSpecs[i].def;
  if (index >= count()) return;
  for (const Setting& s : all()[index].settings) values[s.id] = s.value;
  values[params::kPreset] = static_cast<double>(index);
}

inline const char* const* names() {
  static std::vector<const char*> list = [] {
    std::vector<const char*> n;
    for (const Preset& p : all()) n.push_back(p.name);
    return n;
  }();
  return list.data();
}

}  // namespace bonkr::presets
