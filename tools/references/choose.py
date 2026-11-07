#!/usr/bin/env python3
"""Chooses BONKR's reference recordings from references/candidates.tsv (the survey of freely
available recordings, see references/SOURCES.md) and writes references/manifest.tsv: for every
BONKR sound with a recording of the real thing, up to four of them, from the most reliable
sources first. Run it (tools/references/choose.py) again after editing the rules below; then fetch.py --pin."""
import csv, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REFS = os.path.join(HERE, '..', '..', 'references')

# (category regex, sound regex) -> BONKR sound name (the enum's name without the k).
RULES = [
    ('acoustic kit', r'^kick', 'KickStudio'),
    ('acoustic kit', r'^snare$', 'SnareMaple'),
    ('acoustic kit', r'^rimshot', 'SnareRimshot'),
    ('acoustic kit', r'^cross-stick', 'SnareCrossStick'),
    ('acoustic kit', r'^rack tom', 'TomHigh'),
    ('acoustic kit', r'^floor tom', 'TomFloor'),
    ('acoustic kit', r'^hi-hat closed$', 'HatClosed'),
    ('acoustic kit', r'^hi-hat pedal$', 'HatPedal'),
    ('acoustic kit', r'^hi-hat open', 'HatOpen'),
    ('acoustic kit', r'^crash$', 'Crash18'),
    ('acoustic kit', r'^ride$', 'Ride'),
    ('acoustic kit', r'^ride \(bell\)', 'RideBell'),
    ('acoustic kit', r'^china', 'China'),
    ('acoustic kit', r'^splash', 'Splash'),
    ('orchestral', r'^timpani', 'Timpani'),
    ('orchestral', r'^concert bass drum', 'BassDrum'),
    ('orchestral', r'^tam-tam|^gong / tam-tam', 'TamTam'),
    ('orchestral', r'^triangle', 'Triangle'),
    ('orchestral', r'^clash cymbals', 'ClashCymbals'),
    ('orchestral', r'^tambourine', 'TambourineHit'),
    ('orchestral', r'^castanets', 'Castanets'),
    ('orchestral', r'^sleigh bells', 'SleighBells'),
    ('orchestral', r'^woodblock$', 'WoodBlock'),
    ('orchestral', r'^ratchet', 'Ratchet'),
    ('orchestral', r'^vibraslap', 'Vibraslap'),
    ('orchestral', r'slapstick', 'Slapstick'),
    ('world', r'^bongos', 'BongoHigh'),
    ('world', r'^congas$', 'CongaOpen'),
    ('world', r'^congas \(tumba\)', 'Tumba'),
    ('world', r'^djembe', 'DjembeTone'),
    ('world', r'^agogo', 'AgogoHigh'),
    ('world', r'^cowbell', 'Cowbell'),
    ('world', r'^claves', 'Claves'),
    ('world', r'^guiro', 'GuiroShort'),
    ('world', r'^maracas', 'Maracas'),
    ('world', r'^cabasa', 'Cabasa'),
    ('world', r'^shaker', 'Shaker'),
    ('world', r'^hand clap', 'Clap'),
    ('world', r'^finger snap', 'Snap'),
    ('world', r'^taiko|^large ethnic drum', 'Nagado'),
    ('environmental', r'^telephone bell', 'Telephone'),
    ('environmental', r'^washing machine', 'WashingMachine'),
    ('environmental', r'^jet engine', 'JetFlyby'),
    ('environmental', r'^vuvuzela', 'Vuvuzela'),
    ('environmental', r'^fireworks', 'FireworkBurst'),
    ('environmental', r'^cannon', 'Cannon'),
    ('environmental', r'^artillery', 'Howitzer'),
    ('environmental', r'^gunshot', 'Rifle'),
    ('environmental', r'^explosion', 'Explosion'),
    ('environmental', r'^thunder', 'Thunder'),
    ('environmental', r'^trash can', 'TrashCan'),
    ('environmental', r'^glass breaking', 'GlassSmash'),
    # The drum machines: recordings of the machines themselves, used for measurement only.
    ('drum machine', r'^RolandTR808 kick', 'TR808Kick'),
    ('drum machine', r'^RolandTR808 snare', 'TR808Snare'),
    ('drum machine', r'^RolandTR808 clap', 'TR808Clap'),
    ('drum machine', r'^RolandTR808 closed hat', 'TR808HatClosed'),
    ('drum machine', r'^RolandTR808 open hat', 'TR808HatOpen'),
    ('drum machine', r'^RolandTR808 cymbal', 'TR808Cymbal'),
    ('drum machine', r'^RolandTR808 cowbell|^Roland TR-08 cowbell', 'TR808Cowbell'),
    ('drum machine', r'^RolandTR808 (high|low) tom', 'TR808Tom'),
    ('drum machine', r'^RolandTR808 rimshot', 'TR808Rim'),
    ('drum machine', r'^RolandTR808 maracas', 'TR808Maracas'),
    ('drum machine', r'^RolandTR808 perc CL', 'TR808Claves'),
    ('drum machine', r'^RolandTR909 (kick|BD)', 'TR909Kick'),
    ('drum machine', r'^RolandTR909 (snare|SD)', 'TR909Snare'),
    ('drum machine', r'^RolandTR909 (clap|CLP)', 'TR909Clap'),
    ('drum machine', r'^RolandTR909 (closed hat|CHH)', 'TR909HatClosed'),
    ('drum machine', r'^RolandTR909 (open hat|OHH)', 'TR909HatOpen'),
    ('drum machine', r'^RolandTR909 (cymbal/crash|CSH)', 'TR909Crash'),
    ('drum machine', r'^RolandTR909 (ride|RD)', 'TR909Ride'),
    ('drum machine', r'^RolandTR909 (high tom|low tom|LT)', 'TR909Tom'),
    ('drum machine', r'^RolandTR909 (RIM|rimshot)', 'TR909Rim'),
]

# Sources in order of trust: dry, isolated, losslessly stored first.
HOST_ORDER = ['theremin.music.uiowa.edu', 'raw.githubusercontent.com/sgossner', 'drumgizmo.org', 'philharmonia', 'cdn.freesound.org',
              'raw.githubusercontent.com', 'machines.hyperreal.org', 'upload.wikimedia.org', 'archive.org']
PER_SOUND = 4


def rank(url):
    for i, h in enumerate(HOST_ORDER):
        if h in url: return i
    return len(HOST_ORDER)


def main():
    rows = list(csv.DictReader(open(os.path.join(REFS, 'candidates.tsv')), delimiter='\t'))
    chosen = {}
    for r in rows:
        url = r['url']
        if not url.startswith('http') or url.endswith('.zip') or 'all instruments' in r['sound']: continue
        for cat, pattern, sound in RULES:
            if re.search(cat, r['category']) and re.search(pattern, r['sound']):
                chosen.setdefault(sound, []).append(r)
                break
    with open(os.path.join(REFS, 'manifest.tsv'), 'w') as out:
        out.write('# BONKR reference recordings, chosen by tools/references/choose.py from candidates.tsv. Licences and\n')
        out.write('# sources in SOURCES.md. The audio is fetched into ~/.cache/bonkr-references and never committed;\n')
        out.write('# only the features measured from it are. The drum-machine rows are measurement only.\n')
        out.write('# sound\turl\tlicense\tsource\n')
        for sound in sorted(chosen):
            picks = sorted(chosen[sound], key=lambda r: rank(r['url']))[:PER_SOUND]
            for r in picks:
                out.write('\t'.join([sound, r['url'], r['license'].replace('\t', ' '), r['author/source'].replace('\t', ' ')]) + '\n')
    print(f'{sum(min(len(v), PER_SOUND) for v in chosen.values())} recordings for {len(chosen)} sounds', file=sys.stderr)


if __name__ == '__main__':
    main()
