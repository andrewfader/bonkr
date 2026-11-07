# The quality bar

Two bars, both enforced by the test suite.

## Every preset, judged from audio

`build/bonkr-preset-audit <dir>` renders each preset with the same phrase and prints the
measurements below; `engine_tests` (`every_preset_meets_the_quality_bar`) enforces them with the
same phrase (`tests/audit_phrase.hpp`). For a kit the phrase is a bar of groove on the GM map
(kick, snare, closed, pedal and open hats), a four-tom fill and a crash on the downbeat; for one
sound across the keyboard, a held C4 with the E and G above struck after it, then three short
strokes higher up. Then the preset's own tail.

| Measurement | Bar | Why |
|---|---|---|
| Loudness (the loudest 400 ms, BS.1770 momentary, mono sum) | -18 LUFS within 2 dB | Presets sit at one level under the fingers |
| Impulsive presets | a gunshot or a cannon reaches the limiter's allowance (5.5 dB) before -18 LUFS; it is then as loud as it can be without squashing, and is not flagged quiet | A peak a hundred times its average cannot be made loud by level alone |
| Limiter | no more than 9 dB of reduction anywhere; the clip flag clear | A preset that needs more is being squashed |
| The first stroke | audible (above -60 dBFS) within 250 ms | |
| DC | below -40 dBFS | |
| Idle output before the first note | below -55 dBFS RMS | |
| Tail | within the engine's advertised tail; below -50 dBFS once it has passed | Hosts stop calling `process` after the tail |
| Finite | every sample | |
| Character | the spectrogram shows the thing named | Numbers cannot see this; a person looks |

`bonkr-preset-audit --fit` finds each preset's Master for this bar; `--levels` prints every
sound's calibrated trim.

## Every sound, level

Every sound is calibrated at activation (`levels::trims` in `src/engine.hpp`): one stroke at
velocity 0.8 with the default settings peaks at -9 dBFS, measured from the model itself at the
host's sample rate. `engine_tests` holds it (`every_sound_is_calibrated_to_the_same_peak`).

## Every referenced sound, against recordings of the real thing

72 of the 121 sounds have recordings of the real instrument or machine (211 of them). Each must
sit no further from its recordings than twice the recordings' own spread (or 4; 5 with one
recording), on a score of profile, brightness, ring time and noisiness, and may not fall behind
its baseline. See [references/README.md](../references/README.md).

Where the recordings disagree among themselves the gate is wider, so a sound that passes is
within the range real instruments of that kind span, not a match to one recording. The scores
are in `references/baseline.tsv`; most acoustic and machine sounds sit at or under the spread of
their own recordings.

## How a round goes

1. Build, run `bonkr-reference match` and the audit, and read both.
2. Fix the model before the preset: a preset that only sounds right at one level is a model that
   is wrong at the others.
3. Refit the presets (`--fit`), recheck the references, refresh the baseline only for an
   improvement or a deliberate correction.
4. Run the complete suite.
