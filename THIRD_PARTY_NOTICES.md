# Third-party notices

## CLAP

The CLAP headers (free-audio/clap, tag 1.2.7) are fetched at configure time. MIT License,
Copyright (c) 2021 Alexandre BIQUE.

## Editor engine and test harness

The platform-neutral editor (`src/ui/editor.hpp`, the canvases and window backends), the CLAP
shell, the CLAP test harness, the audio measurements, the temperaments and the room and air model
(`src/dsp/air.hpp`) are adapted from the same author's SCHRODR
(https://github.com/andrewfader/schrodr), FUZZBOI (https://github.com/andrewfader/fuzzboi) and
YANES (https://github.com/andrewfader/yanes) projects, under the same source-available licence as
this repository.

## Published data

The physical models' forms and constants are taken from the published literature named in each
source file's header comment and in docs/PHYSICS.md (Rayleigh; Leissa; Fletcher and Rossing;
Rossing; Hunt and Crossley; Avanzini and Rocchesso; Cook's PhISEM; Peltola et al.; Kinney and
Graham; Whitham; Few; McIntyre, Schumacher and Woodhouse; ISO 9613-1; Sabine; Jot; Allen and
Berkley); none of it is code.

## Reference recordings

BONKR is measured against recordings of the real instruments (references/README.md,
references/SOURCES.md). They are downloaded by `tools/references/fetch.py` into a local cache and
never redistributed; `references/features.tsv` holds only numbers measured from them. Sources:
VSCO-2 Community Edition (Versilian Studios, CC0), the University of Iowa Electronic Music
Studios' Musical Instrument Samples (Lawrence Fritts, "without restrictions"), the Philharmonia
Orchestra's sound samples (free to use, not to be redistributed as they are), the DrumGizmo
DRSKit and CrocellKit (CC-BY 4.0: DrumGizmo, Jes Eiler/DRSDrums, Crocell), and individually
credited CC0 and CC-BY recordings from Freesound, Wikimedia Commons and archive.org listed per
file in references/manifest.tsv. The drum-machine recordings (TR-808, TR-909) carry no stated
licence and are used for measurement only.
