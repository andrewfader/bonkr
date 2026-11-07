// Every sound BONKR can make, by name, in the order of its saved value. A sound's index is what
// the Sound parameter and the kits store, so new sounds are only ever appended.
#pragma once

namespace bonkr::dsp {

#define BONKR_SOUNDS(X)                                                                          \
  /* the drum kit */                                                                             \
  X(KickStudio, "Kick, studio 22in")                                                             \
  X(KickVintage, "Kick, open vintage")                                                           \
  X(KickJazz, "Kick, jazz 18in")                                                                 \
  X(KickDeep, "Kick, deep 26in")                                                                 \
  X(SnareMaple, "Snare, maple")                                                                  \
  X(SnareBrass, "Snare, brass piccolo")                                                          \
  X(SnareFat, "Snare, fat 70s")                                                                  \
  X(SnareRimshot, "Snare, rimshot")                                                              \
  X(SnareCrossStick, "Snare, cross-stick")                                                       \
  X(SnareBrush, "Snare, brush tap")                                                              \
  X(SnareBrushSweep, "Snare, brush sweep")                                                       \
  X(TomHigh, "Tom, 10in")                                                                        \
  X(TomMid, "Tom, 12in")                                                                         \
  X(TomFloor, "Floor tom, 14in")                                                                 \
  X(TomFloorLow, "Floor tom, 16in")                                                              \
  X(HatClosed, "Hi-hat, closed")                                                                 \
  X(HatPedal, "Hi-hat, pedal")                                                                   \
  X(HatOpen, "Hi-hat, open")                                                                     \
  X(HatHalf, "Hi-hat, half-open")                                                                \
  X(Crash16, "Crash, 16in")                                                                      \
  X(Crash18, "Crash, 18in")                                                                      \
  X(Ride, "Ride, 20in")                                                                          \
  X(RideBell, "Ride, bell")                                                                      \
  X(China, "China, 18in")                                                                        \
  X(Splash, "Splash, 10in")                                                                      \
  X(CymbalSwell, "Cymbal, mallet roll")                                                          \
  /* the orchestra */                                                                            \
  X(Timpani, "Timpani")                                                                          \
  X(BassDrum, "Concert bass drum")                                                               \
  X(TamTam, "Tam-tam")                                                                           \
  X(OperaGong, "Opera gong")                                                                     \
  X(Triangle, "Triangle")                                                                        \
  X(TriangleMuted, "Triangle, muted")                                                            \
  X(TubularBell, "Tubular bell")                                                                 \
  X(ClashCymbals, "Clash cymbals")                                                               \
  X(TambourineShake, "Tambourine, shake")                                                        \
  X(TambourineHit, "Tambourine, hit")                                                            \
  X(Castanets, "Castanets")                                                                      \
  X(CastanetRoll, "Castanets, roll")                                                             \
  X(SleighBells, "Sleigh bells")                                                                 \
  X(WoodBlock, "Wood block")                                                                     \
  X(TempleBlock, "Temple block")                                                                 \
  X(Claves, "Claves")                                                                            \
  X(Slapstick, "Slapstick")                                                                      \
  X(Ratchet, "Ratchet (grogger)")                                                                \
  X(Anvil, "Anvil")                                                                              \
  /* hand drums and Latin percussion */                                                          \
  X(BongoHigh, "Bongo, macho")                                                                   \
  X(BongoLow, "Bongo, hembra")                                                                   \
  X(CongaOpen, "Conga, open")                                                                    \
  X(CongaSlap, "Conga, slap")                                                                    \
  X(CongaMute, "Conga, muted")                                                                   \
  X(Tumba, "Tumba, open")                                                                        \
  X(DjembeBass, "Djembe, bass")                                                                  \
  X(DjembeTone, "Djembe, tone")                                                                  \
  X(DjembeSlap, "Djembe, slap")                                                                  \
  X(Tabla, "Tabla, dayan")                                                                       \
  X(FrameDrum, "Frame drum")                                                                     \
  X(Cowbell, "Cowbell")                                                                          \
  X(AgogoHigh, "Agogo, high")                                                                    \
  X(AgogoLow, "Agogo, low")                                                                      \
  X(GuiroShort, "Guiro, short")                                                                  \
  X(GuiroLong, "Guiro, long")                                                                    \
  X(Maracas, "Maracas")                                                                          \
  X(Cabasa, "Cabasa")                                                                            \
  X(Shaker, "Shaker")                                                                            \
  X(RainStick, "Rain stick")                                                                     \
  X(Vibraslap, "Vibraslap")                                                                      \
  /* taiko */                                                                                    \
  X(ODaiko, "O-daiko")                                                                           \
  X(Nagado, "Nagado-daiko")                                                                      \
  X(Shime, "Shime-daiko")                                                                        \
  X(TaikoRim, "Taiko, kara (rim)")                                                               \
  /* hands */                                                                                    \
  X(Clap, "Hand clap")                                                                           \
  X(GroupClap, "Hand claps, group")                                                              \
  X(Snap, "Finger snap")                                                                         \
  /* the machines */                                                                             \
  X(TR808Kick, "808 kick")                                                                       \
  X(TR808Snare, "808 snare")                                                                     \
  X(TR808Clap, "808 clap")                                                                       \
  X(TR808HatClosed, "808 closed hat")                                                            \
  X(TR808HatOpen, "808 open hat")                                                                \
  X(TR808Cymbal, "808 cymbal")                                                                   \
  X(TR808Cowbell, "808 cowbell")                                                                 \
  X(TR808Tom, "808 tom")                                                                         \
  X(TR808Conga, "808 conga")                                                                     \
  X(TR808Rim, "808 rimshot")                                                                     \
  X(TR808Claves, "808 claves")                                                                   \
  X(TR808Maracas, "808 maracas")                                                                 \
  X(TR909Kick, "909 kick")                                                                       \
  X(TR909Snare, "909 snare")                                                                     \
  X(TR909Clap, "909 clap")                                                                       \
  X(TR909HatClosed, "909 closed hat")                                                            \
  X(TR909HatOpen, "909 open hat")                                                                \
  X(TR909Crash, "909 crash")                                                                     \
  X(TR909Ride, "909 ride")                                                                       \
  X(TR909Tom, "909 tom")                                                                         \
  X(TR909Rim, "909 rimshot")                                                                     \
  X(CR78Kick, "CR-78 kick")                                                                      \
  X(CR78Snare, "CR-78 snare")                                                                    \
  X(CR78Hat, "CR-78 hat")                                                                        \
  X(CR78Guiro, "CR-78 guiro")                                                                    \
  /* the junkyard */                                                                             \
  X(TrashCan, "Trash can")                                                                       \
  X(TrashLid, "Trash can lid")                                                                   \
  X(OilDrum, "Oil drum")                                                                         \
  X(MetalPipe, "Metal pipe, dropped")                                                            \
  X(GlassSmash, "Glass smash")                                                                   \
  /* machines and the environment */                                                             \
  X(Telephone, "Telephone bell")                                                                 \
  X(WashingMachine, "Washing machine")                                                           \
  X(JetFlyby, "Jet flyby")                                                                       \
  X(Vuvuzela, "Vuvuzela")                                                                        \
  X(Siren, "Air-raid siren")                                                                     \
  X(FireworkLaunch, "Firework, launch")                                                          \
  X(FireworkBurst, "Firework, burst")                                                            \
  X(FireworkCrackle, "Firework, crackle")                                                        \
  X(FireworkWhistle, "Firework, whistle")                                                        \
  X(Cannon, "Cannon (1812)")                                                                     \
  X(Howitzer, "Howitzer")                                                                        \
  X(Mortar, "Mortar")                                                                            \
  X(Rifle, "Rifle shot")                                                                         \
  X(MachineGun, "Machine gun")                                                                   \
  X(Grenade, "Grenade")                                                                          \
  X(Explosion, "Big explosion")                                                                  \
  X(Thunder, "Thunder")                                                                          \
  X(SonicBoom, "Sonic boom")                                                                     \
  X(WhipCrack, "Whip crack")

enum Sound : int {
#define BONKR_ENUM(id, name) k##id,
  BONKR_SOUNDS(BONKR_ENUM)
#undef BONKR_ENUM
  kSoundCount
};

inline constexpr const char* kSoundNames[] = {
#define BONKR_NAME(id, name) name,
    BONKR_SOUNDS(BONKR_NAME)
#undef BONKR_NAME
};
static_assert(sizeof(kSoundNames) / sizeof(kSoundNames[0]) == kSoundCount);

// The families, for the editor's pictures and the help.
enum class Family : int { Membrane, Cymbal, Bar, Bell, Wood, Particles, Hands, Circuit, Junk, Machine, Blast };

inline Family family(int s) {
  if (s <= kTomFloorLow) return Family::Membrane;
  if (s <= kCymbalSwell) return Family::Cymbal;
  if (s == kTimpani || s == kBassDrum) return Family::Membrane;
  if (s == kTamTam || s == kOperaGong || s == kClashCymbals) return Family::Cymbal;
  if (s == kTriangle || s == kTriangleMuted || s == kTubularBell || s == kAnvil) return Family::Bar;
  if (s == kTambourineShake || s == kSleighBells || (s >= kGuiroShort && s <= kVibraslap) || s == kRatchet) return Family::Particles;
  if (s == kTambourineHit) return Family::Membrane;
  if (s == kCastanets || s == kCastanetRoll || s == kWoodBlock || s == kTempleBlock || s == kClaves || s == kSlapstick) return Family::Wood;
  if (s >= kBongoHigh && s <= kFrameDrum) return Family::Membrane;
  if (s == kCowbell || s == kAgogoHigh || s == kAgogoLow) return Family::Bell;
  if (s >= kODaiko && s <= kShime) return Family::Membrane;
  if (s == kTaikoRim) return Family::Wood;
  if (s >= kClap && s <= kSnap) return Family::Hands;
  if (s >= kTR808Kick && s <= kCR78Guiro) return Family::Circuit;
  if (s >= kTrashCan && s <= kGlassSmash) return Family::Junk;
  if (s >= kTelephone && s <= kSiren) return Family::Machine;
  return Family::Blast;
}

}  // namespace bonkr::dsp
