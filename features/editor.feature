Feature: The editor

  # ui_tests :: coordinates_scale_to_any_window_and_back
  Scenario: coordinates scale to any window and back
    Given several window sizes from half to double the design
    Then design coordinates survive the trip and type never drops below legibility

  # ui_tests :: every_parameter_is_placed_exactly_once
  Scenario: every parameter is placed exactly once
    Given the page table
    Then each parameter is reachable from exactly one place

  # ui_tests :: widgets_suit_their_parameters
  Scenario: widgets suit their parameters
    Given every control on every page
    Then toggles are two-valued, segments short lists, menus long lists

  # ui_tests :: every_page_fits_without_overlap
  Scenario: every page fits without overlap
    Given each page with every card open
    Then everything sits between the tabs and the footer, nothing overlaps, every control is visible

  # ui_tests :: tabs_switch_pages_with_an_animation_that_settles
  Scenario: tabs switch pages with an animation that settles
    Given a click on the fourth tab
    Then the page changes, animates, and comes to rest without any parameter edit

  # ui_tests :: sections_collapse_and_the_column_reflows
  Scenario: sections collapse and the column reflows
    Given the kit page's Sound card, clicked closed
    Then the card shrinks to its header and the state survives a page change

  # ui_tests :: knobs_drag_fine_tune_reset_and_wheel
  Scenario: knobs drag fine tune reset and wheel
    Given the master knob on the kit page
    When it is dragged up by the full drag range
    Then it reaches its maximum inside one gesture
    When Shift is held
    Then the same movement is a tenth as large
    When it is right-clicked, Ctrl-clicked and double-clicked
    Then each resets to the default
    When the wheel turns over it
    Then the value nudges by the wheel fraction
    Given the tune knob
    Then small movements accumulate into a continuous retuning

  # ui_tests :: toggles_segments_and_menus_choose_options
  Scenario: toggles segments and menus choose options
    Given the layout segments on the kit page and the kit menu
    Then the menu opens without an edit and lists every kit
    When the second item is clicked
    Then the kit changes inside one gesture and the menu closes
    When the sound menu, the longest option list, is opened
    Then all of its sounds are listed inside the editor
    When the header preset menu is opened from another page and the wheel turns
    Given the hi-hat choke toggle on the machine page
    Then it flips

  # ui_tests :: lost_releases_and_pointer_leave_never_leak_gestures
  Scenario: lost releases and pointer leave never leak gestures
    Given a drag whose release is lost
    Then the next press closes the old gesture before opening a new one

  # ui_tests :: strip_clicks_reach_the_host_and_the_size_button_works
  Scenario: strip clicks reach the host and the size button works
    Given the room page's artwork strip
    Then the click reaches the host with its page and button
    Given a host that can resize
    Then clicks step the size down and wrap, right-click restores 100%, none of it is an edit

  # ui_tests :: every_page_draws_inside_the_editor_with_every_label
  Scenario: every page draws inside the editor with every label
    Given each page, settled, collapsed, hovered, and at extreme values
    Then every label is drawn and nothing is drawn outside the design space

  # ui_tests :: short_names_fit_a_cell_and_ellipsis_respects_utf8
  Scenario: short names fit a cell and ellipsis respects utf8
    Given every short name and a multibyte string
    Then names are at most 14 characters and the ellipsis cuts between characters

  # ui_tests :: presets_have_unique_names_that_fit_the_header
  Scenario: presets have unique names that fit the header
    Given the preset table
    Then names are unique and short, and every setting is inside its parameter's range

  # ui_tests :: scope_holds_still_and_amplifies_quiet_output
  Scenario: scope holds still and amplifies quiet output
    Given a 220 Hz sine at -24 dBFS, captured at two unrelated moments
    Then it is amplified by the 23.1 dB that brings its peak to 0.9 of the frame, and says so
    Then the trace starts on a rising zero crossing, so both captures draw the same picture
    Then the window is 20 ms: 4.4 cycles of 220 Hz
    Given silence, hiss at -90 dBFS, a quiet sine at -40 dBFS and a full-scale sine
    Then silence and hiss are not blown up, the gain stops at +24 dB, and loud output is never turned down

  # frontend_tests :: the_host_describes_every_parameter_with_help_and_labels
  Scenario: the host describes every parameter with help and labels
    Given the plug-in's editor host
    Then every parameter has matching info, a label, a short label, help, and formats its default

  # frontend_tests :: relevance_follows_the_layout_and_the_sound
  Scenario: relevance follows the layout and the sound
    Given the default kit layout
    Then the kit is live, the single sound dimmed, the snares live, and dependent controls follow their switches
    When one sound is played across the keyboard: the timpani
    Then the sound is live, the kit, the choke and the snares dimmed
    When the sound is a snare
    Then the snares light up
    Then no sound dims the whole editor

  # frontend_tests :: editor_gestures_reach_the_host_as_clap_events
  Scenario: editor gestures reach the host as clap events
    Given a drag of the master level through the editor host
    Then the plug-in holds the value, asked for a flush, and marked the state dirty
    When the host flushes
    Then it receives begin, two values and end, in order
    When the preset 'Orchestral percussion' is chosen through the editor
    Then the whole preset is applied and the host is asked to rescan on its main thread
    When the queue overflows
    Then events are dropped rather than the editor blocking, and the value is still right

  # frontend_tests :: page_artwork_draws_from_the_running_plugin
  Scenario: page artwork draws from the running plugin
    Given a plug-in that has rendered a note
    Then every page's strip and the meters draw inside the editor, the keyboard strip names the kit and the struck sound, and lights its key

  # frontend_tests :: the_plugins_scope_follows_its_output
  Scenario: the plugins scope follows its output
    Given a plug-in that has rendered 40 blocks of a held note
    Then the scope ring holds the last 4096 samples at full rate, exactly what the plug-in output

  # frontend_tests :: the_whole_editor_draws_and_ticks_without_a_window
  Scenario: the whole editor draws and ticks without a window
    Given the plug-in's editor, created on demand
    Then it draws every page from live plug-in state and takes pointer input
    When the editor asks for a size the host cannot grant
    Then the window size stays as it was

