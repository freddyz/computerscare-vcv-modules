#!/usr/bin/env python3
"""Generate Blunch's dynamic channel captions with the shared distorted type tool."""
from pathlib import Path
import importlib.util
import subprocess
import tempfile
import xml.etree.ElementTree as E
ROOT = Path(__file__).resolve().parent.parent
N = 'http://www.w3.org/2000/svg'
E.register_namespace('', N)
def tag(name): return '{'+N+'}'+name
spec=importlib.util.spec_from_file_location('type',ROOT/'scripts/svg_text_randomize.py')
helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
output=ROOT/'res/components/blunch-channels';output.mkdir(exist_ok=True)
captions=[(channel, 'All' if channel==0 else f'Ch {channel}', f'channel-{channel}.svg', 51, 11) for channel in range(17)]
captions += [(-1, 'Edit', 'edit.svg', 26, 12), (-1, 'Ctrl', 'ctrl.svg', 26, 12)]
for channel,caption,filename,max_width,max_height in captions:
    with tempfile.TemporaryDirectory() as tmp:
        source=Path(tmp)/'text.svg'
        subprocess.run([str(ROOT/'scripts/gen.sh'),caption,'--rand','40','--seed','blunch-'+caption,'--output',str(source)],check=True,stdout=subprocess.DEVNULL)
        artwork=E.parse(source).getroot()
        if channel == 15:
            # Keep the other glyphs as-is; only soften the distorted "1".
            mild=Path(tmp)/'mild.svg'
            subprocess.run([str(ROOT/'scripts/gen.sh'),caption,'--rand','16','--seed','blunch-'+caption,'--output',str(mild)],check=True,stdout=subprocess.DEVNULL)
            clean=E.parse(mild).getroot()
            replacement=next(path for path in clean.iter(tag('path')) if path.get('id')=='char-4')
            target=next(path for path in artwork.iter(tag('path')) if path.get('id')=='char-4')
            target.set('d',replacement.get('d'))

    points=[]
    for path in artwork.iter(tag('path')):
        for segment in helper.parse_path(path.get('d')): points.extend(segment['points'])
    xs=[p[0] for p in points];ys=[p[1] for p in points]
    x,y=min(xs)-1,min(ys)-1
    w,h=max(xs)-x+1,max(ys)-y+1
    scale=min(max_height/h,max_width/w)
    svg=E.Element(tag('svg'),width=str(w*scale),height=str(h*scale),viewBox=f'0 0 {w*scale} {h*scale}')
    group=E.SubElement(svg,tag('g'),transform=f'scale({scale}) translate({-x},{-y})')
    for child in artwork: group.append(child)
    E.indent(svg)
    E.ElementTree(svg).write(output/filename,encoding='utf-8',xml_declaration=True)
