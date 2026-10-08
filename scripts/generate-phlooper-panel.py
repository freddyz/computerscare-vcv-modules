#!/usr/bin/env python3
"""Phlooper panel using computerscare distorted type and perspective frames."""
from pathlib import Path
import subprocess, tempfile, xml.etree.ElementTree as E, importlib.util
ROOT=Path(__file__).resolve().parent.parent
N='http://www.w3.org/2000/svg';E.register_namespace('',N)
def tag(s):return '{'+N+'}'+s
spec=importlib.util.spec_from_file_location('type',ROOT/'scripts/svg_text_randomize.py');helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
svg=E.Element(tag('svg'),width='330',height='380',viewBox='0 0 330 380')
def embed(root,x,y,w,h,bounds=None,name='asset'):
    a,b,c,d=bounds or map(float,root.get('viewBox').split())
    group=E.SubElement(svg,tag('g'),id=name,transform=f'translate({x},{y}) scale({w/c},{h/d}) translate({-a},{-b})')
    for child in root:group.append(child)
def text(s,x,y,w,h,rand=25):
    with tempfile.TemporaryDirectory() as tmp:
        out=Path(tmp)/'text.svg'
        subprocess.run([str(ROOT/'scripts/gen.sh'),s,'--rand',str(rand),'--seed','phlooper-'+s,'--output',str(out)],check=True,stdout=subprocess.DEVNULL)
        root=E.parse(out).getroot()
    points=[]
    for path in root.iter(tag('path')):
        for seg in helper.parse_path(path.get('d')):points.extend(seg['points'])
    xs=[p[0] for p in points];ys=[p[1] for p in points];bounds=(min(xs)-1,min(ys)-1,max(xs)-min(xs)+2,max(ys)-min(ys)+2)
    embed(root,x,y,min(w,h*bounds[2]/bounds[3]),h,bounds,'label-'+s.replace(' ','-'))
# Perspective blocks are drawn by the procedural WarpedBlock component.
text('Phlooper',16,5,144,24,48)
for s,x in [('Start',22),('Length',74),('Offset',126),('Speed',178),('Rec mix',226),('Out mix',278)]:text(s,x,215,45,7)
for s,x in [('Record',16),('Erase',66),('Restart',111),('Stop',168),('Hold',218),('Hold CV',253),('EOC',288)]:text(s,x,270,35,5)
for s,x in [('L / mono',14),('R',76),('Record',117),('Erase',170),('Restart',219),('Out L',281)]:text(s,x,318,48,6)
for s,x in [('Mute',17),('Stop',69),('Start CV',118),('Length CV',165),('Offset CV',217),('Out R',281)]:text(s,x,361,48,5.5)
logo=E.parse(ROOT/'res/components/computerscare-logo-normal.svg').getroot()
embed(logo,94,367,12,12,name='computerscare-logo')
text('computerscare',111,371,160,7,47)
E.indent(svg);E.ElementTree(svg).write(ROOT/'res/panels/ComputerscarePhlooperPanel.svg',encoding='utf-8',xml_declaration=True)
