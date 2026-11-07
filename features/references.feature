Feature: BONKR against recordings of the real instruments

  # reference_tests :: every_referenced_sound_passes_the_gate_and_keeps_its_score
  Scenario: every referenced sound passes the gate and keeps its score
    Given the features of the reference recordings and BONKR's last scores
    When every referenced sound is struck through the engine and measured the same way
    Then each is within its gate and no worse than its baseline

