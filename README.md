# BONKR: drums, percussion, machines and blasts from their physics

BONKR is a cross-platform CLAP instrument that makes drums and percussion from their physics
rather than from samples: struck membranes, plates, bars and shells, shaken particles, snare
wires, analogue drum-machine circuits, and the sounds of machines, fireworks and artillery from
blast waves, shock waves and moving sources. It builds on Linux, Windows and macOS with a native
editor on all three. It follows the model of [SCHRODR](https://github.com/andrewfader/schrodr),
[FUZZBOI](https://github.com/andrewfader/fuzzboi), [YANES](https://github.com/andrewfader/yanes)
and HONKR: a header-only physics engine, a thin CLAP shell, a native editor, and a suite that
proves every claim from the audio. Its quality gate is **recordings of the real
instruments**: 211 recordings of 72 of its sounds, against which each must measure within the
spread real instruments show (see [references/README.md](references/README.md)).

## What is in it

**121 sounds**, each built at the stroke from its physical description:

| Family | Sounds |
|---|---|
| Drum kit | four kicks (studio 22", open vintage, jazz 18", deep 26"); snares (maple, brass piccolo, fat 70s, rimshot, cross-stick, brush tap, brush sweep); toms 10", 12", 14", 16"; hi-hat closed, pedal, half-open, open; crash 16" and 18", ride and its bell, china, splash, a mallet roll |
| Orchestra | timpani, concert bass drum, tam-tam, opera gong, triangle (open and muted), tubular bell, clash cymbals, tambourine (shaken and struck), castanets and a roll, sleigh bells, wood and temple blocks, claves, slapstick, ratchet (grogger), anvil |
| Hand drums and Latin | bongos, congas (open, slap, muted), tumba, djembe (bass, tone, slap), tabla, frame drum, cowbell, agogo, guiro, maracas, cabasa, shaker, rain stick, vibraslap |
| Taiko | o-daiko, nagado-daiko, shime-daiko, the kara (rim) |
| Hands | a hand clap, a group of clappers, a finger snap |
| TR-808 | kick, snare, clap, closed and open hats, cymbal, cowbell, tom, conga, rimshot, claves, maracas: its circuits |
| TR-909 | kick, snare, clap, closed and open hats, crash, ride (real cymbals through 6 bits, as the 909's were), tom, rimshot |
| CR-78 | kick, snare, hat, guiro |
| Junkyard | trash can, its lid, an oil drum, a dropped metal pipe, a smashed pane of glass |
| Machines and the environment | an electromechanical telephone bell, a washing machine, a jet flyby (Doppler), a vuvuzela, an air-raid siren |
| Fireworks and artillery | firework launch, burst, crackle and whistle; the 1812 cannon, a howitzer, a mortar, a rifle, a machine gun, a grenade, a big explosion, thunder, a sonic boom, a whip crack |

**Twenty kits** on the General MIDI drum map (kick on C1, snare on D1, hats on F#1, G#1, A#1,
cymbals and Latin percussion above), so any GM drum part plays any of them: studio rock, jazz
brushes, funk, a big room, gated 80s, orchestral, Latin, taiko, world hand drums, the TR-808,
TR-909 and CR-78, the LinnDrum, Oberheim DMX, E-mu SP-1200, Akai MPC60 and Alesis HR-16 (acoustic
models through each machine's converter), a junkyard, foley and machines, artillery and
fireworks. Or play **one sound across the keyboard**, C4 at its own pitch.

**The player**: the striker (each sound's own, or a stick, felt mallet, hand, brush, rods or hard
mallet) meeting the surface by Hertzian contact, its hardness, where it lands, how velocity
reaches the stroke's speed; a drum struck again while it rings is the same drum struck again.
**The drum**: decay, damping, the tension glide of a hard stroke, the snares and their strainer,
the resonant head. **The machine**: the converters (LinnDrum and DMX 8-bit companded, SP-1200 and
MPC60 12-bit, HR-16 16-bit), MPC swing and note repeat from the host's tempo, drive, the hi-hat
choke. **The studio**: the listen-mic compressor and a gate on the room, for the gated drums of
the early 1980s. **The room and its air**, as in HONKR. Chokes from the host (CLAP note choke) or
an electronic kit's cymbal grab (polyphonic pressure).

**47 presets**, among them "In the Air Tonight toms", "Gated snare, 1985", "Stairwell drums
(Levee)", "808 boom, long and driven", "909 house, swung", "SP-1200 boom bap", "MPC60 swing",
"1812 Overture cannons", "Jet flyby", "Thunder", "Telephone bell", "Vuvuzela", "Air-raid siren",
"Metal pipe, dropped", "Ratchet (grogger)" and "Fireworks finale", each held to the [quality
bar](docs/QUALITY.md).

See [docs/PHYSICS.md](docs/PHYSICS.md) for what is simulated, how, and what is not.

## Building

Requirements: CMake 3.20+, a C++20 compiler, and on Linux `libx11-dev`, `libxft-dev` and
`pkg-config` (plus DejaVu fonts at run time). The CLAP headers are fetched at configure time,
pinned by tag.

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    ctest --test-dir build --output-on-failure
    cmake --install build --prefix ~/.local      # puts BONKR.clap in ~/.local/clap

Offline, point the fetch at an existing checkout: `-DFETCHCONTENT_SOURCE_DIR_CLAP=/path/to/clap`.
`-DBONKR_EDITOR=OFF` builds without the native editor.

## How it is tested

Every suite is a plain executable with Given/When/Then scenarios (`tests/spec.hpp`); `features/`
maps each scenario to its test. Nothing depends on a display, audio hardware, installed plug-ins
or the network.

| Suite | Proves |
|---|---|
| `dsp_tests` | the contact pulse, a mode's frequency and decay, the converters' quantisation, the compressor, the limiter, the filters |
| `physics_tests` | the Bessel zeros; f01 from tension and density; every mode scaling with tuning; the tension glide; Hertzian contact against speed; snare wires buzzing and their strainer; the kettle's near-harmonic timpani; the Euler-Bernoulli bar ratios; a gong's rising pitch; a cymbal's density and cascade; Kinney and Graham's blasts; Whitham's N-waves; a jet's Doppler shift; the vuvuzela's B-flat; the siren winding up and down; the telephone's 20 Hz ringer; the ratchet's teeth; shaken particles; every sound finite and dying away |
| `kit_tests` | the GM map in every kit, chromatic tuning, the hi-hat choke, restriking a ringing drum, MPC swing, note repeat, the stereo picture |
| `air_tests` | the room and its air against the physics |
| `engine_tests` | strokes and held sounds, velocity, every sound calibrated to one peak, every preset against the quality bar, the limiter, the tail, the converters, the compressor and the gate |
| `reference_tests` | every referenced sound within its gate against recordings of the real instrument, and no worse than its baseline |
| `plugin_smoke`, `note_tests`, `state_tests` | the built `.clap` through the public CLAP ABI: discovery, ports, parameters, events at sample offsets, MIDI, chokes, presets, every sound, state |
| `advertised_tests` | one proof per parameter from audio; fails if a parameter has no proof |
| `ui_tests`, `frontend_tests` | the editor against a fake host and a recording canvas; the plug-in side of the editor |

Tools: `build/bonkr-note-render` renders any sound or kit key to WAV; `build/bonkr-preset-audit`
renders and measures every preset (`--fit` refits their levels); `build/bonkr-reference` measures
the recordings and scores BONKR against them; `build/bonkr-ui-preview` renders every editor page
offscreen. [AGENTS.md](AGENTS.md) is the verification contract for changes.

## Layout

    src/dsp/modal.hpp     the stroke (Hertzian contact) and the body (modes; membranes, plates, bars, shells)
    src/dsp/sources.hpp   noise bands, particles, snare wires, circuits, blasts and shocks, the lip horn, a flyby
    src/dsp/voice.hpp     every sound's recipe, and one sounding voice
    src/dsp/catalog.hpp   the sounds' names, in the order of their saved values
    src/dsp/kits.hpp      the kits on the GM map
    src/dsp/machine.hpp   the converters, the compressor, the gate
    src/dsp/air.hpp       the room and the air
    src/engine.hpp        voices, chokes, swing, note repeat, the bus, level calibration, the limiter
    src/params.hpp        the parameter table; src/presets.hpp the presets
    src/bonkr.cpp         the CLAP plug-in; src/ui/ the editor
    references/           the reference recordings' manifest, features and BONKR's scores
