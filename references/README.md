# Reference recordings: the quality gate

BONKR is compared with recordings of the real instruments, so "it sounds like a snare drum" is a
measurement and not an opinion. Every sound with a recording of the real thing (72 of BONKR's
121, from 211 recordings) must pass the gate below, and `reference_tests` (in `ctest`) holds it.

**The recordings** are chosen from a survey of freely available ones (`candidates.tsv`, sources
and licences in `SOURCES.md`) by `tools/references/choose.py`, which keeps up to four per sound
from the most trustworthy sources first: the University of Iowa's anechoic recordings, VSCO-2 CE
(CC0), the DrumGizmo multitrack kits (CC-BY 4.0, every microphone summed), the Philharmonia's
samples, then individually licensed CC0 and CC-BY recordings. The drum machines are recordings of
the machines themselves, with no stated licence: measurement only.

| File | What | Committed |
|---|---|---|
| `candidates.tsv`, `SOURCES.md` | the survey | yes |
| `manifest.tsv` | the chosen recordings, by BONKR sound | yes |
| `checksums.sha256` | their SHA-256, pinned | yes |
| `features.tsv` | the features of every recording | yes: numbers, small |
| `baseline.tsv` | BONKR's score per sound | yes |
| the recordings | about 1.4 GB (raw and converted) | no: `~/.cache/bonkr-references` |

**The features** (`tests/timbre.hpp`), from the onset over a window (0.6 s; 2 s for what rings
long or arrives late): the octave-band profile from 31.5 Hz to 16 kHz; the spectral centroid; the
time to fall 20 dB from the peak; the attack; the spectral flatness (bins floored 60 dB under the
strongest, so a recording's noise floor does not count).

**The score** (`tests/reference_match.hpp`) sums, in units of a plainly audible difference: the
profile's RMS difference after removing the level (per 6 dB), the brightness (per half octave),
the ring time (per factor of two), the flatness (per 6 dB). BONKR strikes each sound dry, with
the default settings, at velocity 0.8, and is scored against each of its recordings.

**The gate.** Real recordings of one instrument differ among themselves (another drum, room,
microphone, player). A sound's spread is the mean score between its recordings. BONKR must score
no worse than twice that spread, or 4, whichever is larger; with a single recording, no worse
than 5. The gate depends only on the recordings. On top of the gate, `baseline.tsv` is a
ratchet: no sound may score worse than its baseline.

    build-release/bonkr-reference match              # every sound: score, spread, gate, baseline
    build-release/bonkr-reference match Snare --features   # BONKR's features beside each recording's
    build-release/bonkr-reference match --update-baseline

To change the recordings: edit the rules in `tools/references/choose.py`, then

    tools/references/choose.py                       # rewrite manifest.tsv
    tools/references/fetch.py --pin                  # download into the cache, pin the checksums
    build-release/bonkr-reference extract            # rewrite features.tsv
    build-release/bonkr-reference match --update-baseline

`fetch.py` without `--pin` verifies the downloads against the pins. Three Iowa and Philharmonia
woodblock files could not be decoded, so the woodblock has no reference yet; nor do the sounds no
free recording was found for (see `SOURCES.md`).
