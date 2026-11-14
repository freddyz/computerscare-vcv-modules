#!/usr/bin/env python3
"""Beekapoo panel using computerscare distorted type and perspective frames."""
from pathlib import Path
import subprocess, tempfile, xml.etree.ElementTree as E, importlib.util
ROOT=Path(__file__).resolve().parent.parent
N='http://www.w3.org/2000/svg';E.register_namespace('',N)
def tag(s):return '{'+N+'}'+s
spec=importlib.util.spec_from_file_location('type',ROOT/'scripts/svg_text_randomize.py');helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
svg=E.Element(tag('svg'),width='360',height='380',viewBox='0 0 360 380')
E.SubElement(svg,tag('rect'),width='360',height='380',fill='#c6c9c7')
def embed(root,x,y,w,h,bounds=None,name='asset'):
    a,b,c,d=bounds or map(float,root.get('viewBox').split())
    group=E.SubElement(svg,tag('g'),id=name,transform=f'translate({x},{y}) scale({w/c},{h/d}) translate({-a},{-b})')
    for child in root:group.append(child)
def text(s,x,y,w,h,rand=25,center=False):
    with tempfile.TemporaryDirectory() as tmp:
        out=Path(tmp)/'text.svg'
        subprocess.run([str(ROOT/'scripts/gen.sh'),s,'--rand',str(rand),'--seed','beekapoo-'+s,'--output',str(out)],check=True,stdout=subprocess.DEVNULL)
        root=E.parse(out).getroot()
    points=[]
    for path in root.iter(tag('path')):
        for seg in helper.parse_path(path.get('d')):points.extend(seg['points'])
    xs=[p[0] for p in points];ys=[p[1] for p in points];bounds=(min(xs)-1,min(ys)-1,max(xs)-min(xs)+2,max(ys)-min(ys)+2)
    actual=min(w,h*bounds[2]/bounds[3])
    embed(root,x-actual*.5 if center else x,y,actual,h,bounds,'label-'+s.replace(' ','-'))
text('Beekapoo',12,7,150,23,40)
# The header artwork is drawn separately so it can fit narrow panel widths.
import copy
asset_dir=ROOT/'res/Beekapoo';asset_dir.mkdir(exist_ok=True)
title=E.Element(tag('svg'),width='150',height='23',viewBox='12 7 150 23')
title.append(copy.deepcopy(svg[-1]))
E.indent(title);E.ElementTree(title).write(asset_dir/'title.svg',encoding='utf-8',xml_declaration=True)
panel=svg
svg=E.Element(tag('svg'),width='80',height='52',viewBox='0 0 80 52')
text('Beek',2,1,76,23,40)
text('apoo',2,27,76,23,40)
E.indent(svg);E.ElementTree(svg).write(asset_dir/'title-stacked.svg',encoding='utf-8',xml_declaration=True)
svg=panel
text('Visible',13,80,33,7,20)
text('All',13,108,33,7,20)

text('computerscare',130,374,80,4,30)
E.indent(svg);E.ElementTree(svg).write(ROOT/'res/panels/ComputerscareBeekapooPanel.svg',encoding='utf-8',xml_declaration=True)
