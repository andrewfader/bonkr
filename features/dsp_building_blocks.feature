Feature: The DSP building blocks

  # dsp_tests :: the_limiter_holds_the_ceiling_and_recovers
  Scenario: the limiter holds the ceiling and recovers
    Given the output limiter fed a sine 6 dB over the ceiling, then one 6 dB under
    Then nothing leaves above the ceiling, the meter reads the 6 dB it took, and the quiet signal passes untouched once the gain has recovered

  # dsp_tests :: the_biquads_shape_the_spectrum_as_designed
  Scenario: the biquads shape the spectrum as designed
    Given an RBJ peaking filter of +8 dB at 1800 Hz, Q 1.5
    Then a sine at the centre gains 8 dB and one far below it passes untouched
    Given a Butterworth low-pass (Q 0.707) at 700 Hz
    Then it is 3 dB down at its corner, flat an octave and more below, and falls at least 12 dB per octave above, as the bilinear Butterworth gives

  # dsp_tests :: a_delay_line_delays_by_its_fractional_length
  Scenario: a delay line delays by its fractional length
    Given a delay line holding an impulse, read 10.25 samples back
    Then the impulse comes out split between samples 10 and 11 as 0.75 and 0.25: its centre at 10.25
    Then a whole-sample read is exact, and a read past the capacity is held at the capacity

  # dsp_tests :: a_contact_delivers_its_impulse_over_its_duration
  Scenario: a contact delivers its impulse over its duration
    Given a contact of impulse 2 over 1 ms at 48 kHz
    Then the force is a half-sine 48 samples long whose area is the impulse

  # dsp_tests :: a_mode_rings_at_its_frequency_for_its_decay_time
  Scenario: a mode rings at its frequency for its decay time
    Given one mode at 440 Hz with a T60 of 0.5 s, struck once
    Then it rings at 440 Hz and is 60 dB down after half a second

  # dsp_tests :: the_converters_quantise_as_their_storage_does
  Scenario: the converters quantise as their storage does
    Given the SP-1200's 12-bit linear store and the LinnDrum's 8-bit companded one
    Then the SP-1200's step is 1/2047 everywhere
    Then the LinnDrum's steps are fine near zero and coarse near full scale (mu-law)

  # dsp_tests :: the_listen_mic_compressor_squashes_hard_and_fast
  Scenario: the listen mic compressor squashes hard and fast
    Given the compressor at full crush, fed a -6 dBFS tone then a -40 dBFS one
    Then 34 dB of input difference comes out as less than 8 dB (above a -40 dBFS threshold at 20 : 1)

