#!/usr/bin/env python3
"""Generate Settings lettering with the shared distorted type tool."""
from pathlib import Path
import importlib.util
import subprocess
import tempfile
import xml.etree.ElementTree as E

ROOT = Path(__file__).resolve().parent.parent
NS = '{http://www.w3.org/2000/svg}'
E.register_namespace('', NS[1:-1])
spec = importlib.util.spec_from_file_location('svg_text', ROOT/'scripts/svg_text_randomize.py')
helper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)
with tempfile.TemporaryDirectory() as tmp:
    out = Path(tmp)/'settings.svg'
    subprocess.run([str(ROOT/'scripts/gen.sh'), 'Set', '--rand', '24', '--seed', 'blunch-settings', '--output', str(out)], check=True)
    root = E.parse(out).getroot()
    points = [p for path in root.iter(NS+'path') for seg in helper.parse_path(path.get('d')) for p in seg['points']]
    xs, ys = [p[0] for p in points], [p[1] for p in points]
    x, y, w, h = min(xs)-1, min(ys)-1, max(xs)-min(xs)+2, max(ys)-min(ys)+2
    svg=E.Element(NS+'svg', width='26', height='12', viewBox='0 0 26 12')
    group=E.SubElement(svg, NS+'g', transform=f'translate({(26-w*min(26/w,12/h))/2},0) scale({min(26/w,12/h)}) translate({-x},{-y})')
    for child in root: group.append(child)
    E.indent(svg)
    E.ElementTree(svg).write(ROOT/'res/components/blunch-channels/settings.svg', encoding='utf-8', xml_declaration=True)

