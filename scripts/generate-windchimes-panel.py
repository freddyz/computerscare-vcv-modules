#!/usr/bin/env python3
"""Rebuild Windchimes artwork using the shared distorted type/perspective tools."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
NS = 'http://www.w3.org/2000/svg'
ET.register_namespace('', NS)
def tag(name): return f'{{{NS}}}{name}'
spec = importlib.util.spec_from_file_location('svg_type', ROOT / 'scripts/svg_text_randomize.py')
helper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)
svg = ET.Element(tag('svg'), width='900', height='380', viewBox='0 0 900 380')



ET.SubElement(svg, tag('rect'), x='0', y='0', width='500', height='380', fill='#0d1d1c')

def embed(root, x, y, width, height, name, bounds=None):
    vx, vy, vw, vh = bounds or map(float, root.get('viewBox').split())
    group = ET.SubElement(svg, tag('g'), id=name,
        transform=f'translate({x},{y}) scale({width/vw},{height/vh}) translate({-vx},{-vy})')
    for el in root:
        for child in el.iter():
            if child.get('id'): child.set('id', name+'-'+child.get('id'))
        group.append(el)

def lettering(text, x, y, width, height, rand=22, centered=False, right=False):
    x=500+(x-500)*1.45
    width*=1.45
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp)/'type.svg'
        subprocess.run([str(ROOT/'scripts/gen.sh'), text, '--rand',str(rand),
                        '--seed','windchimes-'+text,'--output',str(out)],check=True)
        root = ET.parse(out).getroot()
    points=[]
    for el in root.iter(tag('path')):
        for segment in helper.parse_path(el.get('d')):
            points.extend(segment['points'])
    xs=[p[0] for p in points];ys=[p[1] for p in points]
    bounds=(min(xs)-1,min(ys)-1,max(xs)-min(xs)+2,max(ys)-min(ys)+2)
    # Keep each short caption legible instead of stretching it to its full slot.
    width=min(width,height*bounds[2]/bounds[3])
    if centered: x -= width / 2
    elif right: x -= width
    embed(root,x,y,width,height,'type-'+text.lower().replace(' ','-'),bounds)

def box(x,y,width,height,name,color):
    x=500+(x-500)*1.45
    width*=1.45
    with tempfile.TemporaryDirectory() as tmp:
        out=Path(tmp)/'box.svg'
        subprocess.run(['python3',str(ROOT/'scripts/svg_fake_perspective_rect.py'),
          '--aspect',str(width/height),'--height',str(height),'--color',color,
          '--rand','7','--depth','6','--border','.5','--angle','40','--seed','windchimes-'+name,
          '--output',str(out)],check=True)
        embed(ET.parse(out).getroot(),x,y,width,height,name)

lettering('Windchimes',514,9,98,17,42)
logo=ET.parse(ROOT/'res/components/computerscare-logo-normal.svg').getroot()
embed(logo,669,9,17,17,'windchimes-logo')
# Captions share their controls' center coordinates before the column stretch.
def caption(text, center, y, width=34, height=6.5):
    lettering(text, center + 180, y, width, height, centered=True)

for i,label in enumerate(['Root','Register','Fine','EDO','Spread']):
    caption(label,349.25+i*42,90)
for i,label in enumerate(['Tubes','Level','Swing','Sail','Decay','Bright',
                          'Hardness','Weight','Shape','Body','Inharmonicity']):
    caption(label,349.25+(i%6)*42,195 if i<6 else 230,
            width=40 if label=='Inharmonicity' else 34,
            height=7.5 if label=='Swing' else 6.5)
caption('1V/oct',567.5,90,height=6)
caption('Scale',511,49,width=30,height=6.5)
lettering('Wind',518,245,75,7)
lettering('Reverb',620,245,65,7)
lettering('Delay',620,289,65,7)
for label,center,y in [('Strength',351.25,277),('Gusts',398.25,277),
                       ('Turb',351.25,315),('Mix',398.25,315),
                       ('Tone',351.25,352),('Texture',398.25,352),
                       ('Mix',457.25,277),('Size',495.25,277),
                       ('Mix',457.25,321),('Time',495.25,321),
                       ('Feedback',457.25,364)]:
    caption(label,center,y,width=30,height=6)
caption('Clock',487,364,width=30,height=6)
caption('Wind',553,247,width=55,height=8)
caption('Gain',538,294,width=28,height=6)
caption('Chimes',553,312,width=55,height=7)
for label,center in [('CV',538),('Audio',568)]:
    caption(label,center,257,width=29,height=5.5)
# Outside captions frame a tight square of chime sockets.
for label,x,y,right in [('Front L',713,321,False),('Front R',763,321,True),
                         ('Rear L',713,370,False),('Rear R',763,370,True)]:
    lettering(label,x,y,30,5.5,right=right)
ET.indent(svg)
ET.ElementTree(svg).write(ROOT/'res/panels/ComputerscareWindchimesPanel.svg',encoding='utf-8',xml_declaration=True)
