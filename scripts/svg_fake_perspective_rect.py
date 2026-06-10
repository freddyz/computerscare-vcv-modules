#!/usr/bin/env python3
import argparse
import math
import random
import re
import sys
import xml.etree.ElementTree as ET

SVG_NS = "http://www.w3.org/2000/svg"

ET.register_namespace("", SVG_NS)

HEX_RE = re.compile(r"^#?([0-9a-fA-F]{3}|[0-9a-fA-F]{6})$")
NAMED_COLORS = {
    "black": (0, 0, 0),
    "white": (255, 255, 255),
    "grey": (128, 128, 128),
    "gray": (128, 128, 128),
}


def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate a fake-perspective 3-face SVG rectangle."
    )
    parser.add_argument(
        "-o",
        "--output",
        help="Output SVG. Defaults to stdout.",
    )
    parser.add_argument(
        "--aspect",
        type=float,
        default=2.0,
        help="Top face aspect ratio, width / height. Default: 2.",
    )
    parser.add_argument(
        "--rand",
        type=float,
        default=0.0,
        help="Random color variation amount, 0-100. Default: 0.",
    )
    parser.add_argument(
        "--color",
        default="#cfcfcf",
        help="Base color as #rgb, #rrggbb, or greyscale 0-255. Default: #cfcfcf.",
    )
    parser.add_argument(
        "--height",
        type=float,
        default=100.0,
        help="Top face height in SVG units. Default: 100.",
    )
    parser.add_argument(
        "--angle",
        type=float,
        default=45.0,
        help="Perspective angle in SVG degrees: 0 right, 90 down, 180 left, 225 up-left. Default: 45.",
    )
    parser.add_argument(
        "--border",
        type=float,
        default=0.0,
        help="Border thickness for each face. Default: 0.",
    )
    parser.add_argument(
        "--border-color",
        default="#000000",
        help="Border color as #rgb, #rrggbb, or greyscale 0-255. Default: black.",
    )
    parser.add_argument("--seed", help="Seed for repeatable color variation.")
    return parser.parse_args()


def qname(name):
    return f"{{{SVG_NS}}}{name}"


def clamp(value, low, high):
    return max(low, min(high, value))


def fmt_num(value):
    if abs(value) < 0.000001:
        value = 0.0
    text = f"{value:.4f}".rstrip("0").rstrip(".")
    return text if text else "0"


def parse_color(value):
    text = value.strip()
    named = NAMED_COLORS.get(text.lower())
    if named:
        return named

    if re.fullmatch(r"\d+(?:\.\d+)?", text):
        grey = clamp(int(round(float(text))), 0, 255)
        return (grey, grey, grey)

    match = HEX_RE.match(text)
    if not match:
        raise ValueError(f"Invalid color: {value}")

    digits = match.group(1)
    if len(digits) == 3:
        digits = "".join(ch * 2 for ch in digits)

    return (
        int(digits[0:2], 16),
        int(digits[2:4], 16),
        int(digits[4:6], 16),
    )


def color_to_hex(color):
    return "#{:02x}{:02x}{:02x}".format(*color)


def shade_color(base, shade, rand_amount, rng):
    jitter = rng.uniform(-rand_amount, rand_amount)
    shift = shade + jitter
    return tuple(clamp(int(round(channel + shift)), 0, 255) for channel in base)


def wiggle_point(point, amount, rng):
    if amount <= 0:
        return point
    x, y = point
    return (
        x + rng.uniform(-amount, amount),
        y + rng.uniform(-amount, amount),
    )


def path_data(points):
    parts = []
    for index, (x, y) in enumerate(points):
        command = "M" if index == 0 else "L"
        parts.append(f"{command} {fmt_num(x)} {fmt_num(y)}")
    parts.append("Z")
    return " ".join(parts)


def add_path(parent, face_id, points, fill, border, border_color):
    attrs = {
        "id": face_id,
        "d": path_data(points),
        "fill": color_to_hex(fill),
    }
    if border > 0:
        attrs.update(
            {
                "stroke": color_to_hex(border_color),
                "stroke-width": fmt_num(border),
                "stroke-linejoin": "round",
            }
        )
    else:
        attrs["stroke"] = "none"
    ET.SubElement(parent, qname("path"), attrs)


def point_toward(point, target, amount):
    if amount <= 0:
        return point
    x, y = point
    tx, ty = target
    dx = tx - x
    dy = ty - y
    distance = math.hypot(dx, dy)
    if distance <= 0:
        return point
    scale = amount / distance
    return (x + dx * scale, y + dy * scale)


def overlap_side(points, anchors, amount):
    if amount <= 0:
        return points
    return [point_toward(point, anchor, amount) for point, anchor in zip(points, anchors)]


def build_svg(args):
    if args.aspect <= 0:
        raise ValueError("--aspect must be greater than 0")
    if args.height <= 0:
        raise ValueError("--height must be greater than 0")
    if args.border < 0:
        raise ValueError("--border must be 0 or greater")

    rng = random.Random(args.seed)
    rand_percent = clamp(args.rand, 0.0, 100.0)
    color_rand_amount = rand_percent * 0.64
    point_rand_amount = args.height * rand_percent / 100.0 * 0.08
    base = parse_color(args.color)
    border_color = parse_color(args.border_color)

    top_h = args.height
    top_w = top_h * args.aspect
    depth = top_h * 0.5
    angle = math.radians(args.angle)
    projection = (math.cos(angle) * depth, math.sin(angle) * depth)

    a = (0.0, 0.0)
    b = (top_w, 0.0)
    c = (top_w, top_h)
    d = (0.0, top_h)
    av = (a[0] + projection[0], a[1] + projection[1])
    bv = (b[0] + projection[0], b[1] + projection[1])
    cv = (c[0] + projection[0], c[1] + projection[1])
    dv = (d[0] + projection[0], d[1] + projection[1])

    a = wiggle_point(a, point_rand_amount, rng)
    b = wiggle_point(b, point_rand_amount, rng)
    c = wiggle_point(c, point_rand_amount, rng)
    d = wiggle_point(d, point_rand_amount, rng)
    av = wiggle_point(av, point_rand_amount, rng)
    bv = wiggle_point(bv, point_rand_amount, rng)
    cv = wiggle_point(cv, point_rand_amount, rng)
    dv = wiggle_point(dv, point_rand_amount, rng)

    overlap = max(0.35, args.height * 0.003)
    if projection[0] >= 0:
        side_1 = overlap_side([b, c, cv, bv], [a, d, dv, a], overlap)
    else:
        side_1 = overlap_side([a, d, dv, av], [b, c, cv, b], overlap)

    if projection[1] >= 0:
        side_2 = overlap_side([d, c, cv, dv], [a, b, bv, a], overlap)
    else:
        side_2 = overlap_side([a, b, bv, av], [d, c, cv, d], overlap)

    faces = [
        ("side-1", side_1, -24.0),
        ("side-2", side_2, -42.0),
        ("top", [a, b, c, d], 12.0),
    ]

    xs = [x for _, points, _ in faces for x, _ in points]
    ys = [y for _, points, _ in faces for _, y in points]
    pad = args.border / 2.0
    min_x = min(xs) - pad
    min_y = min(ys) - pad
    max_x = max(xs) + pad
    max_y = max(ys) + pad
    width = max_x - min_x
    height = max_y - min_y

    svg = ET.Element(
        qname("svg"),
        {
            "width": fmt_num(width),
            "height": fmt_num(height),
            "viewBox": " ".join(fmt_num(value) for value in (min_x, min_y, width, height)),
            "version": "1.1",
        },
    )

    for face_id, points, shade in faces:
        add_path(
            svg,
            face_id,
            points,
            shade_color(base, shade, color_rand_amount, rng),
            args.border,
            border_color,
        )

    return ET.tostring(svg, encoding="unicode")


def main():
    args = parse_args()
    try:
        svg = build_svg(args)
    except ValueError as err:
        print(err, file=sys.stderr)
        return 1

    if args.output:
        with open(args.output, "w", encoding="utf-8") as handle:
            handle.write(svg)
            handle.write("\n")
    else:
        print(svg)
    return 0


if __name__ == "__main__":
    sys.exit(main())
