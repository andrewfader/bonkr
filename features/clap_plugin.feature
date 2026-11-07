Feature: The plug-in through the CLAP ABI

  # plugin_smoke :: the_module_exports_a_compatible_entry_and_one_instrument
  Scenario: the module exports a compatible entry and one instrument
    Given the built plug-in module
    Then its descriptor names BONKR as a stereo instrument

  # plugin_smoke :: an_instance_offers_the_core_extensions_and_ports
  Scenario: an instance offers the core extensions and ports
    Given a created and initialised instance
    Then audio out, note in, params, state, tail, latency and voice info are all there

  # plugin_smoke :: parameters_are_consistent_with_their_declared_ranges
  Scenario: parameters are consistent with their declared ranges
    Given the parameter list as the host sees it
    Then every parameter has a unique id and name, a default inside its range, and formats and parses its default

  # plugin_smoke :: the_instance_activates_processes_silence_and_sleeps
  Scenario: the instance activates processes silence and sleeps
    Given an activated instance with no notes
    Then a block of silence comes out and the output is finite

  # plugin_smoke :: two_instances_are_independent
  Scenario: two instances are independent
    Given two instances from one module
    Then a change in one does not touch the other

  # note_tests :: a_clap_note_on_strikes_the_kit_and_it_rings_out
  Scenario: a clap note on strikes the kit and it rings out
    Given the default kit (studio rock), given a CLAP note on for the snare (D1, key 38)
    Then the snare sounds at once, and dies away by itself though the key is still held
    When the note is released by id
    Then the plug-in stays silent

  # note_tests :: midi_notes_chokes_and_panic_are_honoured
  Scenario: midi notes chokes and panic are honoured
    Given MIDI note on for the 18 inch crash (A1 is the crash 2, key 57) on the studio kit
    When the drummer grabs it (polyphonic pressure on its key)
    Then it stops within a fraction of a second
    When a CLAP choke event arrives for a ringing ride (key 51)
    Then the ride stops too
    When the telephone bell is held in the foley kit and all notes off (CC 123) arrives
    Then it stops ringing and dies away

  # note_tests :: events_are_applied_at_their_sample_offset
  Scenario: events are applied at their sample offset
    Given a note on scheduled 200 samples into a 256-sample block, the kick (key 36)
    Then the first 200 samples are exactly silent and the stroke starts right at the event

  # note_tests :: parameter_automation_changes_the_sound_and_flush_works_offline
  Scenario: parameter automation changes the sound and flush works offline
    Given the master level set by a parameter event inside process, then by flush, on a ringing tam-tam
    Then the level drops by about 24 dB (less the tam-tam's own decay over the gap)
    When a parameter arrives through flush
    Then the plug-in reports it
    When an out-of-range value arrives
    Then it is clamped to the declared range

  # note_tests :: choosing_a_preset_rewrites_the_other_parameters
  Scenario: choosing a preset rewrites the other parameters
    Given the preset parameter set to 'In the Air Tonight toms'
    Then the kit becomes the gated 80s kit, in a room, compressed and gated
    When a parameter is edited afterwards and the same preset index is sent again
    Then the edit survives (re-sending the current preset is not a reload)

  # note_tests :: every_sound_sounds_through_the_plugin
  Scenario: every sound sounds through the plugin
    Given each sound chosen by parameter in chromatic play, struck once on C4
    Then every one is audible and finite

  # note_tests :: tail_reports_longer_in_a_church
  Scenario: tail reports longer in a church
    Given the tail extension on the kit alone, then in a church with the microphones 5 m away
    Then the wet tail is longer and both are sane

  # state_tests :: state_round_trips_every_parameter
  Scenario: state round trips every parameter
    Given an instance with every parameter set away from its default
    When the state is loaded into a fresh instance
    Then every value matches and saving again gives identical bytes

  # state_tests :: state_round_trip_is_audio_invariant
  Scenario: state round trip is audio invariant
    Given a preset ('Timpani', retuned 3.5 semitones down) rendered, saved, and loaded into a new instance
    Then the two renders are the same sound

  # state_tests :: malformed_state_is_rejected_and_leaves_the_instance_usable
  Scenario: malformed state is rejected and leaves the instance usable
    Given a valid state and several corruptions of it
    Then an empty stream, a truncated stream, a bad magic and a future version are all refused
    When the good state is then loaded
    Then it loads and its values apply

  # state_tests :: older_states_with_fewer_parameters_load_with_defaults
  Scenario: older states with fewer parameters load with defaults
    Given a state whose header claims fewer parameters than this build has
    Then it loads, the stored values apply and the rest return to defaults

