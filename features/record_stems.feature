Feature: The kits fitted to records, against the drum stems of those records

  # stem_gate_tests :: the_basement_four_track_kit_sounds_like_the_drums_of_basement_four_track_records
  Scenario: the basement four track kit sounds like the drums of basement four track records
    Given the "Four-track basement kit" preset: a pair of microphones close on the kit in a small, damped room, onto a four-track cassette at 9.5 cm/s whose head has lost touch with the tape
    When a plain rock groove is played on it for 30 s and measured as a separated drum stem
    Then its roll-off, share below 250 Hz, centroid, crest and L/R correlation each lie inside the quartiles of the drums of 14 basement four-track records

  # stem_gate_tests :: the_home_studio_psych_kit_sounds_like_the_drums_of_home_studio_psych_records
  Scenario: the home studio psych kit sounds like the drums of home studio psych records
    Given the "Home-studio psych kit" preset: spot microphones and a pair in a studio-sized room, squashed, onto a worn 19 cm/s reel
    When a plain rock groove is played on it for 30 s and measured as a separated drum stem
    Then its roll-off, share below 250 Hz, centroid, crest and L/R correlation each lie inside the quartiles of the drums of 12 home-studio psych records

  # stem_gate_tests :: the_taped_psych_rock_kit_is_thinner_than_either
  Scenario: the taped psych rock kit is thinner than either
    Given the "Psych rock kit, taped" preset, fitted to studio psych records rather than these
    When the same groove is played on it
    Then its share below 250 Hz lies under both groups' quartiles: the gate tells the kits apart

