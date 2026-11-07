Feature: The room and the air

  # air_tests :: sound_reaches_the_microphone_at_the_speed_of_sound
  Scenario: sound reaches the microphone at the speed of sound
    Given a microphone 5 m from the source in the open, in air at 0 C and at 30 C
    Then the click arrives 5 / (331.3 sqrt(1 + T / 273.15)) s later, within a sample

  # air_tests :: the_air_takes_the_top_end_off_as_iso_9613_gives
  Scenario: the air takes the top end off as iso 9613 gives
    Given a microphone 10 m away in the open, in air at 20 C and 20% humidity
    Then an 8 kHz sine loses what ISO 9613-1 gives over 10 m within 1 dB, and humid air takes less

  # air_tests :: a_room_rings_for_its_sabine_time
  Scenario: a room rings for its sabine time
    Given a 3000 cubic-metre hall with walls of absorption 0.2, the microphone 3 m away
    Then its impulse response in the 500 Hz octave decays within 25% of Sabine's time

  # air_tests :: a_horn_across_a_field_loses_its_top_to_the_air
  Scenario: a horn across a field loses its top to the air
    Given a vuvuzela holding B-flat 4, heard by a microphone at the bell and by one 10 m away in the open, in air at 20 C and 20% humidity
    Then the fundamental arrives at the level it left (the preamp is set for the direct sound)
    Then the harmonic near 8 kHz loses what ISO 9613-1 gives over 10 m, within 0.5 dB, and humid air takes less of it

