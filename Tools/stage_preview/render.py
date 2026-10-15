#!/usr/bin/env python3
"""Renders a contact sheet of the 20 stage models, using the same shading as SEvoStageModel.cpp.

Usage:
  g++ -std=c++17 -O2 -I ../../Source/EvolutionIdle/Private dump_meshes.cpp \
      ../../Source/EvolutionIdle/Private/UI/EvoStageMeshes.cpp -o dump_meshes
  ./dump_meshes > meshes.json
  python3 render.py meshes.json ../../Content/Data/GameData.json stages.png
"""
import json
import math
import sys

from PIL import Image, ImageDraw, ImageFont

SIZE = 300          # cell size in pixels
SS = 2              # supersampling
VISIBILITY = 0.9
PITCH = 0.22
CAM_DIST = 4.0


def hex_lin(h):
    h = h.strip("#")
    c = [int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c]


def lin_srgb(c):
    out = []
    for x in c:
        x = min(max(x, 0.0), 1.0)
        x = 12.92 * x if x <= 0.0031308 else 1.055 * x ** (1 / 2.4) - 0.055
        out.append(int(round(x * 255)))
    return tuple(out)


def lerp(a, b, t):
    return [a[i] + (b[i] - a[i]) * t for i in range(3)]


def norm(v):
    l = math.sqrt(sum(x * x for x in v)) or 1.0
    return [x / l for x in v]


def slot_colors(p, s, bg):
    white = [1.0, 1.0, 1.0]
    return [
        [x * 0.85 for x in p],                       # primary
        [x * 0.85 for x in s],                       # secondary
        lerp(p, white, 0.55),                        # accent (emissive)
        lerp(bg, p, 0.22),                           # dark
        [x * 0.55 for x in lerp(p, s, 0.5)],         # mid
    ]


def render(mesh, p, s, bg, yaw):
    img = Image.new("RGB", (SIZE * SS, SIZE * SS), lin_srgb(bg))
    d = ImageDraw.Draw(img)
    slots = slot_colors(p, s, bg)
    rim_col = lerp(s, [1, 1, 1], 0.3)
    light = norm([-0.45, 0.7, 0.55])
    half = norm([light[0], light[1], light[2] + 1.0])
    cy, sy = math.cos(yaw), math.sin(yaw)
    cp, sp = math.cos(PITCH), math.sin(PITCH)
    view = []
    for x, y, z, _f in mesh["pos"]:
        x, z = x * cy + z * sy, -x * sy + z * cy           # yaw
        y, z = y * cp - z * sp, y * sp + z * cp           # pitch
        view.append((x, y, z))
    scale = SIZE * SS * 0.42
    cx = cyy = SIZE * SS / 2
    tris = []
    for a, b, c, slot, two in mesh["tri"]:
        A, B, C = view[a], view[b], view[c]
        e1 = [B[i] - A[i] for i in range(3)]
        e2 = [C[i] - A[i] for i in range(3)]
        n = [e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]]
        cen = [(A[i] + B[i] + C[i]) / 3 for i in range(3)]
        to_cam = [-cen[0], -cen[1], CAM_DIST - cen[2]]
        facing = sum(n[i] * to_cam[i] for i in range(3))
        if facing <= 0:
            if not two:
                continue
            n = [-x for x in n]
        tris.append((cen[2], A, B, C, norm(n), slot))
    tris.sort(key=lambda t: t[0])
    for depth, A, B, C, n, slot in tris:
        base = slots[slot]
        diff = max(0.0, sum(n[i] * light[i] for i in range(3)))
        if slot == 2:
            col = [x * (0.85 + 0.3 * diff) for x in base]
        else:
            rim = (1.0 - max(0.0, n[2])) ** 3
            spec = max(0.0, sum(n[i] * half[i] for i in range(3))) ** 24 * 0.35
            col = [base[i] * (0.22 + 0.78 * diff) + rim_col[i] * rim * 0.55 + spec for i in range(3)]
            fz = min(max(0.5 + 0.5 * depth, 0.0), 1.0)
            col = [x * (0.6 + 0.4 * fz) for x in col]
        col = lerp(bg, col, VISIBILITY)
        pts = []
        for v in (A, B, C):
            k = CAM_DIST / (CAM_DIST - v[2])
            pts.append((cx + v[0] * k * scale, cyy - v[1] * k * scale))
        d.polygon(pts, fill=lin_srgb(col))
    return img.resize((SIZE, SIZE), Image.LANCZOS)


def main():
    meshes = json.load(open(sys.argv[1]))
    data = json.load(open(sys.argv[2]))
    out = sys.argv[3]
    eras = {e["id"]: e for e in data["eras"]}
    stages = {s["id"]: s for s in data["stages"]}
    cols = 5
    rows = math.ceil(len(meshes) / cols)
    sheet = Image.new("RGB", (cols * SIZE, rows * (SIZE + 24)), (8, 10, 16))
    draw = ImageDraw.Draw(sheet)
    for i, m in enumerate(meshes):
        st = stages[m["stage"]]
        era = eras[st["era"]]
        img = render(m, hex_lin(era["primaryColor"]), hex_lin(era["secondaryColor"]), hex_lin(era["backgroundColor"]), 0.65)
        x, y = (i % cols) * SIZE, (i // cols) * (SIZE + 24)
        sheet.paste(img, (x, y))
        draw.text((x + 8, y + SIZE + 5), f"{st['id']}. {st['name']}", fill=(200, 220, 240))
    sheet.save(out)


if __name__ == "__main__":
    main()
