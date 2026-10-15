#!/usr/bin/env python3
"""Builds the "EI" Steam icons from the game logo and library capsule art.

Usage: python3 Tools/make_icons.py <logo.png (1280x733 original)>
Writes Marketing/Steam/shortcut_icon_256x256.png, app_icon_184x184.jpg and icon_master_1024.png.
"""
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter
from scipy import ndimage

N = 1024
OUT = "Marketing/Steam/"
LETTER_WIDTH = 0.68  # fraction of the icon width taken by "EI"


def letter(logo, box):
    """Cuts one letter out of the logo, dropping the dark shield fill behind it."""
    a = np.array(logo.crop(box)).astype(np.float32)
    lum = a[:, :, :3].max(axis=2)
    m = np.clip((lum - 60) / 50, 0, 1) * (a[:, :, 3] / 255)
    lab, n = ndimage.label(m > 0.3)
    sizes = ndimage.sum(np.ones_like(m), lab, range(1, n + 1))
    keep = ndimage.binary_dilation(lab == (1 + int(np.argmax(sizes))), iterations=3)
    a[:, :, 3] = m * keep * 255
    img = Image.fromarray(a.astype(np.uint8), "RGBA")
    return img.crop(img.getchannel("A").point(lambda v: 255 if v > 40 else 0).getbbox())


def word_ei(logo):
    e = letter(logo, (792, 398, 985, 648))   # "E" from IDLE
    i = letter(logo, (310, 392, 428, 648))   # "I" from IDLE
    gap = 24
    h = max(e.height, i.height)
    word = Image.new("RGBA", (e.width + gap + i.width, h), (0, 0, 0, 0))
    word.alpha_composite(e, (0, h - e.height))
    word.alpha_composite(i, (e.width + gap, h - i.height))
    return word


def background(rounded):
    art = Image.open(OUT + "library_capsule_600x900.png").convert("RGB")
    art = art.crop((0, 560, 600, 900)).resize((N, N), Image.LANCZOS).filter(ImageFilter.GaussianBlur(10))
    art = ImageEnhance.Brightness(art).enhance(0.55)
    y, x = np.mgrid[0:N, 0:N]
    k = np.clip(1.0 - np.sqrt((x - N / 2) ** 2 + (y - N / 2) ** 2) / (N / 2), 0, 1)[..., None]
    a = np.array(art, np.float32) * (1 - 0.75 * k) + np.array([6, 18, 58], np.float32) * (0.75 * k)
    img = Image.fromarray(a.astype(np.uint8), "RGB").convert("RGBA")
    mask = Image.new("L", (N, N), 0)
    if rounded:
        ImageDraw.Draw(mask).rounded_rectangle([8, 8, N - 9, N - 9], radius=190, fill=255)
    else:
        ImageDraw.Draw(mask).rectangle([0, 0, N, N], fill=255)
    img.putalpha(mask)
    inset = 36 if rounded else 28
    box = [inset, inset, N - 1 - inset, N - 1 - inset]
    frame = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    ImageDraw.Draw(frame).rounded_rectangle(box, radius=160 if rounded else 120, outline=(60, 190, 255, 255), width=22)
    glow = frame.filter(ImageFilter.GaussianBlur(22))
    img.alpha_composite(glow)
    img.alpha_composite(glow)
    inner = [box[0] + 8, box[1] + 8, box[2] - 8, box[3] - 8]
    ImageDraw.Draw(frame).rounded_rectangle(inner, radius=150 if rounded else 112, outline=(200, 240, 255, 255), width=5)
    img.alpha_composite(frame)
    return img


def place(img, word):
    s = N * LETTER_WIDTH / word.width
    w = word.resize((int(word.width * s), int(word.height * s)), Image.LANCZOS)
    x, y = (N - w.width) // 2, (N - w.height) // 2 + 8
    halo = Image.new("RGBA", w.size, (40, 170, 255, 0))
    halo.putalpha(w.getchannel("A"))
    glow = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    glow.alpha_composite(halo, (x, y))
    glow = glow.filter(ImageFilter.GaussianBlur(28))
    img.alpha_composite(glow)
    img.alpha_composite(glow)
    img.alpha_composite(w, (x, y))
    return img


def main():
    logo = Image.open(sys.argv[1]).convert("RGBA")
    word = word_ei(logo)
    shortcut = place(background(True), word)
    shortcut.save(OUT + "icon_master_1024.png", optimize=True)
    shortcut.resize((256, 256), Image.LANCZOS).save(OUT + "shortcut_icon_256x256.png", optimize=True)
    place(background(False), word).convert("RGB").resize((184, 184), Image.LANCZOS).save(OUT + "app_icon_184x184.jpg", quality=95)


if __name__ == "__main__":
    main()
