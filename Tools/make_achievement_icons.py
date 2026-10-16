#!/usr/bin/env python3
"""Builds the Steam achievement icons (achieved + unachieved) for every achievement in GameData.json.

Stage achievements show the 3D model of the stage just entered; era achievements show the first stage of
that era in a gold frame; click achievements show one more mouse cursor per tier; automation and offline
achievements use simple symbols.

Usage (from the repo root):
  g++ -std=c++17 -O2 -I Source/EvolutionIdle/Private Tools/stage_preview/dump_meshes.cpp \
      Source/EvolutionIdle/Private/UI/EvoStageMeshes.cpp -o dump_meshes
  ./dump_meshes > meshes.json
  python3 Tools/make_achievement_icons.py meshes.json

Writes Marketing/Steam/Achievements/<API name>_achieved.jpg and _unachieved.jpg (64 x 64, for Steamworks),
256 x 256 PNG masters in Marketing/Steam/Achievements/masters/, a preview sheet
Marketing/Steam/Achievements/_preview.png, and 128 x 128 PNGs for the in-game panel in
Content/Achievements/<API name>.png and <API name>_locked.png.
"""
import json
import math
import os
import sys

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageFont

N = 256           # master size
SS = 3            # supersampling for drawing
OUT = "Marketing/Steam/Achievements/"
GAME_OUT = "Content/Achievements/"   # 128 px PNGs the game loads for its Achievements panel
DATA = "Content/Data/GameData.json"
GOLD = (255, 205, 90)


# ---------------------------------------------------------------------------
# Colour helpers (same conventions as Tools/stage_preview/render.py)
# ---------------------------------------------------------------------------

def hex_rgb(h):
    h = h.strip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def hex_lin(h):
    return [x / 255 / 12.92 if x / 255 <= 0.04045 else ((x / 255 + 0.055) / 1.055) ** 2.4 for x in hex_rgb(h)]


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
    length = math.sqrt(sum(x * x for x in v)) or 1.0
    return [x / length for x in v]


def mix(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


# ---------------------------------------------------------------------------
# Stage model render (transparent background, same shading as the game)
# ---------------------------------------------------------------------------

def render_model(mesh, p, s, bg, yaw=0.65, pitch=0.22, size=N * SS):
    """Transparent render with the game's lighting; smooth triangles blend per-vertex colours (Gouraud)."""
    import numpy as np
    img = np.zeros((size, size, 4), np.float32)
    white = [1.0, 1.0, 1.0]
    slots = [[x * 0.95 for x in p], [x * 0.95 for x in s], lerp(p, white, 0.6), lerp(bg, p, 0.3),
             [x * 0.65 for x in lerp(p, s, 0.5)]]
    rim_col = lerp(s, white, 0.3)
    light = norm([-0.45, 0.7, 0.55])
    half = norm([light[0], light[1], light[2] + 1.0])
    cy, sy = math.cos(yaw), math.sin(yaw)
    cp, sp = math.cos(pitch), math.sin(pitch)
    cam = 4.0

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
            return [x * (0.9 + 0.3 * diff) for x in base]
        rim = (1.0 - max(0.0, nrm[2])) ** 3
        spec = max(0.0, sum(nrm[i] * half[i] for i in range(3))) ** 24 * 0.4
        col = [base[i] * (0.3 + 0.8 * diff) + rim_col[i] * rim * 0.6 + spec for i in range(3)]
        fz = min(max(0.5 + 0.5 * depth, 0.0), 1.0)
        return [x * (0.7 + 0.3 * fz) for x in col]

    tris = []
    for t in mesh["tri"]:
        a, b, c, slot, two = t[:5]
        smooth = t[5] if len(t) > 5 else 0
        A, B, C = view[a], view[b], view[c]
        e1 = [B[i] - A[i] for i in range(3)]
        e2 = [C[i] - A[i] for i in range(3)]
        n = [e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]]
        cen = [(A[i] + B[i] + C[i]) / 3 for i in range(3)]
        to_cam = [-cen[0], -cen[1], cam - cen[2]]
        if sum(n[i] * to_cam[i] for i in range(3)) <= 0:
            if not two:
                continue
            n = [-x for x in n]
        tris.append((cen[2], (a, b, c), norm(n), slot, smooth))
    tris.sort(key=lambda t: t[0])

    scale = size * 0.42
    for depth, idx, fn, slot, smooth in tris:
        base, accent = slots[slot], slot == 2
        pts, cols = [], []
        for k in idx:
            v = view[k]
            kk = cam / (cam - v[2])
            pts.append((size / 2 + v[0] * kk * scale, size / 2 - v[1] * kk * scale))
            if smooth:
                vn = vnorm[k]
                if sum(vn[i] * fn[i] for i in range(3)) <= 0:
                    vn = fn
                cols.append(shade(base, accent, vn, v[2]))
        if not smooth:
            cols = [shade(base, accent, fn, depth)] * 3
        (x0, y0), (x1, y1), (x2, y2) = pts
        minx, maxx = int(max(0, math.floor(min(x0, x1, x2)))), int(min(size - 1, math.ceil(max(x0, x1, x2))))
        miny, maxy = int(max(0, math.floor(min(y0, y1, y2)))), int(min(size - 1, math.ceil(max(y0, y1, y2))))
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
        region[m, :3] = px[m]
        region[m, 3] = 1.0
    lin = np.clip(img[..., :3], 0, 1)
    srgb = np.where(lin <= 0.0031308, lin * 12.92, 1.055 * np.power(lin, 1 / 2.4) - 0.055)
    rgba = np.dstack([srgb, img[..., 3:4]])
    return Image.fromarray((rgba * 255 + 0.5).astype(np.uint8), "RGBA")


def fit(img, box_frac):
    """Crops to the drawn pixels and scales to fill box_frac of the master icon."""
    bbox = img.getchannel("A").getbbox()
    img = img.crop(bbox)
    target = N * SS * box_frac
    s = target / max(img.width, img.height)
    return img.resize((max(1, int(img.width * s)), max(1, int(img.height * s))), Image.LANCZOS)


# ---------------------------------------------------------------------------
# Frame and background
# ---------------------------------------------------------------------------

def base_icon(primary, secondary, background, gold=False):
    S = N * SS
    bg = Image.new("RGB", (S, S), background)
    # radial glow in the era colour
    glow = Image.new("L", (S, S), 0)
    ImageDraw.Draw(glow).ellipse([S * 0.12, S * 0.12, S * 0.88, S * 0.88], fill=255)
    glow = glow.filter(ImageFilter.GaussianBlur(S * 0.16))
    tint = Image.new("RGB", (S, S), mix(background, primary, 0.38))
    bg = Image.composite(tint, bg, glow)
    img = bg.convert("RGBA")

    frame_col = GOLD if gold else primary
    layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    inset = S * 0.045
    radius = S * 0.16
    d.rounded_rectangle([inset, inset, S - inset, S - inset], radius=radius, outline=frame_col + (255,), width=int(S * (0.05 if gold else 0.035)))
    blur = layer.filter(ImageFilter.GaussianBlur(S * 0.03))
    img.alpha_composite(blur)
    img.alpha_composite(layer)
    if gold:
        inner = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        i2 = inset + S * 0.075
        ImageDraw.Draw(inner).rounded_rectangle([i2, i2, S - i2, S - i2], radius=radius * 0.7,
                                                outline=(255, 235, 170, 200), width=int(S * 0.012))
        img.alpha_composite(inner)
    return img


def add_glow(img, sprite, pos, colour, strength=0.75, radius=0.035):
    S = N * SS
    halo = Image.new("RGBA", sprite.size, colour + (0,))
    halo.putalpha(sprite.getchannel("A").point(lambda a: int(a * strength)))
    layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    layer.alpha_composite(halo, pos)
    img.alpha_composite(layer.filter(ImageFilter.GaussianBlur(S * radius)))
    img.alpha_composite(sprite, pos)


def corner_badge(img, text, fill, text_col=(10, 14, 24)):
    """Small rounded tag in the bottom-right corner (era number, 8h)."""
    S = N * SS
    d = ImageDraw.Draw(img)
    font = load_font(int(S * 0.15))
    tw = d.textlength(text, font=font)
    w, h = tw + S * 0.09, S * 0.2
    x1, y1 = S * 0.93, S * 0.93
    d.rounded_rectangle([x1 - w, y1 - h, x1, y1], radius=h * 0.35, fill=fill + (255,), outline=(255, 255, 255, 120), width=int(S * 0.008))
    d.text((x1 - w / 2, y1 - h / 2), text, font=font, fill=text_col, anchor="mm")


def load_font(px):
    for path in ("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
                 "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"):
        if os.path.exists(path):
            return ImageFont.truetype(path, px)
    return ImageFont.load_default()


# ---------------------------------------------------------------------------
# Symbols
# ---------------------------------------------------------------------------

CURSOR = [(0, 0), (0, 16.5), (4.2, 12.6), (7.1, 19.2), (9.8, 18.0), (7.0, 11.6), (12.6, 11.6)]


def cursor_sprite(height, fill=(255, 255, 255), outline=(10, 16, 30)):
    s = height / 19.2
    pad = int(height * 0.12)
    pts = [(pad + x * s, pad + y * s) for x, y in CURSOR]
    w = int(12.6 * s + pad * 2)
    h = int(19.2 * s + pad * 2)
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.polygon(pts, fill=fill + (255,))
    d.line(pts + [pts[0]], fill=outline + (255,), width=max(2, int(height * 0.06)), joint="curve")
    return img


def click_ripple(img, centre, r, colour):
    d = ImageDraw.Draw(img)
    S = N * SS
    for k, rr in enumerate((r, r * 1.6)):
        a = 230 if k == 0 else 120
        d.ellipse([centre[0] - rr, centre[1] - rr, centre[0] + rr, centre[1] + rr], outline=colour + (a,), width=int(S * 0.014))


def gear_sprite(radius, teeth=8, fill=(220, 240, 255)):
    size = int(radius * 2.6)
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = size / 2
    pts = []
    for i in range(teeth * 4):
        a = i / (teeth * 4) * math.tau
        r = radius * (1.18 if (i % 4) in (1, 2) else 0.92)
        pts.append((c + math.cos(a) * r, c + math.sin(a) * r))
    d.polygon(pts, fill=fill + (255,))
    hr = radius * 0.38
    d.ellipse([c - hr, c - hr, c + hr, c + hr], fill=(0, 0, 0, 0))
    # punch the hole
    alpha = img.getchannel("A")
    ImageDraw.Draw(alpha).ellipse([c - hr, c - hr, c + hr, c + hr], fill=0)
    img.putalpha(alpha)
    return img


def chevron_stack(width, count, colours):
    h_one = width * 0.42
    img = Image.new("RGBA", (int(width * 1.1), int(h_one * (count * 0.72 + 0.6))), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    t = width * 0.16
    for i in range(count):
        y = img.height - h_one * 0.9 - i * h_one * 0.72
        x0 = width * 0.05
        pts = [(x0, y + h_one * 0.6), (x0 + width / 2, y), (x0 + width, y + h_one * 0.6),
               (x0 + width, y + h_one * 0.6 + t), (x0 + width / 2, y + t), (x0, y + h_one * 0.6 + t)]
        d.polygon(pts, fill=colours[i % len(colours)] + (255,))
    return img


def moon_sprite(radius, fill=(255, 236, 170)):
    size = int(radius * 2.2)
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = size / 2
    d.ellipse([c - radius, c - radius, c + radius, c + radius], fill=fill + (255,))
    alpha = img.getchannel("A")
    off = radius * 0.55
    ImageDraw.Draw(alpha).ellipse([c - radius + off, c - radius - off * 0.45, c + radius + off, c + radius - off * 0.45], fill=0)
    img.putalpha(alpha)
    return img


# ---------------------------------------------------------------------------
# Icon builders
# ---------------------------------------------------------------------------

def stage_icon(mesh, era, gold=False, badge=None):
    p, s, b = hex_rgb(era["primaryColor"]), hex_rgb(era["secondaryColor"]), hex_rgb(era["backgroundColor"])
    img = base_icon(p, s, b, gold)
    model = fit(render_model(mesh, hex_lin(era["primaryColor"]), hex_lin(era["secondaryColor"]), hex_lin(era["backgroundColor"])),
                0.66 if gold else 0.72)
    S = N * SS
    pos = ((S - model.width) // 2, (S - model.height) // 2 + int(S * 0.01))
    add_glow(img, model, pos, s, 0.6, 0.04)
    if badge:
        corner_badge(img, badge, GOLD)
    return img


def clicks_icon(tier, era):
    p, s, b = hex_rgb(era["primaryColor"]), hex_rgb(era["secondaryColor"]), hex_rgb(era["backgroundColor"])
    img = base_icon(p, s, b)
    S = N * SS
    # Cursor layouts for 1-4 cursors: (x, y, height) as fractions of the icon.
    layouts = {
        1: [(0.40, 0.30, 0.50)],
        2: [(0.24, 0.30, 0.42), (0.54, 0.40, 0.42)],
        3: [(0.18, 0.22, 0.36), (0.52, 0.22, 0.36), (0.35, 0.50, 0.36)],
        4: [(0.17, 0.18, 0.32), (0.53, 0.18, 0.32), (0.17, 0.52, 0.32), (0.53, 0.52, 0.32)],
    }[tier]
    for x, y, h in layouts:
        click_ripple(img, (S * x, S * y), S * h * 0.14, s)
    for x, y, h in layouts:
        c = cursor_sprite(S * h, fill=(245, 252, 255))
        pad = int(S * h * 0.12)
        add_glow(img, c, (int(S * x) - pad, int(S * y) - pad), p, 0.8, 0.025)
    return img


def auto_icon(gears, era):
    p, s, b = hex_rgb(era["primaryColor"]), hex_rgb(era["secondaryColor"]), hex_rgb(era["backgroundColor"])
    img = base_icon(p, s, b)
    S = N * SS
    if gears == 1:
        g = gear_sprite(S * 0.22, fill=mix(p, (255, 255, 255), 0.55))
        add_glow(img, g, ((S - g.width) // 2 - int(S * 0.06), (S - g.height) // 2 - int(S * 0.04)), p)
        c = cursor_sprite(S * 0.34, fill=(245, 252, 255))
        add_glow(img, c, (int(S * 0.50), int(S * 0.46)), p, 0.8, 0.02)
    else:
        specs = [(0.20, 0.36, 0.40, p), (0.13, 0.66, 0.30, s), (0.11, 0.30, 0.70, s)]
        for r, cx, cy, col in specs:
            g = gear_sprite(S * r, teeth=8 if r > 0.15 else 7, fill=mix(col, (255, 255, 255), 0.5))
            add_glow(img, g, (int(S * cx - g.width / 2), int(S * cy - g.height / 2)), col, 0.6, 0.025)
        c = cursor_sprite(S * 0.30, fill=(245, 252, 255))
        add_glow(img, c, (int(S * 0.55), int(S * 0.52)), p, 0.8, 0.02)
    return img


def all_upgrades_icon(eras):
    e = eras[5]
    p, s, b = hex_rgb(e["primaryColor"]), hex_rgb(e["secondaryColor"]), hex_rgb(e["backgroundColor"])
    img = base_icon(p, s, b, gold=True)
    S = N * SS
    colours = [hex_rgb(eras[i]["primaryColor"]) for i in range(1, 6)]
    ch = chevron_stack(S * 0.5, 5, colours)
    add_glow(img, ch, ((S - ch.width) // 2, (S - ch.height) // 2), GOLD, 0.6, 0.03)
    return img


def offline_icon(eras, cap_hours):
    e = eras[4]
    p, s, b = hex_rgb(e["primaryColor"]), hex_rgb(e["secondaryColor"]), hex_rgb(e["backgroundColor"])
    img = base_icon(p, s, b)
    S = N * SS
    d = ImageDraw.Draw(img)
    for x, y, r in ((0.25, 0.25, 0.012), (0.72, 0.22, 0.009), (0.78, 0.48, 0.011), (0.30, 0.70, 0.008), (0.62, 0.74, 0.007)):
        d.ellipse([S * (x - r), S * (y - r), S * (x + r), S * (y + r)], fill=(255, 255, 255, 220))
    m = moon_sprite(S * 0.27)
    add_glow(img, m, ((S - m.width) // 2 - int(S * 0.03), (S - m.height) // 2 - int(S * 0.04)), (255, 225, 140), 0.7, 0.04)
    corner_badge(img, f"{cap_hours:g}h", (255, 226, 122))
    return img


def unachieved(img):
    g = img.convert("L").convert("RGB")
    g = ImageEnhance.Brightness(g).enhance(0.55)
    return ImageEnhance.Contrast(g).enhance(0.85)


# ---------------------------------------------------------------------------

def main():
    meshes = {m["stage"]: m for m in json.load(open(sys.argv[1]))}
    data = json.load(open(DATA, encoding="utf-8"))
    eras = {e["id"]: e for e in data["eras"]}
    stages = {s["id"]: s for s in data["stages"]}
    cap = data["economy"].get("offlineCapHours", 8)
    os.makedirs(OUT + "masters", exist_ok=True)
    os.makedirs(GAME_OUT, exist_ok=True)

    click_tiers = sorted(a["target"] for a in data["achievements"] if a["type"] == "clicks")
    auto_tiers = sorted(a["target"] for a in data["achievements"] if a["type"] == "autoClickers")
    made = []
    for a in data["achievements"]:
        t, target = a["type"], int(a["target"])
        if t == "stage":
            icon = stage_icon(meshes[target], eras[stages[target]["era"]])
        elif t == "eraComplete":
            first = min(sid for sid, st in stages.items() if st["era"] == target)
            icon = stage_icon(meshes[first], eras[target], gold=True, badge=["I", "II", "III", "IV", "V"][target - 1])
        elif t == "clicks":
            tier = click_tiers.index(target) + 1
            icon = clicks_icon(min(tier, 4), eras[1])
        elif t == "autoClickers":
            icon = auto_icon(1 if auto_tiers.index(target) == 0 else 3, eras[2])
        elif t == "allUpgrades":
            icon = all_upgrades_icon(eras)
        elif t == "offlineFull":
            icon = offline_icon(eras, cap)
        else:
            continue
        master = icon.convert("RGB").resize((N, N), Image.LANCZOS)
        master.save(f"{OUT}masters/{a['id']}_achieved.png")
        unachieved(master).save(f"{OUT}masters/{a['id']}_unachieved.png")
        small = master.resize((64, 64), Image.LANCZOS).filter(ImageFilter.UnsharpMask(radius=1, percent=60, threshold=2))
        small.save(f"{OUT}{a['id']}_achieved.jpg", quality=95)
        unachieved(small).save(f"{OUT}{a['id']}_unachieved.jpg", quality=95)
        game = master.resize((128, 128), Image.LANCZOS)
        game.save(f"{GAME_OUT}{a['id']}.png", optimize=True)
        unachieved(game).save(f"{GAME_OUT}{a['id']}_locked.png", optimize=True)
        made.append((a, master))
        print("made", a["id"])

    # Preview sheet: each icon at 128 px with its name, plus 64 px achieved/unachieved as Steam shows them.
    cols = 6
    cell_w, cell_h = 224, 200
    rows = math.ceil(len(made) / cols)
    sheet = Image.new("RGB", (cols * cell_w, rows * cell_h + 50), (10, 12, 18))
    d = ImageDraw.Draw(sheet)
    title = load_font(22)
    small_font = load_font(12)
    d.text((16, 14), "Evolution Idle achievement icons: large preview, then 64 px achieved and unachieved", font=title, fill=(220, 235, 250))
    for i, (a, master) in enumerate(made):
        x, y = (i % cols) * cell_w, 50 + (i // cols) * cell_h
        sheet.paste(master.resize((128, 128), Image.LANCZOS), (x + 8, y + 8))
        sheet.paste(Image.open(f"{OUT}{a['id']}_achieved.jpg"), (x + 146, y + 8))
        sheet.paste(Image.open(f"{OUT}{a['id']}_unachieved.jpg"), (x + 146, y + 76))
        name = a["name"] if len(a["name"]) <= 26 else a["name"][:25] + "…"
        d.text((x + 8, y + 142), name, font=small_font, fill=(230, 240, 250))
        d.text((x + 8, y + 160), a["id"], font=small_font, fill=(120, 140, 160))
    sheet.save(OUT + "_preview.png")


if __name__ == "__main__":
    main()
