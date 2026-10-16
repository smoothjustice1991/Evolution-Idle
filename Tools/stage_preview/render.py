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
    """Rasterises the mesh with the game's lighting. Smooth triangles blend per-vertex colours (Gouraud)."""
    import numpy as np
    n = SIZE * SS
    img = np.zeros((n, n, 3), np.float32)
    img[:, :] = bg
    slots = slot_colors(p, s, bg)
    rim_col = lerp(s, [1, 1, 1], 0.3)
    light = norm([-0.45, 0.7, 0.55])
    half = norm([light[0], light[1], light[2] + 1.0])
    cy, sy = math.cos(yaw), math.sin(yaw)
    cp, sp = math.cos(PITCH), math.sin(PITCH)

    def rot(x, y, z):
        x, z = x * cy + z * sy, -x * sy + z * cy
        y, z = y * cp - z * sp, y * sp + z * cp
        return (x, y, z)

    view, vnorm = [], []
    for v in mesh["pos"]:
        view.append(rot(v[0], v[1], v[2]))
        vnorm.append(rot(v[4], v[5], v[6]) if len(v) >= 7 else (0.0, 0.0, 0.0))

    def shade(base, accent, nrm, depth):
        diff = max(0.0, sum(nrm[i] * light[i] for i in range(3)))
        if accent:
            col = [x * (0.85 + 0.3 * diff) for x in base]
        else:
            rim = (1.0 - max(0.0, nrm[2])) ** 3
            spec = max(0.0, sum(nrm[i] * half[i] for i in range(3))) ** 24 * 0.35
            col = [base[i] * (0.22 + 0.78 * diff) + rim_col[i] * rim * 0.55 + spec for i in range(3)]
            fz = min(max(0.5 + 0.5 * depth, 0.0), 1.0)
            col = [x * (0.6 + 0.4 * fz) for x in col]
        return lerp(bg, col, VISIBILITY)

    tris = []
    for t in mesh["tri"]:
        a, b, c, slot, two = t[:5]
        smooth = t[5] if len(t) > 5 else 0
        A, B, C = view[a], view[b], view[c]
        e1 = [B[i] - A[i] for i in range(3)]
        e2 = [C[i] - A[i] for i in range(3)]
        nn = [e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]]
        cen = [(A[i] + B[i] + C[i]) / 3 for i in range(3)]
        to_cam = [-cen[0], -cen[1], CAM_DIST - cen[2]]
        if sum(nn[i] * to_cam[i] for i in range(3)) <= 0:
            if not two:
                continue
            nn = [-x for x in nn]
        tris.append((cen[2], (a, b, c), norm(nn), slot, smooth))
    tris.sort(key=lambda t: t[0])

    scale = n * 0.42
    for depth, idx, fn, slot, smooth in tris:
        base, accent = slots[slot], slot == 2
        pts, cols = [], []
        for k in idx:
            v = view[k]
            kk = CAM_DIST / (CAM_DIST - v[2])
            pts.append((n / 2 + v[0] * kk * scale, n / 2 - v[1] * kk * scale))
            if smooth:
                vn = vnorm[k]
                if sum(vn[i] * fn[i] for i in range(3)) <= 0:
                    vn = fn
                cols.append(shade(base, accent, vn, v[2]))
        if not smooth:
            cols = [shade(base, accent, fn, depth)] * 3
        (x0, y0), (x1, y1), (x2, y2) = pts
        minx, maxx = int(max(0, math.floor(min(x0, x1, x2)))), int(min(n - 1, math.ceil(max(x0, x1, x2))))
        miny, maxy = int(max(0, math.floor(min(y0, y1, y2)))), int(min(n - 1, math.ceil(max(y0, y1, y2))))
        if minx > maxx or miny > maxy:
            continue
        den = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
        if abs(den) < 1e-9:
            continue
        ys, xs = np.mgrid[miny:maxy + 1, minx:maxx + 1].astype(np.float32) + 0.5
        w0 = ((y1 - y2) * (xs - x2) + (x2 - x1) * (ys - y2)) / den
        w1 = ((y2 - y0) * (xs - x2) + (x0 - x2) * (ys - y2)) / den
        w2 = 1 - w0 - w1
        m = (w0 >= -1e-4) & (w1 >= -1e-4) & (w2 >= -1e-4)
        if not m.any():
            continue
        c = np.array(cols, np.float32)
        px = w0[..., None] * c[0] + w1[..., None] * c[1] + w2[..., None] * c[2]
        region = img[miny:maxy + 1, minx:maxx + 1]
        region[m] = px[m]
    lin = np.clip(img, 0, 1)
    srgb = np.where(lin <= 0.0031308, lin * 12.92, 1.055 * np.power(lin, 1 / 2.4) - 0.055)
    out = Image.fromarray((srgb * 255 + 0.5).astype(np.uint8), "RGB")
    return out.resize((SIZE, SIZE), Image.LANCZOS)


def main():
    meshes = json.load(open(sys.argv[1]))
    data = json.load(open(sys.argv[2]))
    out = sys.argv[3]
    eras = {e["id"]: e for e in data["eras"]}
    stages = {s["id"]: s for s in data["stages"]}
    cols = 5
    rows = math.ceil(len(meshes) / cols)
    sheet = Image.new("RGB", (cols * SIZE, rows * (SIZE + 24)), (8, 10, 16))
    only = set(int(x) for x in sys.argv[4].split(",")) if len(sys.argv) > 4 else None
    if only:
        meshes = [m for m in meshes if m["stage"] in only]
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
