#!/usr/bin/env python3
"""Expose the shipped JSON recipes to the SDK-independent C++ regression tests."""
import json
import math
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
config = json.loads((root / 'res/programs/blunch-random.json').read_text())
assert config['schemaVersion'] == 1

def literal(text):
    assert isinstance(text, str) and len(text.encode()) <= 4096
    return '"' + ''.join('\\x%02x' % byte for byte in text.encode()) + '"'

out = ['#include "Blunch/BlunchRandomProgram.hpp"',
       'inline blunch::random_program::Catalogue testRandomCatalogue() {',
       '  blunch::random_program::Catalogue result;']
for key in ('startup', 'randomize'):
    out.append(f'  result.{key} = {literal(config[key])};')
for name, definition in config['definitions'].items():
    choices = [{'weight': 1, 'template': definition}] if isinstance(definition, str) else definition
    assert choices and len(choices) <= 64
    total = 0
    for choice in choices:
        weight = choice['weight']
        assert type(weight) in (int, float) and math.isfinite(weight) and weight >= 0
        total += weight
        out.append(f'  result.definitions[{literal(name)}].emplace_back({weight}, {literal(choice["template"])});')
    assert math.isfinite(total) and total > 0
out += ['  return result;', '}']
Path(sys.argv[1]).write_text('\n'.join(out) + '\n')
