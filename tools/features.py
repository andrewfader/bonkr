#!/usr/bin/env python3
"""Writes features/*.feature from the executable scenarios in tests/*.cpp: every SCENARIO with its
GIVEN/WHEN/THEN steps, under the test binary and scenario function that prove it. Run after
changing a scenario; the feature files are the readable index, the tests are the truth."""
import os, re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
FEATURES = [
    ('dsp_building_blocks', 'The DSP building blocks', ['dsp_tests']),
    ('physics', 'The physics every sound is built from', ['physics_tests']),
    ('kit', 'Kits, chromatic play, chokes, restrikes, swing and note repeat', ['kit_tests']),
    ('air', 'The room and the air', ['air_tests']),
    ('engine', 'The engine: strokes, levels, presets, limiter, converter, compressor and gate', ['engine_tests']),
    ('references', 'BONKR against recordings of the real instruments', ['reference_tests']),
    ('clap_plugin', 'The plug-in through the CLAP ABI', ['plugin_smoke', 'note_tests', 'state_tests']),
    ('editor', 'The editor', ['ui_tests', 'frontend_tests']),
]
STEP = re.compile(r'\b(GIVEN|WHEN|THEN)\("((?:[^"\\]|\\.)*)"')


def scenarios(binary):
    src = open(os.path.join(ROOT, 'tests', binary + '.cpp')).read()
    parts = re.split(r'SCENARIO\((\w+)\)', src)
    for i in range(1, len(parts), 2):
        name, body = parts[i], parts[i + 1]
        steps = [(k.capitalize(), t.replace('\\"', '"')) for k, t in STEP.findall(body)]
        yield name, steps


for slug, title, binaries in FEATURES:
    with open(os.path.join(ROOT, 'features', slug + '.feature'), 'w') as out:
        out.write(f'Feature: {title}\n\n')
        for b in binaries:
            for name, steps in scenarios(b):
                out.write(f'  # {b} :: {name}\n  Scenario: {name.replace("_", " ")}\n')
                for verb, text in steps:
                    out.write(f'    {verb} {text}\n')
                out.write('\n')
with open(os.path.join(ROOT, 'features', 'advertised_parameters.feature'), 'w') as out:
    out.write('Feature: Every parameter does what it advertises\n\n')
    out.write('  # advertised_tests :: proofs() (one entry per parameter ID; the suite fails if any is missing)\n')
    out.write('  Scenario Outline: a parameter proved from audio in its own unit\n')
    out.write('    Given the musical context of the parameter\'s proof (a kit or one sound, settings, keys)\n')
    out.write('    When it is rendered through the built plug-in at the two values of the proof\n')
    out.write('    Then the measurement in the parameter\'s unit moves as the parameter advertises\n')
print('features written')
