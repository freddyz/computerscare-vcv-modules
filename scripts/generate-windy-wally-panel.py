#!/usr/bin/env python3
"""Generate Windy Wally lettering with the shared computerscare type tools."""
from pathlib import Path
import importlib.util
import subprocess
import tempfile
import xml.etree.ElementTree as E

ROOT = Path(__file__).resolve().parent.parent
NS = 'http://www.w3.org/2000/svg'
E.register_namespace('', NS)
def tag(name): return '{'+NS+'}'+name
spec = importlib.util.spec_from_file_location('type', ROOT/'scripts/svg_text_randomize.py')
helper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)
svg = E.Element(tag('svg'), width='180', height='380', viewBox='0 0 180 380')
def embed(root, x, y, width, height, name, bounds=None):
    a,b,c,d = bounds or map(float, root.get('viewBox').split())
    group = E.SubElement(svg,tag('g'),id=name,
        transform=f'translate({x},{y}) scale({width/c},{height/d}) translate({-a},{-b})')
    for child in root: group.append(child)
    return group

def text(label, x, y, width, height, center=True, distortion=24):
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp)/'text.svg'
        subprocess.run([str(ROOT/'scripts/gen.sh'),label,'--rand',str(distortion),
            '--seed','windy-wally-'+label,'--output',str(out)],check=True,stdout=subprocess.DEVNULL)
        root = E.parse(out).getroot()
    points = [p for path in root.iter(tag('path')) for seg in helper.parse_path(path.get('d')) for p in seg['points']]
    xs,ys = [p[0] for p in points],[p[1] for p in points]
    bounds = min(xs)-1,min(ys)-1,max(xs)-min(xs)+2,max(ys)-min(ys)+2
    width = min(width,height*bounds[2]/bounds[3])
    embed(root,x-width/2 if center else x,y,width,height,'label-'+label.replace(' ','-'),bounds)

text('Windy Wally',14,12,125,17,False,40)
for i,label in enumerate(['Strength','Gustiness','Turbulence','Tone','Texture','Presence','Width','Gain']):
    text(label,130 if i%2 else 48,44+(i//2)*56,68,8)
text('Gust',130,278,40,7)
for label,x in [('L / mono',35),('R',89),('Wind CV',143)]: text(label,x,357,48,6)
text('computerscare',90,372,120,5,True,38)
logo = E.parse(ROOT/'res/components/computerscare-logo-normal.svg').getroot()
group = embed(logo,148,9,18,22,'windy-wally-logo')
group.set('transform','rotate(7 157 20) skewX(-3) '+group.get('transform'))
E.indent(svg)
E.ElementTree(svg).write(ROOT/'res/panels/ComputerscareWindyWallyPanel.svg',encoding='utf-8',xml_declaration=True)
