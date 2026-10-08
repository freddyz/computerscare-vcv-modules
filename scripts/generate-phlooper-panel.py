#!/usr/bin/env python3
"""Phlooper panel using computerscare distorted type and perspective frames."""
from pathlib import Path
import subprocess, tempfile, xml.etree.ElementTree as E, importlib.util, random
ROOT=Path(__file__).resolve().parent.parent
N='http://www.w3.org/2000/svg';E.register_namespace('',N)
def tag(s):return '{'+N+'}'+s
spec=importlib.util.spec_from_file_location('type',ROOT/'scripts/svg_text_randomize.py');helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
svg=E.Element(tag('svg'),width='330',height='380',viewBox='0 0 330 380')
E.SubElement(svg,tag('rect'),width='330',height='380',fill='#dedede')
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
def box(x,y,w,h,name,color):
    with tempfile.TemporaryDirectory() as tmp:
        out=Path(tmp)/'box.svg'
        subprocess.run(['python3',str(ROOT/'scripts/svg_fake_perspective_rect.py'),'--aspect',str(w/h),'--height',str(h),'--color',color,'--rand','9','--depth','5','--border','.5','--angle','40','--seed','phlooper-'+name,'--output',str(out)],check=True,stdout=subprocess.DEVNULL)
        embed(E.parse(out).getroot(),x,y,w,h,name=name)
box(9,3,312,31,'title','#cdcdcd');text('Phlooper',22,8,210,20,48)
box(9,150,312,83,'loop-controls','#ececec')
box(9,237,312,42,'actions','#d2d2d2')
box(9,282,312,83,'connections','#e8e8e8')
for s,x in [('Start',22),('Length',83),('Offset',147),('Rec mix',205),('Out mix',271)]:text(s,x,223,52,7)
for s,x in [('Record',31),('Erase',140),('Restart',239)]:text(s,x,270,62,6)
for s,x in [('L / mono',14),('R',76),('Record',117),('Erase',170),('Restart',219),('Out L',281)]:text(s,x,318,48,6)
for s,x in [('Mute',17),('Stop',69),('Start CV',118),('Length CV',165),('Offset CV',217),('Out R',281)]:text(s,x,361,48,5.5)
logo=E.parse(ROOT/'res/components/computerscare-logo-normal.svg').getroot()
rng=random.Random('phlooper-logo')
for path in logo.iter(tag('path')):
    segments=helper.parse_path(path.get('d'))
    helper.randomize_segments(segments,helper.mutation_profile(58),rng)
    path.set('d',helper.path_to_string(segments))
embed(logo,18,367,18,12,name='phlooper-distorted-logo')
text('computerscare',111,371,160,7,47)
E.indent(svg);E.ElementTree(svg).write(ROOT/'res/panels/ComputerscarePhlooperPanel.svg',encoding='utf-8',xml_declaration=True)
