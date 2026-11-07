# Reference recording sources (survey, 2026-11-07)

Candidates are listed in `candidates.tsv`: one row per file, with its URL, licence, author and the
HTTP check made on 2026-11-07. Nothing is downloaded or committed. As with HONKR, a fetch script
should download files into `~/.cache/bonkr-references`, pin their SHA-256, and commit only the
features measured from them.

**Verification column.** For a direct URL, it holds the HTTP status and Content-Length of a curl
HEAD (`-L`). For a zip member, it holds the zip's HEAD plus the member's size from the zip's central directory. From this machine, the
`upload.wikimedia.org` and archive.org data-node (`dn*.archive.org`) connections timed out throughout
the survey: the redirect from archive.org returns 302, then the TCP connect fails. This looks like a local
firewall or router block. Those rows are verified only through the Commons API and archive.org
metadata API (the file exists, at the size given). Re-check them with a direct GET once those hosts are allowed.

**URL convention.** A URL of the form `https://.../Kit.zip!Dir/file.wav` points to a member of a
remote zip. DrumGizmo kits are 0.4 to 5.6 GB each, so never download a whole kit. Read the zip's
central directory with HTTP Range requests (the last ~100 KB), then range-GET only the member's
local header and data. Both drumgizmo.org and S3 serve `Accept-Ranges`. This was tested during the survey: Python's
`zipfile.ZipFile` over a small seekable file object that turns `read()` into Range GETs lists a kit
with 20-110 KB of requests and extracts one member for about its compressed size. The Philharmonia `Percussion.zip` is only 7.2 MB,
so downloading it whole is fine.

## Sources, in order of preference

### 1. VSCO-2 Community Edition: CC0
`https://github.com/sgossner/VSCO-2-CE` (raw files at `raw.githubusercontent.com/sgossner/VSCO-2-CE/master/...`)
- Licence (`LICENSE`): "CC0 1.0 Universal". `Readme.txt` adds: "You are permitted to use these samples for ANY
  purpose. We ask that you do not sell the samples directly ... Please provide credit to Versilian Studios"
  (a request, not a condition, under CC0).
- Credit: Versilian Studios; recorded by Sam Gossner & Simon Dalzell; cutting by Elan Hickler/Soundemote.
- Contents: timpani (5 drums x 3-4 dynamic layers x 2 round-robins), and the "VSCO 1 Percussion" legacy
  set. That set has a concert bass drum (pp-fff, muted, rolls), snares (concert, old, marching), tenor drums, bongos,
  congas ("congo" folder) plus a tumba, a giant ethnic drum, clash and suspended cymbals, a gong (17 dynamics),
  triangles, tambourines, agogo, cowbell, cabasa, shaker, claves, guiro, maracas, ratchet,
  sleigh bells, slapstick, vibraslap and woodblock ("wood_click"). "Miscellania Raw" adds glass breaks and metal hits.
- Format: WAV, 44.1 kHz stereo, 24-bit (VSCO1) or 16-bit (`*_Sum.wav`). One hit per file. Small room, fairly
  close and dry.

### 2. University of Iowa MIS: "without restrictions"
`https://theremin.music.uiowa.edu/MIS.html`. The percussion pages are MIScymbals, MISgongtamtams,
MIShandpercussion and MIStambourines, plus the post-2012 versions under `MIS-Pitches-2012/`.
- Terms quote: "Since 1997, these recordings have been freely available on this website and may be
  downloaded and used for any projects, without restrictions." Credit (customary, not required):
  Lawrence Fritts, University of Iowa Electronic Music Studios.
- Recorded in an anechoic chamber, the best "dry" references here. Coverage: hi-hat (stick normal/bell/shoulder,
  foot close, foot splash), crash 13/17/18/20", china 16/19/20", 21" ride, splash (stick and mallet,
  bell/normal/shoulder, choke, bowed, rolls), orchestral crash pairs, tam-tams 22/28/40", a wind gong, Thai
  gongs C4-C5, woodblocks, granite blocks, claves, guiro, castanets, triangles and tambourines, all at pp/mf/ff.
- The files I probed (pre-2012 percussion) are AIFF, 24-bit stereo 44.1 kHz, one hit with leading silence. The site
  text says pre-2011 recordings are 16-bit mono, so measure each file rather than trusting the description.
  Post-2012 `*.stereo.aif` files are the L/R mics of a Decca tree.
- No drums: the MIS has no kick, snare, toms, timpani or bass drum.

### 3. Philharmonia Orchestra sound samples: free, but no redistribution "as is"
`https://philharmonia.co.uk/resources/sound-samples/`, still live. The percussion pack is
`https://philharmonia-assets.s3-eu-west-1.amazonaws.com/uploads/2020/02/12112005/Percussion.zip` (7,150,856 B).
- Terms quote: "You are free to use these samples as you wish, including releasing them as part of a
  commercial work. The only restriction is that they must not be sold or made available 'as is' (i.e. as
  samples or as a sampler instrument)." This fits the HONKR model (cache only, commit only measured features).
  It is not an open licence, so never commit the audio.
- MP3, 44.1 kHz mono, lossy and short (tails can be clipped by the file length). There are 39 instruments,
  including bass drum, snare, tom-toms, tam-tam, Thai gong, triangle, castanets, tambourine, clash/Chinese/
  suspended cymbals, woodblock, cowbell, agogo, cabasa, guiro, ratchet, sleigh bells, djembe, surdo, whip,
  vibraslap and washboard. File names encode length, dynamic and articulation.

### 4. DrumGizmo kits: CC-BY-4.0
Kit pages are at `https://drumgizmo.org/wiki/doku.php?id=kits`. The zips are DRSKit 2.1 (2.8 GB), CrocellKit 1.1
(5.6 GB), MuldjordKit 3 (2.5 GB), Aasimonster 2.1 (2.4 GB), ShittyKit 1.2 (370 MB) and Sommerhack 2016 (12 MB).
- Licence quote (DRSKit; the other kits are the same): "released under the Creative Commons Attribution 4.0 International
  License. When the samples are used in a composition, any context in which other credits (e.g. to
  performers) are presented must include an attribution." Attribute each kit by name and author: DrumGizmo team
  with Jes Eiler/DRSDrums (DRSKit), Lars Muldjord (MuldjordKit), and so on.
- Each hit is one multichannel WAV: DRSKit has 13 channels (close mics, 2 overheads, 2 room mics) at 32-bit float
  44.1 kHz. This is the best acoustic-kit reference: close mics give a dry signal, and the room mics show how much
  room is in the recording. Samples are numbered in order of ascending power, which is listed in each
  `<instrument>.xml`. Coverage: kick, snare, rimshot (CrocellKit, Aasimonster), cross-stick (ShittyKit `xstick`,
  probably DRSKit `Snare_rim`), toms, hi-hat closed/open/semi/pedal, crash, ride and bell, china, splash. The
  Sommerhack kit has a hand clap, finger snaps, glass, cup and mug.

### 5. Wikimedia Commons: per-file licences
Found with the API (`action=query&generator=search ... filetype:audio`). The licence and author recorded in each
row are Commons' `extmetadata`, quoted verbatim. Licences are a mix of PD, CC0, CC BY and CC BY-SA. A CC BY-SA
file is fine for measurement, but derived audio would have to be share-alike. Highlights: a CC BY-SA 3.0
Wikipedia drum-kit demo set (bass drum, snare, rim, toms, hats, crash, ride; author not machine-readable),
thunder, washing machine (PD, ezwa), fireworks (PD), vuvuzela, an airliner flyover, a WE Model 500 ring and
glass breaking. One file, "9 mm gunshot-mike-koenig-123.wav", looks like a SoundBible upload relabelled by its
uploader, so its provenance is doubtful.

### 6. Freesound: CC0 or CC BY only
The search pages (`freesound.org/search/?q=...&f=license:"Creative Commons 0"`) and the preview files
`https://cdn.freesound.org/previews/<id//1000>/<id>_<userid>-hq.mp3` need no login or API key. The
original files do need a login. Previews are about 128 kbps MP3, adequate for envelopes and spectra and poor
for fine high-frequency detail. Each row records the sound id, the uploader and the licence. CC0 needs nothing.
"Attribution" sounds are CC BY 3.0 or 4.0 (check the sound page) and must be credited as "<title> by <user>,
freesound.org/s/<id>, CC BY".

### 7. archive.org: USC / Sunset Editorial sound-effects library ("SSE Library"): CC0 as published
There are 64 items, `https://archive.org/details/SSE_Library_<CATEGORY>`, with FLAC, MP3 and spectrograms. Each
item's `licenseurl` is CC0 1.0, and each description reads "Sound effects from the USC Cinema and/or the Sunset
Editorial Collection" (uploaded by archive.org staff). Many of the same files are also on Freesound as CC0 (user
craigsmith). These are vintage film-library effects (optical/tape era, often reverberant and edited), but they
are the only free source found for a crank-magneto telephone bell, naval and field cannon, mortars, and dry
pistol and rifle shots. The CC0 rests on archive.org's labelling: there is no deed from the original
studios.

### Drum machines: no truly free-licensed source found
- **geikha/tidal-drum-machines** (GitHub, raw URLs). The repo has no licence file and the README does not say
  where the samples came from. It covers TR-808 (the BD0000-BD7575 naming of the widely circulated "Michael
  Fischer" 808 set), TR-909, LinnDrum, Oberheim DMX, E-mu SP-12/SP-1200, Akai MPC60 and Alesis HR-16. Treat the
  licence as *none*. The files are good as timbre references for measurement, but do not redistribute them.
- **Hyperreal Music Machines, TR-909** (Jason Baker / Rob Roy Recordings, 1995) `TR909.TXT`: "Please feel free
  to copy or distribute the set in any way. Please DO NOT change samples, add samples, delete samples, or
  modify this text file in any way." This allows free copying but grants nothing explicit beyond that.
- **Hyperreal, LinnDrum** `linndrum.txt`: "44.1 Khz/16 Bit samples of the LinnDrum ... brought to you by
  majortom@muc.de". No licence text.
- **archive.org "LinnDrum and Roland 808 KnobCon 2025 Sample Drop"** (Paisley Computer): new recordings of real
  units, no licence field. It is useful as a second, independent unit.
- I did not find Michael Fischer's original 808 readme or its licence text online, so I am not quoting it.
- I found no DMX, SP-1200, MPC60 or HR-16 sets outside tidal-drum-machines.

## Gaps
- **Taiko**: there are no isolated, labelled o-daiko, nagado-daiko or shime-daiko hits under a free licence. The
  closest are a few CC BY Freesound uploads of unspecified taiko types, one CC0 Commons performance excerpt, and VSCO's "giant ethnic drum" as a
  stand-in.
- **1812-style black-powder cannon**: there is no isolated field or salute cannon. The nearest are USC "Cannon shot",
  "6 inch naval cannon" and "Black powder explosion", and a distant ZiS-3 salute on Freesound.
- **Trash can**: there are only casual Freesound recordings and a USC "metal barrel hit with wooden hammer".
- **Drum machines**: there are references but no clean licence (see above).
