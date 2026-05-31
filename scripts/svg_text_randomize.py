#!/usr/bin/env python3
import argparse
import math
import random
import re
import sys
import xml.etree.ElementTree as ET

SVG_NS = "http://www.w3.org/2000/svg"
XLINK_NS = "http://www.w3.org/1999/xlink"

ET.register_namespace("", SVG_NS)
ET.register_namespace("xlink", XLINK_NS)

COMMAND_RE = re.compile(
    r"[AaCcHhLlMmQqSsTtVvZz]|[-+]?(?:\d*\.\d+|\d+\.?)(?:[eE][-+]?\d+)?"
)


def parse_args():
    parser = argparse.ArgumentParser(description="Expand and randomize SVG text paths.")
    parser.add_argument("--input", required=True, help="Input SVG from pango-view.")
    parser.add_argument("--output", help="Output SVG. Defaults to stdout.")
    parser.add_argument("--rand", type=float, default=32, help="Randomization amount, 0-100.")
    parser.add_argument("--seed", help="Seed for repeatable randomization.")
    return parser.parse_args()


def clamp(value, low, high):
    return max(low, min(high, value))


def local_name(tag):
    return tag.rsplit("}", 1)[-1]


def qname(ns, name):
    return f"{{{ns}}}{name}"


def parse_float(value, default=0.0):
    if value is None:
        return default
    try:
        return float(value)
    except ValueError:
        return default


def tokenize_path(path_data):
    return COMMAND_RE.findall(path_data)


def is_command(token):
    return len(token) == 1 and token.isalpha()


def fmt_num(value):
    if abs(value) < 0.000001:
        value = 0.0
    text = f"{value:.4f}".rstrip("0").rstrip(".")
    return text if text else "0"


def parse_path(path_data):
    tokens = tokenize_path(path_data)
    index = 0
    command = None
    segments = []
    current = [0.0, 0.0]
    start = [0.0, 0.0]

    param_counts = {
        "M": 2,
        "L": 2,
        "H": 1,
        "V": 1,
        "C": 6,
        "S": 4,
        "Q": 4,
        "T": 2,
        "A": 7,
        "Z": 0,
    }

    while index < len(tokens):
        if is_command(tokens[index]):
            command = tokens[index]
            index += 1
        if command is None:
            raise ValueError("Path data starts without a command")

        upper = command.upper()
        if upper == "Z":
            segments.append({"cmd": "Z", "points": [], "raw": []})
            current = start[:]
            command = None
            continue

        count = param_counts.get(upper)
        if count is None:
            raise ValueError(f"Unsupported path command: {command}")

        first_moveto = upper == "M"
        while index < len(tokens) and not is_command(tokens[index]):
            if index + count > len(tokens):
                raise ValueError(f"Not enough parameters for path command: {command}")
            values = [float(tokens[index + offset]) for offset in range(count)]
            index += count

            effective = command
            if first_moveto:
                first_moveto = False
            elif upper == "M":
                effective = "l" if command.islower() else "L"
                upper = "L"

            relative = effective.islower()
            eff_upper = effective.upper()
            points = []
            raw = []

            if eff_upper == "M":
                point = to_absolute(values[0], values[1], current, relative)
                current = point[:]
                start = point[:]
                points = [point]
            elif eff_upper == "L":
                point = to_absolute(values[0], values[1], current, relative)
                current = point[:]
                points = [point]
            elif eff_upper == "H":
                x = values[0] + current[0] if relative else values[0]
                current = [x, current[1]]
                points = [current[:]]
            elif eff_upper == "V":
                y = values[0] + current[1] if relative else values[0]
                current = [current[0], y]
                points = [current[:]]
            elif eff_upper == "C":
                points = [
                    to_absolute(values[0], values[1], current, relative),
                    to_absolute(values[2], values[3], current, relative),
                    to_absolute(values[4], values[5], current, relative),
                ]
                current = points[-1][:]
            elif eff_upper == "S" or eff_upper == "Q":
                points = [
                    to_absolute(values[0], values[1], current, relative),
                    to_absolute(values[2], values[3], current, relative),
                ]
                current = points[-1][:]
            elif eff_upper == "T":
                point = to_absolute(values[0], values[1], current, relative)
                current = point[:]
                points = [point]
            elif eff_upper == "A":
                point = to_absolute(values[5], values[6], current, relative)
                current = point[:]
                raw = values[:5]
                points = [point]

            segments.append({"cmd": eff_upper, "points": points, "raw": raw})

            if index < len(tokens) and is_command(tokens[index]):
                break

    return segments


def to_absolute(x, y, current, relative):
    if relative:
        return [current[0] + x, current[1] + y]
    return [x, y]


def path_to_string(segments):
    parts = []
    for segment in segments:
        cmd = segment["cmd"]
        points = segment["points"]
        parts.append(cmd)
        if cmd == "Z":
            continue
        if cmd == "A":
            raw = segment["raw"]
            point = points[0]
            parts.extend(fmt_num(value) for value in raw)
            parts.extend([fmt_num(point[0]), fmt_num(point[1])])
        else:
            for point in points:
                parts.extend([fmt_num(point[0]), fmt_num(point[1])])
    return " ".join(parts)


def apply_matrix(point, matrix):
    x, y = point
    a, b, c, d, e, f = matrix
    return [a * x + c * y + e, b * x + d * y + f]


def multiply(m1, m2):
    a1, b1, c1, d1, e1, f1 = m1
    a2, b2, c2, d2, e2, f2 = m2
    return [
        a1 * a2 + c1 * b2,
        b1 * a2 + d1 * b2,
        a1 * c2 + c1 * d2,
        b1 * c2 + d1 * d2,
        a1 * e2 + c1 * f2 + e1,
        b1 * e2 + d1 * f2 + f1,
    ]


def translation(tx, ty):
    return [1.0, 0.0, 0.0, 1.0, tx, ty]


def rotation(angle):
    return [math.cos(angle), math.sin(angle), -math.sin(angle), math.cos(angle), 0.0, 0.0]


def transform_segments(segments, matrix):
    for segment in segments:
        for point in segment["points"]:
            point[:] = apply_matrix(point, matrix)


def mutable_points(segments):
    points = []
    for segment in segments:
        if segment["cmd"] != "Z":
            points.extend(segment["points"])
    return points


def point_runs(segments):
    runs = []
    current = []
    for segment in segments:
        if segment["cmd"] == "Z":
            if current:
                runs.append(current)
                current = []
            continue
        current.extend(segment["points"])
    if current:
        runs.append(current)
    return runs


def bounds_for_segments(segments):
    xs = []
    ys = []
    for point in mutable_points(segments):
        xs.append(point[0])
        ys.append(point[1])
    if not xs:
        return (0.0, 0.0, 0.0, 0.0)
    return (min(xs), min(ys), max(xs), max(ys))


def mutation_profile(rand_value):
    amount = clamp(rand_value, 0.0, 100.0) / 100.0
    eased = amount * amount * (3.0 - 2.0 * amount)
    return {
        "amount": amount,
        "point_chance": clamp(0.004 + eased * 0.08, 0.0, 0.1),
        "jitter": 0.1 + eased * 1.65,
        "shared_shift_chance": clamp(0.18 + eased * 0.72, 0.0, 0.92),
        "shared_shift": 0.9 + eased * 8.5,
        "drop_chance": 0.0 if amount < 0.8 else clamp((amount - 0.8) / 0.2 * 0.008, 0.0, 0.008),
        "transform_chance": 0.0 if amount <= 0.0 else 0.72 + eased * 0.28,
        "translate_x": eased * 10.0,
        "translate_y": eased * 18.0,
        "spacing": eased * 12.0,
        "rotation": math.radians(eased * 40.0),
        "scale_x": eased * 0.46,
        "scale_y": eased * 0.38,
        "skew": eased * 0.42,
    }


def maybe_drop_points(segments, profile, rng):
    if profile["drop_chance"] <= 0:
        return

    for segment in segments:
        if segment["cmd"] == "Z" or len(segment["points"]) <= 1:
            continue
        if rng.random() >= profile["drop_chance"]:
            continue

        # Keep the segment structurally valid while occasionally simplifying a curve.
        index = rng.randrange(len(segment["points"]))
        segment["points"].pop(index)
        if segment["cmd"] == "C" and len(segment["points"]) == 2:
            segment["cmd"] = "Q"
        elif segment["cmd"] in ("S", "Q") and len(segment["points"]) == 1:
            segment["cmd"] = "L"


def apply_shared_point_shifts(segments, profile, rng):
    runs = [run for run in point_runs(segments) if len(run) >= 3]
    if not runs or rng.random() >= profile["shared_shift_chance"]:
        return

    group_count = 1 + int(profile["amount"] * 4)
    for _ in range(group_count):
        run = rng.choice(runs)
        radius = rng.randint(2, min(len(run), 4 + int(profile["amount"] * 8)))
        center = rng.randrange(len(run))
        distance = abs(rng.gauss(0.0, profile["shared_shift"] * 0.45))
        angle = rng.random() * math.tau
        dx = math.cos(angle) * distance
        dy = math.sin(angle) * distance
        for index, point in enumerate(run):
            offset = abs(index - center)
            if offset > radius:
                continue
            weight = 0.5 + 0.5 * math.cos(math.pi * offset / (radius + 1))
            point[0] += dx * weight
            point[1] += dy * weight


def randomize_segments(segments, profile, rng):
    if profile["amount"] <= 0:
        return

    x_min, y_min, x_max, y_max = bounds_for_segments(segments)
    cx = (x_min + x_max) / 2.0
    cy = (y_min + y_max) / 2.0

    if rng.random() < profile["transform_chance"]:
        sx = 1.0 + rng.uniform(-profile["scale_x"], profile["scale_x"])
        sy = 1.0 + rng.uniform(-profile["scale_y"], profile["scale_y"])
        skew_x = rng.uniform(-profile["skew"], profile["skew"])
        skew_y = rng.uniform(-profile["skew"], profile["skew"])
        angle = rng.uniform(-profile["rotation"], profile["rotation"])
        tx = rng.gauss(0.0, profile["translate_x"] * 0.5)
        tx += rng.gauss(0.0, profile["spacing"] * 0.4)
        ty = rng.gauss(0.0, profile["translate_y"] * 0.55)

        matrix = translation(cx + tx, cy + ty)
        matrix = multiply(matrix, rotation(angle))
        matrix = multiply(matrix, [1.0, math.tan(skew_y), math.tan(skew_x), 1.0, 0.0, 0.0])
        matrix = multiply(matrix, [sx, 0.0, 0.0, sy, 0.0, 0.0])
        matrix = multiply(matrix, translation(-cx, -cy))
        transform_segments(segments, matrix)

    maybe_drop_points(segments, profile, rng)
    apply_shared_point_shifts(segments, profile, rng)

    for segment in segments:
        for point in segment["points"]:
            if rng.random() < profile["point_chance"]:
                distance = abs(rng.gauss(0.0, profile["jitter"] * 0.32))
                angle = rng.random() * math.tau
                point[0] += math.cos(angle) * distance
                point[1] += math.sin(angle) * distance


def collect_glyphs(root):
    glyphs = {}
    for element in root.iter():
        if local_name(element.tag) != "g":
            continue
        glyph_id = element.get("id")
        if not glyph_id:
            continue
        paths = []
        for child in list(element):
            if local_name(child.tag) == "path" and child.get("d"):
                paths.append(child.get("d"))
        if paths:
            glyphs[glyph_id] = paths
    return glyphs


def collect_uses(root):
    uses = []
    for element in root.iter():
        if local_name(element.tag) != "use":
            continue
        href = element.get(qname(XLINK_NS, "href")) or element.get("href")
        if not href or not href.startswith("#"):
            continue
        uses.append(
            {
                "glyph_id": href[1:],
                "x": parse_float(element.get("x")),
                "y": parse_float(element.get("y")),
            }
        )
    return uses


def build_svg(root, glyphs, uses, rand_value, seed):
    rng = random.Random(seed)
    profile = mutation_profile(rand_value)
    out_root = ET.Element(
        qname(SVG_NS, "svg"),
        {
            "width": root.get("width", "0"),
            "height": root.get("height", "0"),
            "viewBox": root.get("viewBox", "0 0 0 0"),
        },
    )
    group = ET.SubElement(out_root, qname(SVG_NS, "g"), {"fill": "rgb(0%, 0%, 0%)", "fill-opacity": "1"})

    for index, use in enumerate(uses):
        for path_data in glyphs.get(use["glyph_id"], []):
            segments = parse_path(path_data)
            transform_segments(segments, translation(use["x"], use["y"]))
            randomize_segments(segments, profile, rng)
            ET.SubElement(
                group,
                qname(SVG_NS, "path"),
                {
                    "id": f"char-{index}",
                    "d": path_to_string(segments),
                },
            )

    return ET.ElementTree(out_root)


def main():
    args = parse_args()
    if args.rand < 0 or args.rand > 100:
        print("--rand must be between 0 and 100", file=sys.stderr)
        return 2

    tree = ET.parse(args.input)
    root = tree.getroot()
    glyphs = collect_glyphs(root)
    uses = collect_uses(root)
    if not glyphs or not uses:
        print("Input SVG did not contain Pango glyph definitions and uses.", file=sys.stderr)
        return 1

    output_tree = build_svg(root, glyphs, uses, args.rand, args.seed)
    if args.output:
        output_tree.write(args.output, encoding="utf-8", xml_declaration=True)
    else:
        ET.indent(output_tree, space="  ")
        output_tree.write(sys.stdout, encoding="unicode", xml_declaration=True)
        sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
