Feature: The physics every sound is built from

  # physics_tests :: the_membrane_modes_are_the_bessel_zeros
  Scenario: the membrane modes are the bessel zeros
    Given the membrane's mode table, computed from J_m by Newton's method
    Then its first modes are the tabulated zeros j01, j11, j21, j02, j31, j12 (Abramowitz and Stegun 9.5)
    Then J_0 at its first zero is zero, and J_1 at 1.0 is 0.4400506

  # physics_tests :: a_drumhead_sounds_its_tension_over_its_density
  Scenario: a drumhead sounds its tension over its density
    Given a 14 inch head at 2750 N/m of 0.353 kg/m^2 Mylar
    Then f01 = 2.405 / (pi d) sqrt(T / sigma) is 189.8 Hz, and four times the tension doubles it
    When a frame drum (one head, 18 inch, goatskin at 320 N/m) is struck off centre
    Then its spectrum has the (1,1) and (2,1) modes at 1.594 and 2.136 times f01

  # physics_tests :: tuning_a_drum_scales_every_mode
  Scenario: tuning a drum scales every mode
    Given the 12 inch tom, then the same tom an octave up (four times the tension)
    Then its (1,1) mode, which the other head does not split, moves from 1.593 f01 to twice that

  # physics_tests :: a_hard_stroke_starts_sharp_and_falls
  Scenario: a hard stroke starts sharp and falls
    Given the 14 inch floor tom struck as hard as the keys allow
    When its lowest mode (the heads breathing together) is measured in the first 60 ms and after half a second
    Then tension modulation starts it between 30 and 250 cents sharp; without it the pitch holds

  # physics_tests :: a_harder_stroke_is_a_shorter_contact
  Scenario: a harder stroke is a shorter contact
    Given Hertzian contact for a stick (alpha 1.5) and a felt mallet (alpha 2.8)
    Then four times the speed shortens the contact by 4^-((alpha - 1)/(alpha + 1)): 0.758 and 0.519
    When the timpani is struck softly and hard with its felt mallet
    Then the hard stroke is brighter (a higher spectral centroid)

  # physics_tests :: snare_wires_buzz_against_the_head
  Scenario: snare wires buzz against the head
    Given the maple snare with its snares on and thrown off
    Then with the snares on, far more of the sound lies above 3 kHz after the stick has left
    When the wires are tightened
    Then tight wires part from the head less: less buzz after 60 ms than loose ones

  # physics_tests :: the_kettle_makes_the_timpani_nearly_harmonic
  Scenario: the kettle makes the timpani nearly harmonic
    Given the timpani tuned to D3 (its principal (1,1) mode at 146.8 Hz)
    Then the (2,1) and (3,1) modes sound a fifth and an octave above it (Rossing's 1.50 and 1.98)

  # physics_tests :: bars_ring_the_euler_bernoulli_ratios
  Scenario: bars ring the euler bernoulli ratios
    Given the free-free bar's modes
    Then they stand at 1 : 2.756 : 5.404 : 8.933 : 13.34
    Then so a tubular bell's 4th, 5th and 6th modes are nearly 2 : 3 : 4, the strike note an octave below the 4th
    When rosewood claves 25 mm across and 0.2 m long are struck
    Then they ring at the bar's fundamental, between 2 and 2.8 kHz like real claves

  # physics_tests :: a_gong_rises_as_it_dies
  Scenario: a gong rises as it dies
    Given the opera gong, struck hard
    Then its pitch climbs by more than a quarter-tone as the vibration dies (a softening nonlinearity)

  # physics_tests :: a_cymbal_is_dense_and_its_highs_bloom
  Scenario: a cymbal is dense and its highs bloom
    Given an 18 inch crash struck firmly
    Then its spectrum is dense: more than 40 separate peaks between 300 Hz and 16 kHz
    Then and, with the energy its low modes pass up into the high ones, a third or more of it lies above 4 kHz, as in the recordings
    Then and the cascade grows with the square of the stroke: a soft stroke is darker

  # physics_tests :: blasts_follow_kinney_and_graham
  Scenario: blasts follow kinney and graham
    Given Kinney and Graham's fits
    Then overpressure falls with scaled distance and is 0.0986 atmospheres at 10 m/kg^(1/3)
    When the cannon's pressure (0.45 kg TNT equivalent, 80 m away) arrives, before any microphone
    Then its positive phase lasts Kinney and Graham's duration
    When the charge is eight times larger (an octave down)
    Then its positive phase is longer

  # physics_tests :: supersonic_bodies_trail_n_waves
  Scenario: supersonic bodies trail n waves
    Given Whitham's N-wave
    Then a 7.62 mm bullet at Mach 2.5 passing 5 m away makes one of 0.15 to 0.35 ms
    When the sonic boom (a 120 ms N-wave) is heard
    Then its two shocks, the jump up at the front and back up at the tail, are 120 ms apart

  # physics_tests :: a_passing_jet_is_doppler_shifted
  Scenario: a passing jet is doppler shifted
    Given the jet flyby: 140 m/s, 90 m off at its closest, its fan whining at 2450 Hz
    When the whine is measured as it approaches and as it leaves
    Then the approaching pitch is f / (1 - v cos/c) and the receding f / (1 + v cos/c)

  # physics_tests :: lips_buzz_a_vuvuzela_at_b_flat
  Scenario: lips buzz a vuvuzela at b flat
    Given the vuvuzela blown and held
    Then the lips lock to the horn's resonance at B-flat 3 (233 Hz), within 40 cents

  # physics_tests :: a_siren_winds_up_while_held_and_down_after
  Scenario: a siren winds up while held and down after
    Given the air-raid siren held for 3 seconds, then let go
    Then its pitch (ports times turns per second) climbs while the motor drives it and falls as it coasts

  # physics_tests :: a_telephone_bell_rings_at_the_ringing_current
  Scenario: a telephone bell rings at the ringing current
    Given the telephone bell held for a second
    When the envelope's modulation is measured
    Then the clapper's swing repeats at the 20 Hz ringing current, striking one gong on each half

  # physics_tests :: a_ratchet_clacks_once_per_tooth
  Scenario: a ratchet clacks once per tooth
    Given the ratchet turned for a second: 12 teeth, 4 turns a second
    Then it clacks 48 times a second

  # physics_tests :: shaken_particles_ring_their_shell
  Scenario: shaken particles ring their shell
    Given the maracas, shaken
    Then the clicks of the seeds ring the gourd's resonance near 5.5 kHz (where the recordings put it), so the sound's centroid lies near it
    Then and the shake's return stroke comes back after the seeds have settled

  # physics_tests :: every_sound_is_finite_and_settles
  Scenario: every sound is finite and settles
    Given every sound, struck once at a firm velocity and released after 0.3 s

