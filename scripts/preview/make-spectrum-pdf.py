#!/usr/bin/env python3
"""Lay out the 36 native Beekapoo captures on two Letter print pages."""
import argparse
import json
import reportlab
from pathlib import Path
from reportlab.lib.pagesizes import letter
from reportlab.lib.utils import ImageReader
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

font_dir = Path(reportlab.__file__).parent / "fonts"
pdfmetrics.registerFont(TTFont("SpectrumSans", str(font_dir / "Vera.ttf")))
pdfmetrics.registerFont(TTFont("SpectrumSans-Bold", str(font_dir / "VeraBd.ttf")))

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("captures", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
cases = json.loads((args.captures / "print-manifest.json").read_text())
assert len(cases) == 36
args.output.parent.mkdir(parents=True, exist_ok=True)
pdf = canvas.Canvas(str(args.output), pagesize=letter, pageCompression=1)
pdf.setTitle("Beekapoo - 36 sparse layouts")
pdf.setAuthor("computerscare")
page_w, page_h = letter
margin, gap_x, gap_y = 24, 7, 10
cell_w = (page_w - 2 * margin - 5 * gap_x) / 6
cell_h = 226
for page in range(2):
    pdf.setFillColorRGB(0.08, 0.11, 0.1)
    pdf.setFont("SpectrumSans-Bold", 15)
    pdf.drawString(margin, page_h - 31, "Beekapoo - sparse layouts")
    pdf.setFont("SpectrumSans", 8)
    pdf.drawString(margin, page_h - 46, "Counts refer to exposed controls and jacks; built-in controls are additional.")
    pdf.drawRightString(page_w - margin, page_h - 31, f"{page+1} / 2")
    for slot, case in enumerate(cases[page*18:(page+1)*18]):
        col, row = slot % 6, slot // 6
        x = margin + col * (cell_w + gap_x)
        top = page_h - 65 - row * (cell_h + gap_y)
        pdf.setFont("SpectrumSans-Bold", 8)
        pdf.drawString(x, top, f"{case['index']:02d}  /  {case['hp']} HP")
        pdf.setFont("SpectrumSans", 6.6)
        knobs = "knob" if case["knobs"] == 1 else "knobs"
        buttons = "button" if case["buttons"] == 1 else "buttons"
        pdf.drawString(x, top - 11, f"{case['knobs']} {knobs} / {case['buttons']} {buttons}")
        pdf.drawString(x, top - 21, f"{case['inputs']} in / {case['outputs']} out")
        reader = ImageReader(str(args.captures / case["image"]))
        width, height = reader.getSize()
        scale = min(cell_w / width, (cell_h - 31) / height)
        draw_w, draw_h = width * scale, height * scale
        pdf.drawImage(reader, x + (cell_w - draw_w)/2, top - 30 - draw_h,
                      width=draw_w, height=draw_h, mask="auto")
    pdf.showPage()
pdf.save()
print(args.output)
