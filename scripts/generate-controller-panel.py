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
text('Controller',10,5,187,16,38,False)
for label,x in [('Dead zone',32),('Threshold',105),('Slew',178)]:text(label,x,52,56,7.5,20)
logo=E.parse(ROOT/'res/components/computerscare-logo-normal.svg').getroot();embed(logo,184,366,17,12,name='controller-footer-logo');g=svg[-1];g.set('transform',g.get('transform')+' rotate(-7 12 12) skewX(4)')
text('computerscare',116,370,62,6.5,27,False)
E.indent(svg);E.ElementTree(svg).write(ROOT/'res/panels/ComputerscareControllerPanel.svg',encoding='utf-8',xml_declaration=True)
