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
ET.SubElement(svg, tag('rect'), width='900', height='380', fill='#dedede')
ET.SubElement(svg, tag('path'), d='M 0,0 L 900,0 L 900,380 L 894,376 L 894,4 L 5,4 L 5,375 L 0,380 Z', fill='#a4a4a4')

ET.SubElement(svg, tag('rect'), x='0', y='0', width='500', height='380', fill='#0d1d1c')

def embed(root, x, y, width, height, name, bounds=None):
    vx, vy, vw, vh = bounds or map(float, root.get('viewBox').split())
    group = ET.SubElement(svg, tag('g'), id=name,
        transform=f'translate({x},{y}) scale({width/vw},{height/vh}) translate({-vx},{-vy})')
    for el in root:
        for child in el.iter():
            if child.get('id'): child.set('id', name+'-'+child.get('id'))
        group.append(el)

def lettering(text, x, y, width, height, rand=22):
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

box(507,3,127,30,'title-plaque','#cfcfcf')
lettering('Windchimes',514,9,112,17,42)
box(507,39,265,95,'tuning-box','#ececec')
box(507,137,265,96,'sound-box','#d3d3d3')
box(507,234,265,85,'weather-box','#e8e8e8')
box(507,322,265,47,'connections-box','#e8e8e8')
for i,label in enumerate(['Tubes','Root','Register','Fine','EDO','Spread']):
    lettering(label,515+i*45,120,38,7)
lettering('Level',638,87,35,8.5)
lettering('Swing',687,87,35,8.5)
lettering('Sail',736,87,35,8.5)
for i,label in enumerate(['Decay','Bright','Hard','Weight','Shape','Body','Inharm']):
    lettering(label,512+i*38.5,220,33,7)
lettering('Weather',518,236,100,6,22)
for i,label in enumerate(['Wind','Gusts','Turb','Wind mix','Tone','Texture']):
    lettering(label,515+i*45,268,38,7)
for i,label in enumerate(['Output','Rev wet','Size','Delay mix','Time','Feedback']):
    lettering(label,515+i*45,307,38,6.5)
for label,x in [('Wind CV',518),('1V/oct',553),('Gust',588),('Clock',623)]:
    lettering(label,x,337,27,5.5)
lettering('Wind CV out',643,331,18,5)
lettering('Wind audio',643,355,18,5)
for label,x,y in [('FL',695,331),('FR',733,331),('RL',695,355),('RR',733,355)]:
    lettering(label,x,y,12,5.5)
lettering('computerscare',591,373,97,6,27)
ET.indent(svg)
ET.ElementTree(svg).write(ROOT/'res/panels/ComputerscareWindchimesPanel.svg',encoding='utf-8',xml_declaration=True)
