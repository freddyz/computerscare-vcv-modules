#!/usr/bin/env python3
"""Compact controller title, knob labels and footer branding."""
from pathlib import Path
import subprocess, tempfile, xml.etree.ElementTree as E, importlib.util, re
ROOT=Path(__file__).resolve().parent.parent
N='http://www.w3.org/2000/svg';E.register_namespace('',N)
def tag(s):return '{'+N+'}'+s
spec=importlib.util.spec_from_file_location('type',ROOT/'scripts/svg_text_randomize.py');helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
svg=E.Element(tag('svg'),width='225',height='380',viewBox='0 0 225 380')
def embed(root,x,y,w,h,bounds=None,name='asset'):
    a,b,c,d=bounds or map(float,root.get('viewBox').split())
    group=E.SubElement(svg,tag('g'),id=name,transform=f'translate({x},{y}) scale({w/c},{h/d}) translate({-a},{-b})')
    for child in root:group.append(child)
def text(s,x,y,w,h,rand=20,center=True):
    with tempfile.TemporaryDirectory() as tmp:
        out=Path(tmp)/'text.svg'
        subprocess.run([str(ROOT/'scripts/gen.sh'),s,'--rand',str(rand),'--seed','controller-'+s,'--output',str(out)],check=True,stdout=subprocess.DEVNULL)
        root=E.parse(out).getroot()
    points=[]
    for path in root.iter(tag('path')):
        for seg in helper.parse_path(path.get('d')):points.extend(seg['points'])
    xs=[p[0] for p in points];ys=[p[1] for p in points];bounds=(min(xs)-1,min(ys)-1,max(xs)-min(xs)+2,max(ys)-min(ys)+2)
    width=min(w,h*bounds[2]/bounds[3]);embed(root,x-width/2 if center else x,y,width,h,bounds)

for xbox,width in [(False,225),(True,180)]:
    svg=E.Element(tag('svg'),width=str(width),height='380',viewBox=f'0 0 {width} 380')
    text('xbox controller' if xbox else 'ps5 controller',10,5,width-24,16,38,False)
    for label,x in [('Dead zone',32),('Threshold',90 if xbox else 105),('Slew',148 if xbox else 178)]:text(label,x,52,56,7.5,20)
    logo=E.parse(ROOT/'res/components/computerscare-logo-normal.svg').getroot()
    embed(logo,119 if xbox else 184,367 if xbox else 366,13 if xbox else 17,12,name='controller-footer-logo')
    g=svg[-1];g.set('transform',g.get('transform')+(' rotate(5 12 12) skewX(-3)' if xbox else ' rotate(-7 12 12) skewX(4)'))
    text('computerscare',135 if xbox else 116,370,39 if xbox else 62,5 if xbox else 6.5,27,False)
    E.indent(svg);E.ElementTree(svg).write(ROOT/('res/panels/ComputerscareControllerXboxPanel.svg' if xbox else 'res/panels/ComputerscareControllerPanel.svg'),encoding='utf-8',xml_declaration=True)
