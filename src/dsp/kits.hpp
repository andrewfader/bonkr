// The kits: which sound each key plays, tuned, levelled and placed as a drummer sets them up,
// on the General MIDI percussion map (35 to 81) and its usual extensions, so any GM drum part
// plays every kit. The environmental kits keep the map's roles where they have them (a cannon on
// the kick, the telephone on the cowbell) and fill the rest.
#pragma once

#include <array>

#include "catalog.hpp"

namespace bonkr::dsp::kits {

struct Slot {
  int sound = -1;       // -1: the key is silent
  float tune = 0.0f;    // semitones from the sound's own pitch
  float level = 0.0f;   // dB
  float pan = 0.0f;     // -1 left .. 1 right, from the audience
  int choke = 0;        // a struck key mutes the ringing keys of its group
};
using Map = std::array<Slot, 128>;

// The converter each kit was heard through (see machine.hpp; 0 is none).
enum Converter : int { kConvKit, kConvClean, kConvLinn, kConvDmx, kConvSp1200, kConvMpc60, kConvHr16, kConverterCount };

constexpr int kKitCount = 20;
constexpr const char* kKitNames[kKitCount] = {
    "Studio rock", "Jazz brushes", "Funk, dry", "Big room", "Gated 80s", "Orchestral", "Latin", "Taiko",
    "World hand drums", "TR-808", "TR-909", "CR-78", "LinnDrum", "Oberheim DMX", "E-mu SP-1200", "Akai MPC60",
    "Alesis HR-16", "Junkyard", "Foley and machines", "Artillery and fireworks"};
enum Kit : int {
  kStudioRock, kJazz, kFunk, kBigRoom, kGated80s, kOrchestral, kLatin, kTaiko, kWorld, kKit808, kKit909, kKitCr78,
  kLinn, kDmx, kSp1200, kMpc60, kHr16, kJunkyard, kFoley, kArtillery
};
// Each kit's own converter.
constexpr std::array<int, kKitCount> kKitConverter{
    kConvClean, kConvClean, kConvClean, kConvClean, kConvClean, kConvClean, kConvClean, kConvClean, kConvClean,
    kConvClean, kConvClean, kConvClean, kConvLinn, kConvDmx, kConvSp1200, kConvMpc60, kConvHr16, kConvClean, kConvClean, kConvClean};

inline void put(Map& m, int key, int sound, float tune = 0.0f, float level = 0.0f, float pan = 0.0f, int choke = 0) {
  m[static_cast<size_t>(key)] = Slot{sound, tune, level, pan, choke};
}

// The percussion every kit shares above the drum set (GM 54 to 87).
inline void percussion(Map& m) {
  put(m, 54, kTambourineShake, 0, -4, 0.35f);
  put(m, 56, kCowbell, 0, -6, 0.2f);
  put(m, 58, kVibraslap, 0, -6, 0.3f);
  put(m, 60, kBongoHigh, 0, -2, 0.25f);
  put(m, 61, kBongoLow, 0, -2, 0.3f);
  put(m, 62, kCongaMute, 0, -2, -0.2f);
  put(m, 63, kCongaOpen, 0, -2, -0.2f);
  put(m, 64, kTumba, 0, -2, -0.3f);
  put(m, 65, kTomHigh, 7.0f, -3, 0.4f);   // timbales: shallow metal shells, tuned high
  put(m, 66, kTomMid, 5.0f, -3, 0.45f);
  put(m, 67, kAgogoHigh, 0, -6, 0.1f);
  put(m, 68, kAgogoLow, 0, -6, 0.1f);
  put(m, 69, kCabasa, 0, -6, -0.35f);
  put(m, 70, kMaracas, 0, -6, -0.4f);
  put(m, 71, kFireworkWhistle, 8.0f, -12, 0.0f);  // short whistle
  put(m, 72, kFireworkWhistle, 3.0f, -12, 0.0f);  // long whistle
  put(m, 73, kGuiroShort, 0, -4, 0.35f);
  put(m, 74, kGuiroLong, 0, -4, 0.35f);
  put(m, 75, kClaves, 0, -6, 0.15f);
  put(m, 76, kWoodBlock, 0, -6, 0.2f);
  put(m, 77, kWoodBlock, -4.0f, -6, 0.2f);
  put(m, 78, kTabla, 0, -4, -0.1f);   // cuica's keys: the nearest pitched hand drum
  put(m, 79, kTabla, 5.0f, -4, -0.1f);
  put(m, 80, kTriangleMuted, 0, -8, 0.5f, 2);
  put(m, 81, kTriangle, 0, -8, 0.5f, 2);
  put(m, 82, kShaker, 0, -6, -0.3f);
  put(m, 83, kSleighBells, 0, -8, 0.3f);
  put(m, 84, kTubularBell, 0, -8, 0.4f);
  put(m, 85, kCastanets, 0, -6, -0.2f);
  put(m, 86, kFrameDrum, 0, -2, 0.0f);   // mute surdo
  put(m, 87, kFrameDrum, -3.0f, 0, 0.0f);  // open surdo
}

// A drum set on the GM map: kick, snare, toms, hats and cymbals, panned from the audience.
struct Set {
  int kick = kKickStudio, kick2 = kKickVintage, snare = kSnareMaple, snare2 = kSnareFat, side = kSnareCrossStick, clap = kClap;
  int tom_lo = kTomFloorLow, tom_mid_lo = kTomFloor, tom_mid = kTomMid, tom_hi = kTomHigh;
  int hat_closed = kHatClosed, hat_pedal = kHatPedal, hat_open = kHatOpen;
  int crash = kCrash16, crash2 = kCrash18, ride = kRide, bell = kRideBell, china = kChina, splash = kSplash;
  float tom_tune = 0.0f;
};
inline void drum_set(Map& m, const Set& s) {
  put(m, 35, s.kick2, 0, 0, 0.0f);
  put(m, 36, s.kick, 0, 0, 0.0f);
  put(m, 37, s.side, 0, -2, 0.1f);
  put(m, 38, s.snare, 0, 0, 0.1f);
  put(m, 39, s.clap, 0, -2, 0.0f);
  put(m, 40, s.snare2, 0, 0, 0.1f);
  put(m, 41, s.tom_lo, s.tom_tune - 2.0f, 0, -0.5f);
  put(m, 42, s.hat_closed, 0, -3, 0.45f, 1);
  put(m, 43, s.tom_lo, s.tom_tune, 0, -0.4f);
  put(m, 44, s.hat_pedal, 0, -4, 0.45f, 1);
  put(m, 45, s.tom_mid_lo, s.tom_tune, 0, -0.25f);
  put(m, 46, s.hat_open, 0, -4, 0.45f, 1);
  put(m, 47, s.tom_mid, s.tom_tune - 1.0f, 0, -0.05f);
  put(m, 48, s.tom_mid, s.tom_tune + 1.0f, 0, 0.1f);
  put(m, 49, s.crash, 0, -3, 0.35f);
  put(m, 50, s.tom_hi, s.tom_tune, 0, 0.25f);
  put(m, 51, s.ride, 0, -4, -0.45f);
  put(m, 52, s.china, 0, -4, -0.6f);
  put(m, 53, s.bell, 0, -4, -0.45f);
  put(m, 55, s.splash, 0, -5, 0.5f);
  put(m, 57, s.crash2, 0, -3, -0.35f);
  put(m, 59, s.ride, -1.0f, -4, -0.5f);
}

inline Map make(int kit) {
  Map m{};
  percussion(m);
  Set s;
  switch (kit) {
    case kStudioRock: drum_set(m, s); break;
    case kJazz:
      s.kick = s.kick2 = kKickJazz; s.snare = kSnareBrush; s.snare2 = kSnareBrushSweep; s.ride = kRide; s.crash = kCrash18; s.tom_tune = 3.0f;
      drum_set(m, s);
      break;
    case kFunk:
      s.kick = kKickStudio; s.snare = kSnareBrass; s.snare2 = kSnareRimshot; s.tom_tune = 2.0f;
      drum_set(m, s);
      break;
    case kBigRoom:
      s.kick = kKickDeep; s.kick2 = kKickVintage; s.snare = kSnareFat; s.snare2 = kSnareMaple; s.crash = kCrash18; s.tom_tune = -2.0f;
      drum_set(m, s);
      break;
    case kGated80s:
      s.kick = kKickStudio; s.snare = kSnareMaple; s.snare2 = kSnareRimshot; s.tom_tune = -1.0f;
      drum_set(m, s);
      break;
    case kOrchestral:
      drum_set(m, s);
      put(m, 35, kBassDrum, -2.0f); put(m, 36, kBassDrum, 0);
      put(m, 41, kTimpani, -5.0f, 0, -0.3f); put(m, 43, kTimpani, -2.0f, 0, -0.25f); put(m, 45, kTimpani, 0.0f, 0, -0.2f);
      put(m, 47, kTimpani, 3.0f, 0, -0.15f); put(m, 48, kTimpani, 5.0f, 0, -0.1f); put(m, 50, kTimpani, 7.0f, 0, -0.05f);
      put(m, 49, kClashCymbals, 0, -2, 0.3f); put(m, 57, kClashCymbals, -1.0f, -2, 0.3f);
      put(m, 51, kCymbalSwell, 0, -3, 0.4f); put(m, 52, kTamTam, 0, -2, 0.5f); put(m, 53, kOperaGong, 0, -6, 0.5f);
      put(m, 55, kCrash16, 0, -5, 0.4f); put(m, 59, kTubularBell, 0, -6, 0.6f);
      put(m, 39, kSlapstick, 0, -3, 0.1f); put(m, 37, kWoodBlock, 0, -6, 0.2f);
      put(m, 42, kTriangleMuted, 0, -8, 0.6f, 2); put(m, 44, kSleighBells, 0, -8, 0.5f); put(m, 46, kTriangle, 0, -8, 0.6f, 2);
      put(m, 58, kRatchet, 0, -6, 0.3f);
      break;
    case kLatin:
      drum_set(m, s);
      put(m, 36, kCongaOpen, -7.0f, 0, -0.3f);
      put(m, 41, kTumba, -2.0f, 0, -0.3f); put(m, 43, kTumba, 0, 0, -0.3f); put(m, 45, kCongaOpen, 0, 0, -0.2f);
      put(m, 47, kCongaSlap, 0, 0, -0.2f); put(m, 48, kBongoLow, 0, 0, 0.25f); put(m, 50, kBongoHigh, 0, 0, 0.3f);
      put(m, 42, kShaker, 0, -6, 0.4f); put(m, 44, kGuiroShort, 0, -4, 0.4f); put(m, 46, kCabasa, 0, -6, 0.4f);
      put(m, 49, kAgogoHigh, 0, -6, 0.2f); put(m, 51, kCowbell, 0, -6, 0.2f); put(m, 53, kAgogoLow, 0, -6, 0.2f);
      break;
    case kTaiko:
      put(m, 35, kODaiko, -2.0f); put(m, 36, kODaiko, 0); put(m, 37, kTaikoRim, 0, -2, 0.2f); put(m, 38, kNagado, 0, 0, -0.25f);
      put(m, 39, kClap, 0, -4); put(m, 40, kNagado, 2.0f, 0, 0.25f); put(m, 41, kNagado, -3.0f, 0, -0.5f); put(m, 43, kNagado, -1.0f, 0, -0.4f);
      put(m, 45, kNagado, 1.0f, 0, 0.3f); put(m, 47, kNagado, 3.0f, 0, 0.4f); put(m, 48, kShime, -2.0f, -2, 0.5f); put(m, 50, kShime, 0, -2, 0.55f);
      put(m, 42, kShime, 2.0f, -4, -0.5f); put(m, 44, kTaikoRim, -3.0f, -3, -0.3f); put(m, 46, kOperaGong, -5.0f, -6, -0.4f);
      put(m, 49, kClashCymbals, 0, -4, 0.0f); put(m, 51, kAgogoHigh, -3.0f, -8, 0.2f);
      break;
    case kWorld:
      put(m, 35, kDjembeBass, -2.0f); put(m, 36, kDjembeBass, 0); put(m, 37, kDjembeTone, 0); put(m, 38, kDjembeSlap, 0, 0, 0.1f);
      put(m, 39, kClap, 0, -3); put(m, 40, kTabla, 0, 0, -0.2f); put(m, 41, kFrameDrum, -2.0f, 0, -0.4f); put(m, 43, kFrameDrum, 0, 0, -0.3f);
      put(m, 45, kTabla, -5.0f, 0, -0.2f); put(m, 47, kTabla, -2.0f, 0, -0.2f); put(m, 48, kTabla, 2.0f, 0, -0.2f); put(m, 50, kTabla, 5.0f, 0, -0.2f);
      put(m, 42, kShaker, 0, -6, 0.4f); put(m, 44, kCastanets, 0, -6, 0.4f); put(m, 46, kTambourineShake, 0, -6, 0.4f);
      put(m, 49, kSleighBells, 0, -6, 0.3f); put(m, 51, kAgogoHigh, 0, -6, 0.2f); put(m, 53, kRainStick, 0, -6, -0.3f);
      break;
    case kKit808:
      put(m, 35, kTR808Kick, -3.0f); put(m, 36, kTR808Kick, 0); put(m, 37, kTR808Rim, 0, -4, 0.1f); put(m, 38, kTR808Snare, 0, 0, 0.1f);
      put(m, 39, kTR808Clap, 0, -2, 0.0f); put(m, 40, kTR808Snare, 2.0f, 0, 0.1f);
      put(m, 41, kTR808Tom, -9.0f, 0, -0.4f); put(m, 43, kTR808Tom, -6.0f, 0, -0.3f); put(m, 45, kTR808Tom, -3.0f, 0, -0.15f);
      put(m, 47, kTR808Tom, 0.0f, 0, 0.0f); put(m, 48, kTR808Tom, 3.0f, 0, 0.15f); put(m, 50, kTR808Tom, 6.0f, 0, 0.3f);
      put(m, 42, kTR808HatClosed, 0, -4, 0.4f, 1); put(m, 44, kTR808HatClosed, -2.0f, -6, 0.4f, 1); put(m, 46, kTR808HatOpen, 0, -5, 0.4f, 1);
      put(m, 49, kTR808Cymbal, 0, -6, -0.3f); put(m, 51, kTR808Cymbal, 2.0f, -8, -0.3f); put(m, 56, kTR808Cowbell, 0, -6, 0.2f);
      put(m, 62, kTR808Conga, 0, -2, -0.2f); put(m, 63, kTR808Conga, -5.0f, -2, -0.2f); put(m, 64, kTR808Conga, -9.0f, -2, -0.3f);
      put(m, 70, kTR808Maracas, 0, -6, -0.4f); put(m, 75, kTR808Claves, 0, -6, 0.15f);
      break;
    case kKit909:
      put(m, 35, kTR909Kick, -2.0f); put(m, 36, kTR909Kick, 0); put(m, 37, kTR909Rim, 0, -4, 0.1f); put(m, 38, kTR909Snare, 0, 0, 0.1f);
      put(m, 39, kTR909Clap, 0, -2, 0.0f); put(m, 40, kTR909Snare, 2.0f, 0, 0.1f);
      put(m, 41, kTR909Tom, -7.0f, 0, -0.4f); put(m, 43, kTR909Tom, -5.0f, 0, -0.3f); put(m, 45, kTR909Tom, -2.0f, 0, -0.15f);
      put(m, 47, kTR909Tom, 0.0f, 0, 0.0f); put(m, 48, kTR909Tom, 3.0f, 0, 0.15f); put(m, 50, kTR909Tom, 5.0f, 0, 0.3f);
      put(m, 42, kTR909HatClosed, 0, -4, 0.4f, 1); put(m, 44, kTR909HatClosed, -2.0f, -6, 0.4f, 1); put(m, 46, kTR909HatOpen, 0, -5, 0.4f, 1);
      put(m, 49, kTR909Crash, 0, -5, -0.3f); put(m, 51, kTR909Ride, 0, -6, -0.35f); put(m, 57, kTR909Crash, 1.0f, -5, 0.3f);
      put(m, 59, kTR909Ride, -1.0f, -6, -0.35f);
      break;
    case kKitCr78:
      put(m, 35, kCR78Kick, -2.0f); put(m, 36, kCR78Kick, 0); put(m, 37, kTR808Rim, 3.0f, -6, 0.1f); put(m, 38, kCR78Snare, 0, 0, 0.1f);
      put(m, 39, kTR808Clap, 2.0f, -4, 0.0f); put(m, 40, kCR78Snare, 1.0f, 0, 0.1f);
      put(m, 42, kCR78Hat, 0, -4, 0.4f, 1); put(m, 44, kCR78Hat, -2.0f, -6, 0.4f, 1); put(m, 46, kTR808HatOpen, 1.0f, -6, 0.4f, 1);
      put(m, 41, kTR808Conga, -12.0f, -2, -0.4f); put(m, 43, kTR808Conga, -9.0f, -2, -0.3f); put(m, 45, kTR808Conga, -5.0f, -2, 0.0f);
      put(m, 47, kTR808Conga, -2.0f, -2, 0.1f); put(m, 48, kTR808Conga, 0.0f, -2, 0.2f); put(m, 50, kTR808Conga, 3.0f, -2, 0.3f);
      put(m, 49, kTR808Cymbal, 1.0f, -6, -0.3f); put(m, 54, kTR808Maracas, -2.0f, -6, 0.3f); put(m, 56, kTR808Cowbell, 2.0f, -6, 0.2f);
      put(m, 73, kCR78Guiro, 0, -4, 0.35f); put(m, 74, kCR78Guiro, -2.0f, -4, 0.35f); put(m, 60, kTR808Conga, 5.0f, -2, 0.25f); put(m, 61, kTR808Conga, 2.0f, -2, 0.3f);
      break;
    case kLinn:  // tight, bright, a little short: what was sampled for the LM-1 and LinnDrum
      s.kick = kKickStudio; s.snare = kSnareMaple; s.snare2 = kSnareRimshot; s.clap = kClap; s.tom_tune = 1.0f;
      drum_set(m, s);
      put(m, 51, kRide, 1.0f, -5, -0.45f); put(m, 49, kCrash16, 0, -4, 0.35f); put(m, 56, kCowbell, 1.0f, -6, 0.2f);
      break;
    case kDmx:  // a big, deep kick and a fat snare
      s.kick = kKickDeep; s.snare = kSnareFat; s.snare2 = kSnareMaple; s.clap = kGroupClap; s.tom_tune = -1.0f;
      drum_set(m, s);
      break;
    case kSp1200:
      s.kick = kKickVintage; s.kick2 = kKickDeep; s.snare = kSnareFat; s.snare2 = kSnareBrass; s.side = kSnareRimshot;
      drum_set(m, s);
      break;
    case kMpc60:
      s.kick = kKickVintage; s.kick2 = kKickStudio; s.snare = kSnareMaple; s.snare2 = kSnareFat; s.clap = kGroupClap;
      drum_set(m, s);
      break;
    case kHr16:
      s.kick = kKickStudio; s.snare = kSnareBrass; s.snare2 = kSnareRimshot; s.tom_tune = 2.0f;
      drum_set(m, s);
      break;
    case kJunkyard:
      put(m, 35, kOilDrum, -3.0f); put(m, 36, kOilDrum, 0); put(m, 37, kMetalPipe, 0, -6, 0.2f); put(m, 38, kTrashCan, 0, 0, 0.1f);
      put(m, 39, kSlapstick, 0, -2); put(m, 40, kTrashLid, 0, -2, 0.2f); put(m, 41, kOilDrum, 3.0f, 0, -0.4f); put(m, 43, kTrashCan, -5.0f, 0, -0.3f);
      put(m, 42, kTrashLid, 7.0f, -6, 0.4f); put(m, 44, kAnvil, 0, -8, 0.4f); put(m, 45, kTrashCan, -2.0f, 0, -0.2f); put(m, 46, kTrashLid, 3.0f, -5, 0.4f);
      put(m, 47, kTrashCan, 2.0f, 0, 0.0f); put(m, 48, kTrashCan, 5.0f, 0, 0.2f); put(m, 49, kGlassSmash, 0, -4, 0.3f); put(m, 50, kTrashCan, 8.0f, 0, 0.3f);
      put(m, 51, kAnvil, 2.0f, -8, -0.4f); put(m, 52, kMetalPipe, -5.0f, -6, -0.5f); put(m, 53, kAnvil, 5.0f, -8, -0.4f);
      break;
    case kFoley:
      put(m, 35, kWashingMachine, 0, 0, 0.0f); put(m, 36, kOilDrum, -5.0f); put(m, 37, kSnap, 0, -2, 0.1f); put(m, 38, kSlapstick, 0, 0, 0.1f);
      put(m, 39, kClap, 0, -2); put(m, 40, kWhipCrack, 0, -2, 0.2f); put(m, 41, kJetFlyby, -3.0f, -2, 0.0f); put(m, 42, kTelephone, 0, -6, 0.3f);
      put(m, 43, kJetFlyby, 0, -2); put(m, 44, kRatchet, 0, -4, 0.4f); put(m, 45, kVuvuzela, -2.0f, -4, -0.3f); put(m, 46, kSiren, 0, -6, -0.3f);
      put(m, 47, kVuvuzela, 0, -4, -0.2f); put(m, 48, kVuvuzela, 2.0f, -4, -0.1f); put(m, 49, kGlassSmash, 0, -3, 0.3f); put(m, 50, kMetalPipe, 0, -4, 0.3f);
      put(m, 51, kTelephone, 2.0f, -6, -0.3f); put(m, 52, kSonicBoom, 0, -2); put(m, 53, kThunder, 0, 0);
      break;
    case kArtillery:
      put(m, 35, kHowitzer, 0); put(m, 36, kCannon, 0); put(m, 37, kRifle, 0, -4, 0.2f); put(m, 38, kMortar, 0, 0, -0.2f);
      put(m, 39, kGrenade, 0, -2, 0.3f); put(m, 40, kMachineGun, 0, -4, -0.3f); put(m, 41, kExplosion, 0, 0); put(m, 42, kFireworkCrackle, 0, -4, 0.4f);
      put(m, 43, kCannon, -3.0f, 0, -0.4f); put(m, 44, kFireworkLaunch, 0, -2, -0.3f); put(m, 45, kCannon, 3.0f, 0, 0.4f); put(m, 46, kFireworkBurst, 0, -2, 0.2f);
      put(m, 47, kFireworkWhistle, 0, -6, -0.2f); put(m, 48, kFireworkBurst, 3.0f, -2, -0.3f); put(m, 49, kThunder, 0, 0); put(m, 50, kSonicBoom, 0, 0);
      put(m, 51, kFireworkCrackle, 2.0f, -4, -0.4f); put(m, 52, kExplosion, 3.0f, 0); put(m, 53, kWhipCrack, 0, -4, 0.3f);
      put(m, 54, kSiren, 0, -6, 0.0f); put(m, 56, kTelephone, 0, -6, 0.2f);
      break;
    default: break;
  }
  return m;
}

// Every kit's map, made once.
inline const std::array<Map, kKitCount>& all() {
  static const std::array<Map, kKitCount> maps = [] {
    std::array<Map, kKitCount> a{};
    for (int k = 0; k < kKitCount; ++k) a[static_cast<size_t>(k)] = make(k);
    return a;
  }();
  return maps;
}

}  // namespace bonkr::dsp::kits
