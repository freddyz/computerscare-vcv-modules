#!/usr/bin/env python3
"""Distorted controller face symbols and D-pad arrows, all editable SVG paths."""
from pathlib import Path
import math
import random
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / 'res/components/controller-icons'
OUTPUT.mkdir(parents=True, exist_ok=True)
NS = 'http://www.w3.org/2000/svg'
ET.register_namespace('', NS)

def write(name, path, outlined=False):
    svg = ET.Element(f'{{{NS}}}svg', width='14', height='14', viewBox='0 0 16 16')
    attrs = {'d': path, 'fill': 'none' if outlined else '#14231c'}
    if outlined:
        attrs.update(stroke='#14231c', **{'stroke-width': '2.1', 'stroke-linejoin': 'bevel'})
    ET.SubElement(svg, f'{{{NS}}}path', attrs)
    ET.indent(svg)
    ET.ElementTree(svg).write(OUTPUT / f'{name}.svg', encoding='utf-8', xml_declaration=True)

def polygon(points):
    return 'M ' + ' L '.join(f'{x:.2f},{y:.2f}' for x, y in points) + ' Z'

write('cross', polygon([(2.4,1.1),(8.1,6.0),(13.0,1.8),(14.7,3.8),(10.0,8.1),(14.0,13.1),(11.9,14.7),(7.7,10.0),(3.0,14.3),(1.4,12.1),(5.9,7.8),(1.0,3.0)]))
write('circle', 'M 8.3,1.6 C 12.4,1.2 14.7,4.8 14.0,8.5 C 14.1,12.5 10.8,14.7 7.1,14.1 C 3.0,14.0 1.3,11.0 1.8,7.4 C 1.6,3.7 4.5,1.5 8.3,1.6 Z', True)
write('square', polygon([(2.0,1.7),(14.1,2.4),(13.3,14.1),(1.7,13.3)]), True)
write('triangle', polygon([(8.4,1.7),(14.2,13.8),(1.8,13.1)]), True)
arrow = [(8,1.1),(14.8,7.5),(10.1,7.3),(10.3,14.7),(5.5,14.4),(5.8,7.5),(1.3,8.0)]
for index, name in enumerate(['up', 'right', 'down', 'left']):
    rng = random.Random(730 + index)
    angle = index * math.pi / 2
    points = []
    for x, y in arrow:
        x, y = x - 8, y - 8
        points.append((8 + x*math.cos(angle) - y*math.sin(angle) + rng.uniform(-.22,.22), 8 + x*math.sin(angle) + y*math.cos(angle) + rng.uniform(-.22,.22)))
    write(f'arrow-{name}', polygon(points))

# Xbox face letters use the same distorted text outlines as the panels.
import importlib.util
import subprocess
import tempfile
spec = importlib.util.spec_from_file_location('controller_type', ROOT/'scripts/svg_text_randomize.py')
helper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)
for letter in 'ABXY':
    with tempfile.TemporaryDirectory() as directory:
        source = Path(directory)/'letter.svg'
        subprocess.run([str(ROOT/'scripts/gen.sh'), letter, '--font', 'Verdana Bold', '--rand', '28', '--seed', 'controller-xbox-'+letter, '--output', str(source)], check=True, stdout=subprocess.DEVNULL)
        original = ET.parse(source).getroot()
    points = [p for path in original.iter(f'{{{NS}}}path') for segment in helper.parse_path(path.get('d')) for p in segment['points']]
    xs, ys = [p[0] for p in points], [p[1] for p in points]
    x,y,w,h=min(xs)-1,min(ys)-1,max(xs)-min(xs)+2,max(ys)-min(ys)+2
    scale=min(14/w,14/h)
    svg=ET.Element(f'{{{NS}}}svg',width='14',height='14',viewBox='0 0 16 16')
    group=ET.SubElement(svg,f'{{{NS}}}g',transform=f'translate({(16-w*scale)/2},{(16-h*scale)/2}) scale({scale}) translate({-x},{-y})')
    for child in original:group.append(child)
    for path in svg.iter(f'{{{NS}}}path'):path.set('fill','#14231c')
    ET.indent(svg)
    ET.ElementTree(svg).write(OUTPUT/f'button-{letter.lower()}.svg',encoding='utf-8',xml_declaration=True)
