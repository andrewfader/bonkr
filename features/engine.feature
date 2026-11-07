Feature: The engine: strokes, levels, presets, limiter, converter, compressor and gate

  # engine_tests :: a_stroke_rings_out_by_itself_and_a_held_sound_stops_on_release
  Scenario: a stroke rings out by itself and a held sound stops on release
    Given the snare on the studio kit, struck and held for a second
    Then it sounds at once and has died away before the key is let go
    Given the vuvuzela, blown for half a second and let go
    Then it sounds while held and stops after the release

  # engine_tests :: velocity_reaches_the_stroke
  Scenario: velocity reaches the stroke
    Given the floor tom struck softly and hard
    Then the hard stroke is more than 20 dB louder and brighter
    When dynamics is off
    Then both strokes are the same

  # engine_tests :: every_sound_is_calibrated_to_the_same_peak
  Scenario: every sound is calibrated to the same peak
    Given every sound's trim, measured from the model at this sample rate
    Then a reference stroke through its trim peaks at -9 dBFS
    Then the trims are measured afresh at another sample rate, and agree within a few dB (more for the random ones)

  # engine_tests :: every_preset_meets_the_quality_bar
  Scenario: every preset meets the quality bar
    Given every factory preset, playing the audit phrase (docs/QUALITY.md)

  # engine_tests :: the_output_limiter_holds_the_ceiling_and_leaves_quiet_signals_alone
  Scenario: the output limiter holds the ceiling and leaves quiet signals alone
    Given the whole studio kit struck at once, as hard as possible, with the master at +12 dB
    Then nothing leaves above the -2 dBFS ceiling and the engine flags the squash
    Given one quiet stroke with the master at -24 dB
    Then the limiter never acts

  # engine_tests :: the_advertised_tail_covers_the_decay
  Scenario: the advertised tail covers the decay
    Given the tam-tam struck in a church, the microphones far away
    Then by the end of the advertised tail it is 50 dB down

  # engine_tests :: the_converter_holds_and_quantises_at_the_machines_clock
  Scenario: the converter holds and quantises at the machines clock
    Given the SP-1200 converter on a tom
    Then over the loud first 20 ms its output moves in steps held for 48000 / 26040 samples, and the clean one does not
    Given the LinnDrum kit on its own converter, and on the LinnDrum converter chosen by hand
    Then they are the same: the kit's own converter is the LinnDrum's

  # engine_tests :: drive_and_the_compressor_raise_the_tail_against_the_hit
  Scenario: drive and the compressor raise the tail against the hit
    Given the floor tom in a club, plain and through the listen-mic compressor
    Then the compressed tail sits at least 10 dB closer to the hit

  # engine_tests :: the_gate_cuts_the_room_after_its_hold
  Scenario: the gate cuts the room after its hold
    Given the snare in a hall, ungated and gated with a 250 ms hold
    Then before the hold ends they are the same; 50 ms after it the gated room is 30 dB quieter

