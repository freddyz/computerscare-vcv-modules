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
svg = ET.Element(tag('svg'), width='780', height='380', viewBox='0 0 780 380')
ET.SubElement(svg, tag('rect'), width='780', height='380', fill='#dedede')
ET.SubElement(svg, tag('path'), d='M 0,0 L 780,0 L 780,380 L 774,376 L 774,4 L 5,4 L 5,375 L 0,380 Z', fill='#a4a4a4')

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
    with tempfile.TemporaryDirectory() as tmp:
        out=Path(tmp)/'box.svg'
        subprocess.run(['python3',str(ROOT/'scripts/svg_fake_perspective_rect.py'),
          '--aspect',str(width/height),'--height',str(height),'--color',color,
          '--rand','7','--depth','6','--border','.5','--angle','40','--seed','windchimes-'+name,
          '--output',str(out)],check=True)
        embed(ET.parse(out).getroot(),x,y,width,height,name)

box(507,3,127,30,'title-plaque','#cfcfcf')
lettering('Windchimes',514,9,112,17,42)
box(507,39,269,95,'tuning-box','#ececec')
box(507,137,269,96,'sound-box','#d3d3d3')
box(507,237,269,135,'weather-box','#e8e8e8')
for i,label in enumerate(['Tubes','Root','Register','Fine','EDO','Spread']):
    lettering(label,515+i*45,122,38,7.5)
lettering('Level',658,87,42,7.5)
lettering('Swing',721,87,37,7.5)
for i,label in enumerate(['Decay','Bright','Hard','Shape','Body','Inharm']):
    lettering(label,515+i*45,222,38,7.5)
lettering('Weather',518,239,100,8,22)
for i,label in enumerate(['Wind','Gusts','Turb','Wind mix','Tone','Texture']):
    lettering(label,515+i*45,277,38,7.5)
for i,label in enumerate(['Output','Rev wet','Size']):
    lettering(label,515+i*45,319,38,7.5)
lettering('Make gust',642,320,35,6.5)
for i,label in enumerate(['Wind CV','1V/oct','Gust','Wind out']):
    lettering(label,500+i*32,364,30,6.5)
for label,x,y in [('FL',682,319),('FR',764,319),('RL',682,351),('RR',764,351)]:
    lettering(label,x,y,12,9)
lettering('computerscare',591,373,112,6,27)
ET.indent(svg)
ET.ElementTree(svg).write(ROOT/'res/panels/ComputerscareWindchimesPanel.svg',encoding='utf-8',xml_declaration=True)
