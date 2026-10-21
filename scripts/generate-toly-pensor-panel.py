#!/usr/bin/env python3
"""Toly Pensor panel using computerscare distorted type and perspective frames."""
from pathlib import Path
import subprocess, tempfile, xml.etree.ElementTree as E, importlib.util
ROOT=Path(__file__).resolve().parent.parent
N='http://www.w3.org/2000/svg';E.register_namespace('',N)
def tag(s):return '{'+N+'}'+s
spec=importlib.util.spec_from_file_location('type',ROOT/'scripts/svg_text_randomize.py');helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
svg=E.Element(tag('svg'),width='780',height='380',viewBox='0 0 780 380')
def embed(root,x,y,w,h,bounds=None,name='asset'):
    a,b,c,d=bounds or map(float,root.get('viewBox').split())
    group=E.SubElement(svg,tag('g'),id=name,transform=f'translate({x},{y}) scale({w/c},{h/d}) translate({-a},{-b})')
    for child in root:group.append(child)
def text(s,x,y,w,h,rand=25,center=False):
    with tempfile.TemporaryDirectory() as tmp:
        out=Path(tmp)/'text.svg'
        subprocess.run([str(ROOT/'scripts/gen.sh'),s,'--rand',str(rand),'--seed','toly-pensor-'+s,'--output',str(out)],check=True,stdout=subprocess.DEVNULL)
        root=E.parse(out).getroot()
    points=[]
    for path in root.iter(tag('path')):
        for seg in helper.parse_path(path.get('d')):points.extend(seg['points'])
    xs=[p[0] for p in points];ys=[p[1] for p in points];bounds=(min(xs)-1,min(ys)-1,max(xs)-min(xs)+2,max(ys)-min(ys)+2)
    actual=min(w,h*bounds[2]/bounds[3])
    embed(root,x-actual*.5 if center else x,y,actual,h,bounds,'label-'+s.replace(' ','-'))
# The frame component draws panel blocks at runtime.
# Background and frames are drawn by the widget.
text('Toly Pensor',17,7,202,23,38)
logo=E.parse(ROOT/'res/components/computerscare-logo-normal.svg').getroot()
embed(logo,223,9,20,18,name='toly-logo')
svg[-1].set('transform','translate(233,18) rotate(7) skewX(-8) translate(-233,-18) '+svg[-1].get('transform'))
E.indent(svg);E.ElementTree(svg).write(ROOT/'res/panels/ComputerscareTolyPensorPanel.svg',encoding='utf-8',xml_declaration=True)
svg=E.Element(tag('svg'),width='780',height='380',viewBox='0 0 780 380')


for i,label in enumerate(['Rnd %','Rnd range','Wig %','Wig range']):text(label,444+i*49,59,46,6)
text('poly',735,24,28,5)
for i,label in enumerate(['Input port','Input channel','Output port','Output channel','Everything']):
    text(label,99+i*113,313,96,7)
text('computerscare',332,367,119,7,45)
E.indent(svg);E.ElementTree(svg).write(ROOT/'res/panels/ComputerscareTolyPensorLabels.svg',encoding='utf-8',xml_declaration=True)

svg=E.Element(tag('svg'),width='780',height='380',viewBox='0 0 780 380')
for axis in range(4):
    text('scale',99,109+axis*54,25,6)
    text('offset',132,109+axis*54,27,6)
E.indent(svg);E.ElementTree(svg).write(ROOT/'res/panels/ComputerscareTolyPensorAdjustmentLabels.svg',encoding='utf-8',xml_declaration=True)
