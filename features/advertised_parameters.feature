Feature: Every parameter does what it advertises

  # advertised_tests :: proofs() (one entry per parameter ID; the suite fails if any is missing)
  Scenario Outline: a parameter proved from audio in its own unit
    Given the musical context of the parameter's proof (a kit or one sound, settings, keys)
    When it is rendered through the built plug-in at the two values of the proof
    Then the measurement in the parameter's unit moves as the parameter advertises
