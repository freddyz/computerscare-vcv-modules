#!/usr/bin/env python3
"""Build Blunch's on-panel help from its user manual. --check detects drift."""
from pathlib import Path
import argparse
import json
import re

ROOT = Path(__file__).resolve().parent.parent
MANUAL = ROOT / 'doc/blunch.md'
OUTPUT = ROOT / 'src/Blunch/BlunchHelp.hpp'

def plain(text):
    text = text.replace(r'\|', '|')
    text = re.sub(r'\[([^]]+)\]\([^)]*\)', r'\1', text)
    return text.replace('**', '').replace('`', '').strip()

def section(text, heading):
    match = re.search(r'^## ' + re.escape(heading) + r'\s*\n(.*?)(?=^## |\Z)', text, re.M | re.S)
    return match.group(1).strip() if match else ''

def prose(text):
    # Keep paragraphs, list entries and table records separate, wrap in the UI.
    blocks, paragraph = [], []
    def flush():
        if paragraph:
            blocks.append(plain(' '.join(paragraph)))
            paragraph.clear()
    for line in text.splitlines():
        line = line.strip()
        if not line:
            flush()
        elif line.startswith('|'):
            flush()
            cells = re.split(r'(?<!\\)\|', line)[1:-1]
            if len(cells) >= 2 and not re.fullmatch(r'[- :]+', cells[0].strip()):
                if plain(cells[0]) not in ('Formula', 'Input', 'Output', 'Shortcut', 'Control'):
                    blocks.append(plain(cells[0]) + ': ' + plain(cells[1]))
        elif line.startswith('### '):
            flush(); blocks.append(plain(line[4:]))
        elif re.match(r'^(?:- |\d+\. )', line):
            flush(); blocks.append(plain(line))
        else:
            paragraph.append(line)
    flush()
    return '\n\n'.join(blocks)

def language_topics(manual):
    language = section(manual, 'Basic language reference')
    topics = []
    for match in re.finditer(r'^### ([^\n]+)\n(.*?)(?=^### |\Z)', language, re.M | re.S):
        heading, body = match.groups()
        name = plain(heading).split(':')[0]
        name = {'Clock values and units':'Clocks and units',
                'Sequential values and brackets':'Sequences',
                'Nested sequences':'Nested sequences',
                'Random choices and ranges':'Choices and ranges'}.get(name, name)
        examples = []
        # Formula tables already pair every example with its description.
        for row in re.finditer(r'^\|\s*`([^`]+)`\s*\|\s*(.*?)\s*\|\s*$', body, re.M):
            examples.append((plain(row[1]), plain(row[2])))
        # Code fences are runnable formulas, except the explicitly shown output.
        for fence in re.finditer(r'```text\n(.*?)\n```', body, re.S):
            prefix = body[:fence.start()].strip()
            if prefix.endswith('This plays:'):
                continue
            description = plain(prefix.split('\n\n')[-1]).rstrip(':')
            if name == 'Nested sequences':
                description = 'Each item takes a turn. Inner sequences advance once per outer pass.'
            elif name == 'Random mode':
                description = 'Prefix $ always uses the signed Seed value. Prefix ! always uses fresh randomness.'
            elif name == 'Skip probability':
                description = 'A failed ? rests for its interval; a failed ?? skips the block without waiting.'
            elif name == 'Probability':
                description = '75% chance of playing each interval; otherwise rest. Repeat for four pulses.'
            elif name == 'Rests':
                description = 'Alternate a pulse with a silent interval; both take 500 ms.'
            for formula in fence[1].splitlines():
                if formula.strip():
                    examples.append((formula.strip(), 'An inner prefix overrides the group random mode.' if formula.startswith('$[') else description))
        for bullet in re.finditer(r'^- `([^`]+)`\s+(.*)$', body, re.M):
            examples.append((plain(bullet[1]), plain(bullet[2]).rstrip('.')))
        # Basic probability examples are inline in the reference.
        if name in ('Probability', 'Skip probability'):
            formula = '120bpm?75' if name == 'Probability' else '120bpm??75'
            assert '`'+formula+'`' in body
            description = '75% chance of playing; otherwise ' + ('rest for the same interval.' if name == 'Probability' else 'skip immediately to the next block.')
            examples.insert(0, (formula, description))
        unique = list(dict.fromkeys(examples))
        if unique:
            topics.append((name, unique))
    return topics

def generate():
    manual = MANUAL.read_text()
    overview = plain(manual.split('## Quick start')[0].split('\n', 1)[1]) + '\n\n' + prose(section(manual, 'Quick start')) + '\n\n' + prose(section(manual, 'Pages, lines, and blocks'))
    controls = [(name, prose(section(manual, name))) for name in ('Controls', 'Inputs', 'Outputs', 'Keyboard shortcuts')]
    q = lambda value: json.dumps(value, ensure_ascii=False)
    out = ['// Generated from doc/blunch.md by scripts/generate-blunch-help.py.',
           '// clang-format off', '#pragma once', '#include "../components/TerminalSettings.hpp"',
           'inline computerscare::TerminalSetting blunchHelp() {',
           '  using Entry = computerscare::TerminalSetting;',
           '  Entry help; help.name="Help";',
           '  Entry overview; overview.name="Overview"; overview.body='+q(overview)+';',
           '  Entry controls; controls.name="Controls";']
    for name, body in controls:
        out += ['  { Entry page; page.name='+q('Panel' if name == 'Controls' else name)+'; page.body='+q(body)+'; controls.children.push_back(page); }']
    out += ['  Entry language; language.name="Language";']
    for name, examples in language_topics(manual):
        out += ['  { Entry topic; topic.name='+q(name)+';']
        for formula, description in examples:
            out += ['    { Entry example; example.name='+q(formula)+'; example.copyText='+q(formula)+'; example.help='+q(description)+'; topic.children.push_back(example); }']
        out += ['    language.children.push_back(topic); }']
    out += ['  help.children={overview,controls,language}; return help;', '}', '// clang-format on', '']
    return '\n'.join(out)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    content = generate()
    if args.check:
        if not OUTPUT.exists() or OUTPUT.read_text() != content:
            raise SystemExit('Blunch Help is out of date: run python3 scripts/generate-blunch-help.py')
        print('Blunch Help matches the user reference.')
    else:
        OUTPUT.write_text(content)
