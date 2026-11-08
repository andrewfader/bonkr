# What BONKR simulates

Every sound is built at the stroke from its physical description (a head's diameter, tension and
areal density; a cymbal's radius, thickness and bronze; a charge in kilograms and a distance in
metres) out of the parts below. Where a sound could not be derived from first principles alone,
its constants were fitted to recordings of the real thing (references/README.md), and the source
says so.

## The stroke (src/dsp/modal.hpp)

A stick, mallet, beater, hand or clapper meets the surface through **Hertzian contact**: force grows
as compression to the power alpha (1.5 for hard elastic bodies, 2.5 to 3.5 for felt and flesh).
Solving the collision gives a contact time that falls as v^-((alpha - 1)/(alpha + 1)) and an
impulse m v (1 + e). The force is a half-sine of that duration carrying that impulse, sampled at
mid-interval so even a one-sample contact carries it. A harder stroke is shorter, so brighter, and
felt brightens with force more than wood does (`physics_tests`: 4^-0.2 for a stick, 4^-0.47 for
felt). A brush is many small contacts spread over 9 ms; rods, seven over 2 ms.

Velocity sets the stroke's speed, 0.4 m/s at ppp to 12 m/s at fff, exponentially. The Hardness
knob scales the contact time by 4 either way; the Striker replaces each sound's own striker.

## The body: modal synthesis

The body is the sum of its normal modes, each a complex one-pole oscillator (exact in frequency
and decay, unconditionally stable, cheap to retune) driven by the force times the mode's shape
where it is struck and heard through its shape where it radiates. The frequencies come from the
geometry:

- **Circular membranes** (Rayleigh): f_mn = j_mn / (pi d) sqrt(T / sigma), j_mn the Bessel zeros,
  computed by Newton's method on Bessel's integral (Abramowitz and Stegun to 1e-4). The shape at
  radius r is J_m(j_mn r / a): a centre stroke rings only the breathing (m = 0) modes.
- **Two heads** couple through the air between them: the breathing modes split into a pair, the
  heads moving together (lower, radiating strongly) and apart.
- **Kettles** (timpani) load the head so the (1,1) to (5,1) modes sit near 2 : 3 : 4 : 5 : 6
  (Rossing's measured 1.50, 1.98, 2.44, 2.94); the tabla's syahi makes its modes harmonic (Raman).
- **Free circular plates** (cymbals, gongs, lids): f = lambda^2 / (2 pi a^2) sqrt(D / rho h),
  Leissa's lambda^2 for the low modes and Chladni's law beyond, stiffened by the dome; 72 modes
  spread over the plate's whole range. A thin plate radiates better toward its coincidence
  frequency, so radiation rises as sqrt(f).
- **Free-free bars** (triangle, tubular bell, claves, pipe): f_n proportional to (beta_n L)^2,
  4.730, 7.853, 10.996, ... ; a tubular bell's 4th to 6th modes fall near 2 : 3 : 4, its strike note
  an octave below the 4th.
- **Thin cylindrical shells** (trash can, oil drum, washing machine tub): Donnell-Mushtari.
- **Measured tables** where geometry is too irregular to derive (cowbell, agogo, anvil, telephone
  gongs, wood blocks, castanets): ratios, decays and gains from the literature, fitted to
  recordings.

**Tension modulation** (Kirchhoff-Carrier): large vibration stretches a membrane, so a hard
stroke starts sharp and falls, f = f0 sqrt(1 + beta A^2), A the body's amplitude at the hardest
stroke. A small opera gong has a softening nonlinearity: its pitch rises as it dies.

**Cymbals' cascade**: energy flows from a cymbal's low modes into its high ones, roughly as the
square of the amplitude. BONKR feeds a high-band noise whose amplitude follows k A^2 with a time
constant of a few milliseconds (hundreds for a tam-tam, whose swell is famous): soft strokes are
dark, hard ones bright, and the high end dies faster than the low.

**Damping** (a hand, a felt, a pillow, the hi-hat's pedal) shortens every mode, the high ones most.

## What is not a single body (src/dsp/sources.hpp)

- **Snare wires** rest against the snare-side head, pressed up by the strainer with a fixed
  acceleration. When the head pulls away faster than that, they part, fly and slap back; every
  slap is a click of the closing speed into their own broad resonances. Tighter wires part less.
- **Particles** (Cook's PhISEM): beads, seeds, jingles and shards in a shell: collisions at a rate
  proportional to their number and the shaken energy, ringing the shell's resonances.
- **Hand claps** (Peltola et al.): the air between the palms pushed out, a damped resonance of
  about 1 to 2 kHz; a group is several clappers, each its own.
- **The TR-808's circuits**: bridged-T resonators rung by the trigger pulse (kick, toms, congas,
  rim, claves), the snare's two resonators and its snappy noise, six square oscillators at
  205.3, 304.4, 369.6, 522.7, 540 and 800 Hz through band-passes (hats, cymbal), two of them for
  the cowbell, and the clap's saw-tooth envelope firing three times. The **TR-909**: swept
  oscillators for kick, snare and toms; its cymbals and hats were 6-bit recordings of real ones,
  so BONKR plays a real cymbal through six bits. The **CR-78**: the same kinds of circuits, tuned
  and shortened.
- **Blast waves**: Friedlander's waveform, p(t) = Ps (1 - t/td) exp(-b t/td), with Kinney and
  Graham's peak overpressure and positive duration at the scaled distance Z = R / W^(1/3); the
  ground reflection; the terrain's echoes; and the scattered tail of many small reflections from
  trees, walls and ground. Each path (and the scattered tail, the long way round) loses ISO
  9613-1's absorption over its length at the Temperature and Humidity knobs' air, and the echoes'
  delays follow that air's speed of sound: a distant cannon is duller on a dry day. A microphone
  hears from 40 Hz up. Tune scales the charge: an octave down is eight times the TNT.
- **N-waves** (Whitham): a supersonic body's pressure jumps up, falls straight through zero and
  jumps back; its duration grows as M d r^(1/4) / (c (M^2 - 1)^(3/8) l^(1/4)): a quarter of a
  millisecond for a rifle bullet, 120 ms for a sonic boom, tens of microseconds for a whip.
- **Thunder** (Few): a kinked channel kilometres long, each segment sending its N-wave, arriving
  over seconds, bunched where the channel crosses the line of sight.
- **A passing jet**: heard at the time its sound left it, so the emission clock runs at
  1 / (1 + dr/dt / c) of the listener's and the Doppler shift falls out, c the speed of sound at
  the Temperature knob (a cold day shifts it further); the fan's whine radiates forward, the
  jet's roar aft; 1/r spreading and ISO 9613-1's absorption over r at the knobs' air.
- **The vuvuzela**: lips locked to the horn's resonance near B-flat 3 let the breath through in
  pulses, heard through the cone's harmonic impedance peaks and its bell.
- **The air-raid siren**: a rotor's ports chopping the air, the pitch ports times turns per
  second, the motor spinning up while held and coasting down after.
- **The telephone**: an armature at the 20 Hz ringing current throws a steel clapper between two
  gongs of different pitch, a strike on each half-cycle, ringing 2 s on and 4 s off while held.
- **Repeating gestures**: a ratchet's teeth (12 at 4 turns a second), a guiro's ridges, a
  vibraslap's rivets, a bouncing pipe (restitution 0.45), a castanet roll, a machine gun, a
  washing machine's tumbling load and its buttons on the tub.

## After the stroke (src/dsp/machine.hpp, src/dsp/air.hpp)

- **Converters**: a zero-order hold at the machine's clock, quantised to its storage: 8-bit
  mu-law companded for the LinnDrum (28 kHz) and DMX (25 kHz), 12-bit linear for the SP-1200
  (26.04 kHz, no filters, so it aliases) and MPC60 (40 kHz), 16-bit for the HR-16 (47.6 kHz),
  with their anti-aliasing and reconstruction filters where they had them. Clock rates and
  filters are approximations from the machines' published specifications.
- **Swing** (MPC): notes on the second sixteenth of each eighth play later, to the swing's share
  of the eighth, following the host's transport. **Note repeat** strikes a held key again at a
  division of the tempo.
- **Drive**: tanh saturation into the converter.
- **The listen-mic compressor**: 20 : 1 over a threshold down to -40 dBFS, 0.1 ms attack, 60 ms
  release. **The gate** opens on the dry kit and holds the room open for its hold time: with a big
  room it is the gated drums of the early 1980s.
- **The room and the air**: ISO 9613-1 absorption, the speed of sound, image-source early
  reflections and a Sabine-timed feedback-delay tail, heard by an omni, cardioid or figure-8 pair
  (the room model is shared with HONKR and documented in src/dsp/air.hpp).

## Not modelled

The membrane's own nonlinear mode coupling beyond the tension glide; air loading of drums other
than the kettle; the shell's own vibration except as a few rim and shell modes; the hi-hat's
continuous openness (it has closed, pedal, half-open and open sounds); the exact circuits of the
sample-playback machines (BONKR plays acoustic models through their converters instead of their
ROMs); wind, ground impedance and temperature gradients outdoors; anything about a sound's
source beyond what its recipe states.
