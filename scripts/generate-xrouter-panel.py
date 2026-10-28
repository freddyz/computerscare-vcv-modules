#!/usr/bin/env python3
"""XRouter panel using computerscare distorted type and perspective frames."""
from pathlib import Path
import subprocess, tempfile, xml.etree.ElementTree as E, importlib.util
ROOT=Path(__file__).resolve().parent.parent
N='http://www.w3.org/2000/svg';E.register_namespace('',N)
def tag(s):return '{'+N+'}'+s
spec=importlib.util.spec_from_file_location('type',ROOT/'scripts/svg_text_randomize.py');helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
svg=E.Element(tag('svg'),width='495',height='380',viewBox='0 0 495 380')
def embed(root,x,y,w,h,bounds=None,name='asset'):
    a,b,c,d=bounds or map(float,root.get('viewBox').split())
    group=E.SubElement(svg,tag('g'),id=name,transform=f'translate({x},{y}) scale({w/c},{h/d}) translate({-a},{-b})')
    for child in root:group.append(child)
def text(s,x,y,w,h,rand=25,center=False):
    with tempfile.TemporaryDirectory() as tmp:
        out=Path(tmp)/'text.svg'
        subprocess.run([str(ROOT/'scripts/gen.sh'),s,'--rand',str(rand),'--seed','xrouter-'+s,'--output',str(out)],check=True,stdout=subprocess.DEVNULL)
        root=E.parse(out).getroot()
    points=[]
    for path in root.iter(tag('path')):
        for seg in helper.parse_path(path.get('d')):points.extend(seg['points'])
    xs=[p[0] for p in points];ys=[p[1] for p in points];bounds=(min(xs)-1,min(ys)-1,max(xs)-min(xs)+2,max(ys)-min(ys)+2)
    actual=min(w,h*bounds[2]/bounds[3])
    embed(root,x-actual*.5 if center else x,y,actual,h,bounds,'label-'+s.replace(' ','-'))
# The frame component draws panel blocks at runtime.
# Background and frames are drawn by the widget.
text('XRouter',8,4,112,22,38)
logo=E.parse(ROOT/'res/components/computerscare-logo-normal.svg').getroot()
embed(logo,133,7,20,17,name='xrouter-logo')
svg[-1].set('transform','translate(143,15) rotate(7) skewX(-8) translate(-143,-15) '+svg[-1].get('transform'))
text('computerscare',323,7,121,13,35)
E.indent(svg);E.ElementTree(svg).write(ROOT/'res/panels/ComputerscareXRouterPanel.svg',encoding='utf-8',xml_declaration=True)
