#!/usr/bin/env python3
"""Blunch panel using computerscare distorted type and perspective frames."""
from pathlib import Path
import subprocess, tempfile, xml.etree.ElementTree as E, importlib.util
ROOT=Path(__file__).resolve().parent.parent
N='http://www.w3.org/2000/svg';E.register_namespace('',N)
def tag(s):return '{'+N+'}'+s
spec=importlib.util.spec_from_file_location('type',ROOT/'scripts/svg_text_randomize.py');helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
svg=E.Element(tag('svg'),width='240',height='380',viewBox='0 0 240 380')
def embed(root,x,y,w,h,bounds=None,name='asset'):
    a,b,c,d=bounds or map(float,root.get('viewBox').split())
    group=E.SubElement(svg,tag('g'),id=name,transform=f'translate({x},{y}) scale({w/c},{h/d}) translate({-a},{-b})')
    for child in root:group.append(child)
def text(s,x,y,w,h,rand=25,center=False,stretch=1):
    with tempfile.TemporaryDirectory() as tmp:
        out=Path(tmp)/'text.svg'
        subprocess.run([str(ROOT/'scripts/gen.sh'),s,'--rand',str(rand),'--seed','blunch-'+s,'--output',str(out)],check=True,stdout=subprocess.DEVNULL)
        root=E.parse(out).getroot()
    points=[]
    for path in root.iter(tag('path')):
        for seg in helper.parse_path(path.get('d')):points.extend(seg['points'])
    xs=[p[0] for p in points];ys=[p[1] for p in points];bounds=(min(xs)-1,min(ys)-1,max(xs)-min(xs)+2,max(ys)-min(ys)+2)
    actual_width=min(w,h*bounds[2]/bounds[3])*stretch
    embed(root,x-actual_width/2 if center else x,y,actual_width,h,bounds,'label-'+s.replace(' ','-'))
# Perspective boxes are rendered with the shared WarpedBlock component.
text('BLunch',11,5,92,18,48)
text('Time',55,303,27,6,center=True)
text('Seed',85,303,27,6,center=True)
text('ext',213,319,36,8.5,40,center=True,stretch=1.3)
for label,x,y in [('W',192.5,281),('X',229,281),('Y',192.5,316),('Z',229,316)]:
    text(label,x,y,14.5,5.7,40)
for label,x in [('out',21.5),('phase',51.5),('EOB',117.5),('EOL',147.5),('EOP',177.5)]:
    text(label,x,366,28,6,40,center=True)
text('poly',212,366,28,6,40,center=True)
logo=E.parse(ROOT/'res/components/computerscare-logo-normal.svg').getroot()
embed(logo,104,4,24,24,name='computerscare-blunch-logo')
E.indent(svg);E.ElementTree(svg).write(ROOT/'res/panels/ComputerscareBlunchPanel.svg',encoding='utf-8',xml_declaration=True)
