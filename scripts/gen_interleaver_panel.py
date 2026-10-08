#!/usr/bin/env python3
"""Rebuild Interleaver's panel using the shared lettering and box generators."""
import subprocess
import tempfile
from pathlib import Path
import xml.etree.ElementTree as ET
from svg_text_randomize import parse_path

ROOT = Path(__file__).resolve().parent.parent
NS = 'http://www.w3.org/2000/svg'
ET.register_namespace('', NS)


def tag(name):
    return '{' + NS + '}' + name


panel = ET.Element(tag('svg'), width='360', height='380', viewBox='0 0 360 380')
ET.SubElement(panel, tag('path'), d='M0 0H360V380H0Z', fill='#d9d9d9')

with tempfile.TemporaryDirectory() as work:
    work = Path(work)

    def lettering(parent, text, x, y, w, h, seed):
        file = work / f'text{seed}.svg'
        subprocess.run([str(ROOT / 'scripts/gen.sh'), text, '--rand',
                        '30' if seed == 0 else '18', '--seed',
                        f'interleaver-label{seed}', '--output', str(file)], check=True)
        svg = ET.parse(file).getroot()
        points = [p for path in svg.iter(tag('path'))
                  for segment in parse_path(path.get('d', '')) for p in segment['points']]
        xs, ys = [p[0] for p in points], [p[1] for p in points]
        left, top = min(xs), min(ys)
        width, height = max(xs) - left, max(ys) - top
        scale = min(w / width, h / height)
        group = ET.SubElement(parent, tag('g'), transform=
                              f'translate({x + (w - width * scale) / 2} '
                              f'{y + (h - height * scale) / 2}) '
                              f'scale({scale}) translate({-left} {-top})')
        for child in svg:
            group.append(child)

    def box(seed, x, y, w, h, rotation):
        file = work / f'box{seed}.svg'
        subprocess.run(['python3', str(ROOT / 'scripts/svg_fake_perspective_rect.py'),
                        '--height', str(h), '--aspect', str(w / h), '--depth', '5',
                        '--rand', '12', '--color', '#d4d4d4', '--border', '0.6',
                        '--seed', f'interleaver-box{seed}', '--output', str(file)], check=True)
        group = ET.SubElement(panel, tag('g'), {'id': f'block-{seed}', 'transform':
                              f'translate({x} {y}) rotate({rotation} {w / 2} {h / 2})'})
        for child in ET.parse(file).getroot():
            # Face IDs must be unique across the assembled panel.
            if 'id' in child.attrib:
                child.set('id', f'block-{seed}-{child.get("id")}')
            group.append(child)
        return group

    title = box('title', 5, 4, 346, 30, -.4)
    lettering(title, 'Interleaver', 19, 5, 308, 21, 0)

    # Each whole block (faces and lettering) paints before the row below it.
    # Rotate the complete perspective box, keeping its shallow extrusion intact.
    positions = [(7, 95, 105, 73, -1.3), (124, 93, 109, 77, 1.1),
                 (247, 96, 105, 73, -1.6), (4, 175, 110, 76, 1.5),
                 (127, 173, 105, 80, -1.7), (243, 176, 110, 73, 1.4),
                 (7, 254, 108, 79, -1.1), (124, 255, 111, 78, 1.2),
                 (247, 253, 106, 80, -1.2)]
    names = ['Mode', 'Timing', 'Route', 'A count', 'B count', 'Balance',
             'Buffer', 'Join', 'Interval']
    for i in sorted(range(len(names)), key=lambda index: positions[index][1]):
        name, position = names[i], positions[i]
        group = box(i, *position)
        lettering(group, name, 0, 43, 52, 10, 7 + i * 3)
        lettering(group, 'CV', 70, 2, 18, 7, 8 + i * 3)
        lettering(group, '+/-', 68, 64, 22, 6, 9 + i * 3)

    # The bottom jack block paints last, over the preceding control boxes.
    jacks = box('jacks', 4, 325, 347, 40, 0)
    for seed, text, x, width in [(1, 'A', 29, 14), (2, 'B', 94, 14),
                                  (3, 'Switch', 160, 42), (4, 'Reset', 238, 36),
                                  (5, 'Out', 303, 26)]:
        lettering(jacks, text, x, 33, width, 6, seed)
    lettering(panel, 'computerscare', 99, 369, 232, 9, 6)

ET.SubElement(panel, tag('path'), transform='translate(4 131) scale(.8 .65)',
              d='M9 367 L32 368 L30 376 L11 375 Z M12 368 L29 369 L28 373 L13 373 Z M8 377 L32 376 L35 379 L7 379 Z',
              fill='#292929', **{'fill-rule': 'evenodd'})
ET.SubElement(panel, tag('path'), transform='translate(4 131) scale(.8 .65)',
              d='M15 370 L18 371 M24 370 L26 371 M18 373 L23 372',
              fill='none', stroke='#292929', **{'stroke-width': '1.2'})
ET.ElementTree(panel).write(ROOT / 'res/panels/ComputerscareInterleaverPanel.svg',
                           encoding='unicode', xml_declaration=True)
