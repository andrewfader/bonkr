Feature: Kits, chromatic play, chokes, restrikes, swing and note repeat

  # kit_tests :: every_kit_puts_its_drums_where_general_midi_puts_them
  Scenario: every kit puts its drums where general midi puts them
    Given every drum-set kit's map
    Then C1 (36) is a kick, D1 (38) a snare, F#1 (42) a closed hat and A#1 (46) an open one, in the acoustic and sampled sets
    Then the machines' kits use their own circuits on the same keys
    Then and every kit strikes something on the kick and snare keys

  # kit_tests :: chromatic_play_tunes_by_the_key
  Scenario: chromatic play tunes by the key
    Given the timpani across the keyboard
    Then C4 sounds its own D3 and G4 a fifth higher, 700 cents
    When Tune is -12 semitones
    Then it sounds an octave down

  # kit_tests :: closing_the_hi_hat_stops_the_open_one
  Scenario: closing the hi hat stops the open one
    Given the open hi-hat struck, then the pedal closed 150 ms later
    Then with the choke on, the open hat has stopped by 300 ms; with it off, it still rings

  # kit_tests :: a_ringing_drum_struck_again_is_the_same_drum
  Scenario: a ringing drum struck again is the same drum
    Given the ride struck twelve times in a row
    Then one voice carries it, its ring building on itself as a real ride's does
    Given twelve different kit keys struck together
    Then each has its own voice

  # kit_tests :: swing_delays_the_second_sixteenth_of_each_eighth
  Scenario: swing delays the second sixteenth of each eighth
    Given the transport playing at 120 bpm with 66% swing, a closed hat on each sixteenth of a beat
    Then the off-beat sixteenths sound (0.66 - 0.5) of an eighth late, 40 ms; the on-beat ones at once
    Given the transport stopped
    Then nothing is delayed

  # kit_tests :: note_repeat_strikes_while_held
  Scenario: note repeat strikes while held
    Given note repeat at 1/16 and 120 bpm, the snare held for one second
    Then it strikes every 125 ms while held: eight strokes, then no more

  # kit_tests :: the_kit_is_spread_as_the_audience_sees_it
  Scenario: the kit is spread as the audience sees it
    Given the hi-hat and the floor tom, at full width
    Then the hat sits right and the floor tom left (the drummer faces the audience)
    When the width is none
    Then the hat is in the middle

