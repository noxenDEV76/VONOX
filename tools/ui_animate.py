"""VONOX UI preview renderer (original "machine" design).

Builds the static artwork once, then renders animation frames:
  - animated flames + rising embers behind the VONOX and AUTO MIX lettering
  - red display with drifting particles, a breathing glow and a pulsing readout
  - light sweep across all metal lettering
Usage: python3 tools/ui_animate.py OUT_DIR [FRAMES]
The real plugin UI is drawn by JUCE; this script is the art/animation reference.
"""
import math, random, sys, os
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageChops

EXPORT = bool(os.environ.get("VONOX_EXPORT"))   # asset-export mode: no value arcs / knob bodies / meter segments
S = 2
W, H = 1100, 700
CW, CH = W * S, H * S
RED, VIOLET, WHITE = (70, 150, 255), (170, 85, 255), (240, 240, 250)      # RED = electric blue accent (name kept), VIOLET = galaxy violet
MAGENTA, CYAN = (255, 70, 190), (90, 225, 255)
BG_VAL = 255 if os.environ.get("VONOX_BG") == "white" else 0   # export runs twice (black / white backdrop) to recover a transparent panel layer
FB = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf"
FH = "/usr/share/fonts/opentype/inter/Inter-Black.otf"
if not os.path.exists(FH): FH = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
def font(sz, path=FB): return ImageFont.truetype(path, int(sz * S))
random.seed(7); np.random.seed(7)

# ---------------------------------------------------------------- noise + master metal
def noise_layer(w, h, sx, sy, seed):
    rng = np.random.RandomState(seed)
    small = rng.rand(max(2, h // sy) + 2, max(2, w // sx) + 2).astype(np.float32)
    return np.asarray(Image.fromarray((small * 255).astype(np.uint8)).resize((w, h), Image.BICUBIC), np.float32) / 255

def pnoise(w, hs, sx, sy, seed):
    """Noise that is exactly periodic vertically (period = hs*sy rows) so animations loop."""
    rng = np.random.RandomState(seed)
    small = rng.rand(hs, max(2, w // sx) + 2).astype(np.float32)
    ext = np.vstack([small, small[:2]])
    im = Image.fromarray((ext * 255).astype(np.uint8)).resize((w, (hs + 2) * sy), Image.BICUBIC)
    return np.asarray(im, np.float32)[sy: sy + hs * sy] / 255

def make_master():
    """Big scratched, brushed, cloudy steel sheet (average value ~60)."""
    rng = np.random.RandomState(3); r = random.Random(11)
    base = np.full((CH, CW), 60, np.float32)
    base += rng.normal(0, 3.0, (CH, 1)).astype(np.float32)
    base += (noise_layer(CW, CH, 140, 1, 5) - 0.5) * 16
    for sx, sy, amp, sd in ((320, 320, 17, 1), (100, 100, 11, 2), (34, 34, 6, 4)):
        base += (noise_layer(CW, CH, sx, sy, sd) - 0.5) * amp * 2
    base += rng.normal(0, 2.4, (CH, CW)).astype(np.float32)
    light = Image.new("L", (CW, CH), 0); dark = Image.new("L", (CW, CH), 0)
    ld, dd = ImageDraw.Draw(light), ImageDraw.Draw(dark)
    for _ in range(2600):
        x, y = r.uniform(0, CW), r.uniform(0, CH); L = r.uniform(12, 300)
        a = r.uniform(-0.45, 0.45) if r.random() < 0.55 else r.uniform(0, math.pi)
        (ld if r.random() < 0.6 else dd).line([(x, y), (x + math.cos(a) * L, y + math.sin(a) * L)], fill=r.randint(30, 120), width=1)
    for _ in range(120):
        x, y = r.uniform(0, CW), r.uniform(0, CH); L = r.uniform(25, 170); a = r.uniform(0, math.pi)
        (ld if r.random() < 0.5 else dd).line([(x, y), (x + math.cos(a) * L, y + math.sin(a) * L)], fill=r.randint(90, 190), width=r.choice([2, 2, 3]))
    for _ in range(1100):
        x, y = r.uniform(0, CW), r.uniform(0, CH); rad = r.uniform(0.8, 2.6)
        dd.ellipse([x - rad, y - rad, x + rad, y + rad], fill=r.randint(60, 150))
    light = light.filter(ImageFilter.GaussianBlur(0.55)); dark = dark.filter(ImageFilter.GaussianBlur(0.55))
    base += np.asarray(light, np.float32) * 0.5 - np.asarray(dark, np.float32) * 0.5
    return np.clip(base, 5, 255)

MASTER = make_master()
canvas = Image.new("RGB", (CW, CH), (BG_VAL, BG_VAL, BG_VAL))             # plain backdrop: the plugin draws the animated star field itself
SHINE = Image.new("L", (CW, CH), 0)           # accumulated mask of all metal lettering

# ---------------------------------------------------------------- drawing helpers
def lit(mask, base=118, bevel=8, k=2.4, tint=(1, 1, 1.03), invert=False, light=(-0.7, -0.7), mul=1.0):
    """Paste scratched steel into `mask` with real height-map lighting (3D bevel)."""
    bb = mask.getbbox()
    if not bb: return
    pad = int(bevel * S * 3)
    box = (max(0, bb[0] - pad), max(0, bb[1] - pad), min(CW, bb[2] + pad), min(CH, bb[3] + pad))
    m = mask.crop(box)
    hgt = np.asarray(m.filter(ImageFilter.GaussianBlur(bevel * S)), np.float32) / 255.0 * bevel * S
    gy, gx = np.gradient(hgt)
    s = -(gx * light[0] + gy * light[1]) / max(1e-6, bevel * S * 0.18)
    s = -s if invert else s
    tex = MASTER[box[1]:box[3], box[0]:box[2]] * (base / 60.0)
    col = tex[..., None] * np.array(tint, np.float32) * np.maximum(0.62 + k * 0.5 * s[..., None], 0.3) * mul
    region = canvas.crop(box); region.paste(Image.fromarray(np.clip(col, 0, 255).astype(np.uint8)), (0, 0), m)
    canvas.paste(region, box[:2])

def poly_mask(pts):
    m = Image.new("L", canvas.size, 0)
    ImageDraw.Draw(m).polygon([(x * S, y * S) for x, y in pts], fill=255); return m

def oct_pts(x, y, w, h, cut):
    return [(x + cut, y), (x + w - cut, y), (x + w, y + cut), (x + w, y + h - cut),
            (x + w - cut, y + h), (x + cut, y + h), (x, y + h - cut), (x, y + cut)]

def glow(fn, color, radius, strength=1.0):
    g = Image.new("RGBA", canvas.size, (0, 0, 0, 0)); fn(ImageDraw.Draw(g), color)
    bb = g.getbbox()
    if not bb: return
    pad = int(radius * S * 3)
    box = (max(0, bb[0] - pad), max(0, bb[1] - pad), min(CW, bb[2] + pad), min(CH, bb[3] + pad))
    gc = g.crop(box).filter(ImageFilter.GaussianBlur(radius * S))
    if strength != 1.0: gc.putalpha(gc.getchannel("A").point(lambda v: min(255, int(v * strength))))
    canvas.paste(gc, box[:2], gc)

def D(): return ImageDraw.Draw(canvas, "RGBA")

def dropshadow(pts, off=6, blur=8, alpha=200):
    g = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ImageDraw.Draw(g).polygon([((x + off) * S, (y + off) * S) for x, y in pts], fill=(0, 0, 0, alpha))
    bb = g.getbbox()
    if not bb: return
    pad = int(blur * S * 3)
    box = (max(0, bb[0] - pad), max(0, bb[1] - pad), min(CW, bb[2] + pad), min(CH, bb[3] + pad))
    gc = g.crop(box).filter(ImageFilter.GaussianBlur(blur * S)); canvas.paste(gc, box[:2], gc)

def screw(x, y, r=5):
    x *= S; y *= S; R = r * S
    k = Image.new("L", canvas.size, 0); ImageDraw.Draw(k).ellipse([x - R, y - R, x + R, y + R], fill=255)
    lit(k, base=150, bevel=r * 0.5, k=3.0)
    d = D(); d.ellipse([x - R, y - R, x + R, y + R], outline=(5, 5, 8, 255), width=S)
    a = random.random() * math.pi
    d.line([x - math.cos(a) * R * .7, y - math.sin(a) * R * .7, x + math.cos(a) * R * .7, y + math.sin(a) * R * .7], fill=(8, 8, 10, 255), width=S + 1)

def anchor(x, y, w=16, h=26):
    pts = [(x - w / 2, y - h / 2), (x + w / 2, y - h / 2), (x + w / 2, y + h / 2), (x - w / 2, y + h / 2)]
    dropshadow(pts, 3, 3, 210); lit(poly_mask(pts), base=125, bevel=3, k=2.6)
    D().line([(px * S, py * S) for px, py in pts + [pts[0]]], fill=(3, 3, 6, 255), width=int(1.6 * S))
    screw(x, y - h / 4, 2.8); screw(x, y + h / 4, 2.8)

# ---------------------------------------------------------------- V-style metal lettering
def metal_label(c, text, size, active=True, anchor_="mm", glow_col=RED, glow_str=0.8, base=205):
    """Heavy bevelled steel letters with dark drop shadow + thin coloured glow (same look as the V mark)."""
    x, y = c[0] * S, c[1] * S; ft = font(size, FH)
    m = Image.new("L", canvas.size, 0); ImageDraw.Draw(m).text((x, y), text, font=ft, fill=255, anchor=anchor_)
    if glow_col:
        glow(lambda g, cc: g.text((x, y), text, font=ft, fill=cc + (255,), anchor=anchor_, stroke_width=int(1.4 * S)),
             glow_col, 2.0 if size < 22 else 6.0, glow_str)
    D().text((x + 1.6 * S, y + 2.4 * S), text, font=ft, fill=(0, 0, 0, 235), anchor=anchor_)
    lit(m, base=base if active else base * 0.62, bevel=max(0.9, size * 0.045), k=2.3, tint=(1, 0.93, 0.97))
    SHINE.paste(255, mask=m)
    return m

# ---------------------------------------------------------------- chain (military style)
def ell(cx, cy, a, b, ang, n=40):
    ca, sa = math.cos(ang), math.sin(ang)
    return [(cx + a * math.cos(t) * ca - b * math.sin(t) * sa, cy + a * math.cos(t) * sa + b * math.sin(t) * ca)
            for t in [2 * math.pi * i / n for i in range(n)]]

def chain(p0, p1, sag, link=26, wire=6.0):
    N = 500
    pts = [(p0[0] + (p1[0] - p0[0]) * t, p0[1] + (p1[1] - p0[1]) * t + sag * 4 * t * (1 - t)) for t in [i / N for i in range(N + 1)]]
    cum = [0.0]
    for i in range(1, len(pts)): cum.append(cum[-1] + math.dist(pts[i - 1], pts[i]))
    pitch = link * 0.74; items = []; j = 0
    for i in range(int(cum[-1] / pitch) + 1):
        s = i * pitch
        while j < len(pts) - 2 and cum[j] < s: j += 1
        a = pts[max(0, j - 1)]; b = pts[min(len(pts) - 1, j + 1)]
        items.append((i, pts[j], math.atan2(b[1] - a[1], b[0] - a[0])))
    for i, pos, ang in items:
        dropshadow(ell(pos[0], pos[1], link / 2, link * 0.3 if i % 2 == 0 else wire * 0.6, ang), 4, 3, 190)
    for i, pos, ang in items:
        if i % 2 == 1:
            lit(poly_mask(ell(pos[0], pos[1], link / 2, wire * 0.62, ang)), base=105, bevel=2.2, k=2.6)
    for i, pos, ang in items:
        if i % 2 == 0:
            m = Image.new("L", canvas.size, 0); md = ImageDraw.Draw(m)
            outer = ell(pos[0], pos[1], link / 2, link * 0.3, ang)
            md.polygon([(x * S, y * S) for x, y in outer], fill=255)
            md.polygon([(x * S, y * S) for x, y in ell(pos[0], pos[1], link / 2 - wire, link * 0.3 - wire, ang)], fill=0)
            lit(m, base=125, bevel=wire * 0.5, k=2.8)
            D().line([(x * S, y * S) for x, y in outer + [outer[0]]], fill=(4, 4, 7, 255), width=S)

def chain_between(p0, p1, sag=10):
    chain(p0, p1, sag); anchor(*p0); anchor(*p1)

# ---------------------------------------------------------------- panels, knobs, buttons, meters
def panel(x, y, w, h, cut=24, title=None):
    outer = oct_pts(x, y, w, h, cut)
    dropshadow(outer, 8, 10, 220)
    lit(poly_mask(outer), base=118, bevel=11, k=2.6)
    D().line([(px * S, py * S) for px, py in outer + [outer[0]]], fill=(3, 3, 6, 255), width=int(3.5 * S))
    i = 16
    inner = oct_pts(x + i, y + i, w - 2 * i, h - 2 * i, cut - 6)
    lit(poly_mask(inner), base=92, bevel=6, k=2.0, invert=True, mul=1.0)
    D().line([(px * S, py * S) for px, py in inner + [inner[0]]], fill=(0, 0, 0, 255), width=2 * S)
    for px, py in [(x + cut + 2, y + 8), (x + w - cut - 2, y + 8), (x + cut + 2, y + h - 8), (x + w - cut - 2, y + h - 8)]:
        screw(px, py, 4)
    if title:
        metal_label((x + w / 2, y + 37), title, 17, glow_col=VIOLET, glow_str=0.9)
        glow(lambda g, cc: g.line([((x + w / 2 - 40) * S, (y + 52) * S), ((x + w / 2 + 40) * S, (y + 52) * S)], fill=cc + (255,), width=2 * S), VIOLET, 3)
        D().line([((x + w / 2 - 40) * S, (y + 52) * S), ((x + w / 2 + 40) * S, (y + 52) * S)], fill=(210, 170, 255, 255), width=S)

def knob(cx, cy, label, value, r=29, accent=RED):
    cx0, cy0 = cx, cy
    cx *= S; cy *= S; R = r * S
    ring = Image.new("L", canvas.size, 0)
    ImageDraw.Draw(ring).ellipse([cx - R - 9 * S, cy - R - 9 * S, cx + R + 9 * S, cy + R + 9 * S], fill=255)
    dropshadow([(cx0 + math.cos(a / 8 * math.pi) * (r + 9), cy0 + math.sin(a / 8 * math.pi) * (r + 9)) for a in range(16)], 5, 5, 200)
    lit(ring, base=100, bevel=5, k=2.6)
    inner = Image.new("L", canvas.size, 0)
    ImageDraw.Draw(inner).ellipse([cx - R - 4 * S, cy - R - 4 * S, cx + R + 4 * S, cy + R + 4 * S], fill=255)
    lit(inner, base=24, bevel=3, k=2.0, invert=True, mul=0.7)
    if EXPORT:                                   # socket + label only; the plugin draws arc and body live
        pl = Image.new("L", canvas.size, 0)
        ImageDraw.Draw(pl).rounded_rectangle([cx - 39 * S, cy + R + 12 * S, cx + 39 * S, cy + R + 34 * S], 4 * S, fill=255)
        lit(pl, base=30, bevel=2.5, k=2.0, invert=True, mul=0.8)
        metal_label((cx0, cy0 + r + 23), label, 11.5, glow_col=accent, glow_str=0.7)
        return
    box = [cx - R - 7 * S, cy - R - 7 * S, cx + R + 7 * S, cy + R + 7 * S]
    glow(lambda g, c: g.arc(box, 135, 135 + 270 * value, fill=c + (255,), width=3 * S), accent, 4, 1.5)
    d = D(); d.arc(box, 135, 405, fill=(0, 0, 0, 170), width=3 * S)
    d.arc(box, 135, 135 + 270 * value, fill=accent + (255,), width=3 * S)
    k = Image.new("RGBA", (int(2 * R + 2), int(2 * R + 2)), (0, 0, 0, 0)); kd = ImageDraw.Draw(k)
    for i in range(int(R), 0, -1):
        t = i / R; v = int(135 + 120 * (1 - t) ** 0.9); kd.ellipse([R - i, R - i, R + i, R + i], fill=(v, v, min(255, v + 6), 255))
    kd.ellipse([1, 1, 2 * R - 1, 2 * R - 1], outline=(25, 25, 30, 255), width=3 * S)
    for i in range(48):
        a = i * 7.5 * math.pi / 180
        kd.line([R + math.cos(a) * (R - 7 * S), R + math.sin(a) * (R - 7 * S), R + math.cos(a) * (R - 2), R + math.sin(a) * (R - 2)], fill=(105, 105, 118, 255), width=S)
    ang = math.radians(135 + 270 * value)
    kd.line([R + math.cos(ang) * R * .22, R + math.sin(ang) * R * .22, R + math.cos(ang) * R * .84, R + math.sin(ang) * R * .84], fill=accent + (255,), width=3 * S)
    canvas.paste(k, (int(cx - R), int(cy - R)), k)
    pl = Image.new("L", canvas.size, 0)
    ImageDraw.Draw(pl).rounded_rectangle([cx - 39 * S, cy + R + 12 * S, cx + 39 * S, cy + R + 34 * S], 4 * S, fill=255)
    lit(pl, base=30, bevel=2.5, k=2.0, invert=True, mul=0.8)
    metal_label((cx0, cy0 + r + 23), label, 11.5, glow_col=accent, glow_str=0.7)

def knob_body(r):
    """White knurled knob body without pointer (RGBA, 1x size) - rotated live by the plugin."""
    R = r * S; k = Image.new("RGBA", (int(2 * R + 2), int(2 * R + 2)), (0, 0, 0, 0)); kd = ImageDraw.Draw(k)
    for i in range(int(R), 0, -1):
        t = i / R; v = int(135 + 120 * (1 - t) ** 0.9); kd.ellipse([R - i, R - i, R + i, R + i], fill=(v, v, min(255, v + 6), 255))
    kd.ellipse([1, 1, 2 * R - 1, 2 * R - 1], outline=(25, 25, 30, 255), width=3 * S)
    for i in range(48):
        a = i * 7.5 * math.pi / 180
        kd.line([R + math.cos(a) * (R - 7 * S), R + math.sin(a) * (R - 7 * S), R + math.cos(a) * (R - 2), R + math.sin(a) * (R - 2)], fill=(105, 105, 118, 255), width=S)
    return k.resize((k.width // S, k.height // S), Image.LANCZOS)

SPRITES = {}
def button(x, y, w, h, text, active=False, color=RED, fs=14, name=None):
    if EXPORT and name:                          # export both states as opaque sprites
        saved = canvas.copy(); rect = (int((x - 8) * S), int((y - 8) * S), int((x + w + 8) * S), int((y + h + 8) * S))
        for st in (False, True):
            canvas.paste(saved); _button(x, y, w, h, text, st, color, fs)
            SPRITES[f"btn_{name}_{'on' if st else 'off'}"] = (canvas.crop(rect), rect)
        canvas.paste(saved); _button(x, y, w, h, text, False, color, fs)
    else:
        _button(x, y, w, h, text, active, color, fs)

def _button(x, y, w, h, text, active=False, color=RED, fs=14):
    shape = Image.new("L", canvas.size, 0)
    ImageDraw.Draw(shape).rounded_rectangle([x * S, y * S, (x + w) * S, (y + h) * S], 9 * S, fill=255)
    if active: glow(lambda g, c: g.rounded_rectangle([x * S, y * S, (x + w) * S, (y + h) * S], 9 * S, fill=c + (210,)), color, 8, 1.1)
    lit(shape, base=38 if not active else 44, bevel=4, k=2.6, tint=(0.85, 0.9, 1.3) if active else (1, 1, 1.1), mul=1.0 if not active else 1.4)
    D().rounded_rectangle([x * S, y * S, (x + w) * S, (y + h) * S], 9 * S, outline=(color + (255,)) if active else (3, 3, 6, 255), width=int(2 * S))
    metal_label((x + w / 2, y + h / 2), text, fs, active=active, glow_col=color if active else None, glow_str=0.9)

def meter(x, y, w, h, label, level):
    d = D()
    d.rounded_rectangle([x * S, y * S, (x + w) * S, (y + h) * S], 4 * S, fill=(5, 4, 9, 255), outline=(0, 0, 0, 255), width=2 * S)
    d.rounded_rectangle([(x - 2) * S, (y - 2) * S, (x + w + 2) * S, (y + h + 2) * S], 5 * S, outline=(110, 110, 125, 255), width=S)
    segs = 26; sw = (w - 8) / segs
    for i in (range(segs) if not EXPORT else []):
        on = (i / segs) < level
        col = VIOLET if i < segs * 0.6 else ((255, 80, 150) if i < segs * 0.85 else RED)
        if on: glow(lambda g, c, i=i: g.rectangle([(x + 4 + i * sw) * S, (y + 4) * S, (x + 4 + (i + 1) * sw - 2) * S, (y + h - 4) * S], fill=c + (160,)), col, 2, 0.8)
        D().rectangle([(x + 4 + i * sw) * S, (y + 4) * S, (x + 4 + (i + 1) * sw - 2) * S, (y + h - 4) * S], fill=(col + (255,)) if on else (34, 28, 44, 255))
    metal_label((x, y - 11), label, 10.5, anchor_="lm", glow_col=None)

def pipe(p0, p1, p2, p3, w=12, glow_col=None):
    pts = []
    for i in range(61):
        t = i / 60; u = 1 - t
        pts.append((u**3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t**3 * p3[0],
                    u**3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t**3 * p3[1]))
    nrm = []
    for i in range(len(pts)):
        a = pts[max(0, i - 1)]; b = pts[min(len(pts) - 1, i + 1)]
        dx, dy = b[0] - a[0], b[1] - a[1]; L = math.hypot(dx, dy) or 1; nrm.append((-dy / L, dx / L))
    d = D()
    d.line([((x + 4) * S, (y + 7) * S) for x, y in pts], fill=(0, 0, 0, 120), width=int(w * S + 6), joint="curve")
    steps = int(w * S)
    for j in range(steps + 1):
        o = (j / steps - 0.5) * w; prof = math.cos((o / (w / 2)) * math.pi / 2.1)
        b = 30 + 150 * max(0, prof) ** 1.8 + (35 if -0.45 < o / (w / 2) < -0.15 else 0)
        d.line([((x + nx * o) * S, (y + ny * o) * S) for (x, y), (nx, ny) in zip(pts, nrm)], fill=(int(b), int(b), int(b * 1.1), 255), width=2)
    if glow_col:
        glow(lambda g, c: g.line([(x * S, y * S) for x, y in pts], fill=c + (255,), width=int(2.2 * S)), glow_col, 3, 1.6)
        d.line([(x * S, y * S) for x, y in pts], fill=(255, 200, 215, 200), width=S)
    for t in (0.18, 0.82):
        i = int(t * (len(pts) - 1)); x, y = pts[i]; nx, ny = nrm[i]
        seg = [((x - nx * (w / 2 + 3)) * S, (y - ny * (w / 2 + 3)) * S), ((x + nx * (w / 2 + 3)) * S, (y + ny * (w / 2 + 3)) * S)]
        d.line(seg, fill=(96, 96, 110, 255), width=int(5 * S)); d.line(seg, fill=(18, 18, 24, 255), width=S)

# ======================= STATIC SCENE =======================
yy, xx = np.mgrid[0:CH, 0:CW]

pipe((550, 95), (550, 110), (550, 125), (550, 150), 16)
pipe((500, 395), (480, 420), (500, 440), (505, 452), 12, glow_col=RED)
pipe((600, 395), (620, 420), (600, 440), (595, 452), 12, glow_col=VIOLET)
pipe((120, 440), (120, 470), (130, 480), (130, 500), 12)
pipe((980, 440), (980, 470), (970, 480), (970, 500), 12)

# TITLE: armored wings + plate
for side in (-1, 1):
    wing = [(550 + side * 270, 22), (550 + side * 450, 30), (550 + side * 500, 70), (550 + side * 440, 100), (550 + side * 270, 96)]
    dropshadow(wing, 7, 9, 220); lit(poly_mask(wing), base=112, bevel=9, k=2.6)
    D().line([(px * S, py * S) for px, py in wing + [wing[0]]], fill=(3, 3, 6, 255), width=int(3.5 * S))
    for k in range(3):
        glow(lambda g, c, k=k: g.line([((550 + side * (300 + k * 40)) * S, 45 * S), ((550 + side * (330 + k * 40)) * S, 85 * S)], fill=c + (255,), width=3 * S), RED, 3, 1.5)
plate = oct_pts(235, 14, 630, 88, 30)
dropshadow(plate, 8, 10, 230); lit(poly_mask(plate), base=122, bevel=12, k=2.8)
D().line([(px * S, py * S) for px, py in plate + [plate[0]]], fill=(3, 3, 6, 255), width=int(3.5 * S))
inner = oct_pts(252, 28, 596, 60, 22); title_window = poly_mask(inner)
lit(title_window, base=24, bevel=6, k=2.4, invert=True, mul=0.7)
D().line([(px * S, py * S) for px, py in inner + [inner[0]]], fill=(0, 0, 0, 255), width=2 * S)
for col, r_, st in ((VIOLET, 22, 0.8), (RED, 10, 1.5)):
    glow(lambda g, cc: g.text((550 * S, 58 * S), "VONOX", font=font(46, FH), fill=cc + (255,), anchor="mm", stroke_width=3 * S), col, r_, st)
vonox_mask = metal_label((550, 58), "VONOX", 46, glow_col=None, base=225)
for px, py in [(262, 24), (838, 24), (262, 92), (838, 92)]: screw(px, py, 4.5)

# SIDE PANELS
panel(18, 118, 255, 330, title="VOICE")
knob(80, 215, "TUNE", 0.0); knob(211, 215, "PRESENCE", 0.62, accent=VIOLET)
knob(80, 360, "AIR", 0.55, accent=VIOLET); knob(211, 360, "WARMTH", 0.4)
panel(827, 118, 255, 330, title="SPACE")
knob(889, 215, "SPACE", 0.5); knob(1020, 215, "CHARACTER", 0.5, accent=VIOLET)
knob(889, 360, "WIDTH", 0.5, accent=VIOLET); knob(1020, 360, "COMPRESS", 0.5)

# CENTER: octagon frame + red glass (animated contents added per frame)
cx, cy = 550, 262
def ring_pts(rad): return [(cx + rad * math.cos(math.radians(22.5 + 45 * i)), cy + rad * math.sin(math.radians(22.5 + 45 * i))) for i in range(8)]
frame, mid, glass = ring_pts(158), ring_pts(130), ring_pts(118)
glow(lambda g, c: g.polygon([(px * S, py * S) for px, py in frame], outline=c + (255,), width=8 * S), RED, 14, 1.4)
dropshadow(frame, 9, 11, 235); lit(poly_mask(frame), base=125, bevel=14, k=2.8)
D().line([(px * S, py * S) for px, py in frame + [frame[0]]], fill=(3, 3, 6, 255), width=int(4 * S))
lit(poly_mask(mid), base=34, bevel=7, k=2.4, invert=True, mul=0.8)
D().line([(px * S, py * S) for px, py in mid + [mid[0]]], fill=(0, 0, 0, 255), width=3 * S)
rr = np.hypot(xx - cx * S, yy - (cy - 8) * S) / (118 * S)
t_ = np.clip(rr, 0, 1)
C0, C1, C2 = np.array([82, 44, 200], np.float32), np.array([30, 18, 108], np.float32), np.array([5, 4, 28], np.float32)
k1 = np.clip(t_ / 0.55, 0, 1)[..., None]; k2 = np.clip((t_ - 0.55) / 0.45, 0, 1)[..., None]
disp = np.where(t_[..., None] < 0.55, C0 + (C1 - C0) * k1, C1 + (C2 - C1) * k2)
neb = noise_layer(CW, CH, 70 * S, 70 * S, 17) * 0.65 + noise_layer(CW, CH, 26 * S, 26 * S, 18) * 0.35
disp += (np.clip(neb - 0.35, 0, 1) * (1 - t_) * 1.4)[..., None] * np.array([120, 30, 150], np.float32)
disp += (np.clip(noise_layer(CW, CH, 50 * S, 50 * S, 23) - 0.5, 0, 1) * (1 - 0.6 * t_) * 1.6)[..., None] * np.array([20, 60, 150], np.float32)
disp = np.clip(disp, 0, 255)
glass_mask = poly_mask(glass); canvas.paste(Image.fromarray(disp.astype(np.uint8)), (0, 0), glass_mask)
_r = random.Random(77)
for _ in range(46):
    _a, _d = _r.uniform(0, 6.283), 108 * math.sqrt(_r.random())
    _x, _y = (cx + _d * math.cos(_a)) * S, (cy + _d * math.sin(_a)) * S; _v = _r.randint(70, 170); _rad = _r.choice([0.7, 0.9, 1.2]) * S
    ImageDraw.Draw(canvas).ellipse([_x - _rad, _y - _rad, _x + _rad, _y + _rad], fill=(_v, _v, min(255, _v + 40)))
dd = D()
for a in range(0, 360, 30):
    x0 = cx + 40 * math.cos(math.radians(a)); y0 = cy + 40 * math.sin(math.radians(a))
    x1 = cx + 112 * math.cos(math.radians(a + 8)); y1 = cy + 112 * math.sin(math.radians(a + 8))
    dd.line([(x0 * S, y0 * S), (x1 * S, y1 * S)], fill=(150, 160, 255, 40), width=S)
refl = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
ImageDraw.Draw(refl).pieslice([(cx - 105) * S, (cy - 125) * S, (cx + 105) * S, (cy + 40) * S], 200, 340, fill=(255, 255, 255, 34))
canvas.paste(refl, (0, 0), Image.composite(refl.getchannel("A"), Image.new("L", canvas.size, 0), glass_mask))
D().line([(px * S, py * S) for px, py in glass + [glass[0]]], fill=(0, 0, 0, 255), width=2 * S)

# AUTO MIX: chunky machine button (flames + embers animated per frame)
bx, by, bw, bh = 428, 428, 244, 74
btn = [(bx + 30, by), (bx + bw - 30, by), (bx + bw, by + 20), (bx + bw - 14, by + bh), (bx + 14, by + bh), (bx, by + 20)]
dropshadow(btn, 8, 10, 230); lit(poly_mask(btn), base=118, bevel=12, k=2.8)
D().line([(px * S, py * S) for px, py in btn + [btn[0]]], fill=(3, 3, 6, 255), width=int(3.5 * S))
bi = [(bx + 44, by + 11), (bx + bw - 44, by + 11), (bx + bw - 22, by + 24), (bx + bw - 30, by + bh - 11), (bx + 30, by + bh - 11), (bx + 22, by + 24)]
auto_window = poly_mask(bi)
glow(lambda g, c: g.polygon([(px * S, py * S) for px, py in bi], fill=c + (230,)), VIOLET, 14, 1.2)
lit(auto_window, base=44, bevel=6, k=2.4, invert=True, tint=(0.75, 0.62, 1.7), mul=1.0)
D().line([(px * S, py * S) for px, py in bi + [bi[0]]], fill=(130, 110, 255, 255), width=int(2.5 * S))
auto_mask = metal_label((bx + bw / 2, by + bh / 2 + 3), "AUTO MIX", 27, glow_col=VIOLET, glow_str=1.3, base=225)
for px, py in [(bx + 18, by + 12), (bx + bw - 18, by + 12), (bx + 10, by + bh - 10), (bx + bw - 10, by + bh - 10)]: screw(px, py, 4)

# BOTTOM LEFT: meters
panel(18, 478, 400, 200, cut=18)
meter(44, 522, 346, 22, "INPUT", 0.55); meter(44, 574, 346, 22, "OUTPUT", 0.68)
for rx, rl in ((44, "LATENCY"), (227, "TRUE PEAK")):                     # recessed readout windows (values are drawn live)
    rm = Image.new("L", canvas.size, 0); ImageDraw.Draw(rm).rounded_rectangle([rx * S, 614 * S, (rx + 163) * S, 656 * S], 5 * S, fill=255)
    lit(rm, base=20, bevel=3, k=2.0, invert=True, mul=0.7)
    D().rounded_rectangle([rx * S, 614 * S, (rx + 163) * S, 656 * S], 5 * S, outline=(0, 0, 0, 255), width=2 * S)
    D().rounded_rectangle([(rx - 2) * S, 612 * S, (rx + 165) * S, 658 * S], 6 * S, outline=(110, 110, 125, 255), width=S)
    metal_label((rx + 81.5, 623.5), rl, 9.5, anchor_="mm", glow_col=None)

# BOTTOM RIGHT: controls
panel(682, 478, 400, 200, cut=18)
knob(746, 566, "MIX", 0.72, r=26, accent=VIOLET); knob(841, 566, "OUTPUT", 0.5, r=26)
button(905, 512, 150, 34, "BYPASS", False, MAGENTA, 13, name="bypass")
button(905, 556, 70, 34, "A", True, VIOLET, 15, name="a"); button(985, 556, 70, 34, "B", False, VIOLET, 15, name="b")
button(905, 600, 70, 34, "RESET", False, RED, 12, name="reset"); button(985, 600, 70, 34, "PRESETS", False, RED, 11, name="presets")

# BOTTOM CENTER: hexagon emblem plate with the embossed "V" mark
hexp = [(550 + 112 * math.cos(math.radians(60 * i)), 598 + 78 * math.sin(math.radians(60 * i))) for i in range(6)]
dropshadow(hexp, 8, 10, 230); lit(poly_mask(hexp), base=125, bevel=12, k=2.8)
D().line([(px * S, py * S) for px, py in hexp + [hexp[0]]], fill=(3, 3, 6, 255), width=int(3.5 * S))
hi = [(550 + 90 * math.cos(math.radians(60 * i)), 598 + 60 * math.sin(math.radians(60 * i))) for i in range(6)]
lit(poly_mask(hi), base=24, bevel=6, k=2.2, invert=True, mul=0.7)
D().line([(px * S, py * S) for px, py in hi + [hi[0]]], fill=(0, 0, 0, 255), width=2 * S)
button(480, 578, 140, 40, "ADVANCED", False, VIOLET, 14, name="adv")

# MILITARY CHAINS: three per side from each side panel to the red display, plus two to AUTO MIX
for py_panel, py_oct in ((190, 222), (262, 262), (334, 302)):
    chain_between((268, py_panel), (408, py_oct), sag=11)
    chain_between((832, py_panel), (692, py_oct), sag=11)
chain_between((262, 444), (434, 468), sag=9)
chain_between((838, 444), (666, 468), sag=9)

BASE = canvas.copy()

# ======================= ANIMATED LAYERS =======================
class FlameFx:
    """Looping procedural flames rising from the lettering shape (drawn behind the letters)."""
    def __init__(self, clip_mask, src_mask, rise, strength, seed, r_ramp=(70, 70, 90, 170, 255), g_ramp=(20, 60, 150, 230, 255), b_ramp=(220, 255, 255, 255, 255), exclude=None):
        self.r_ramp, self.g_ramp, self.b_ramp = r_ramp, g_ramp, b_ramp
        bb = src_mask.getbbox()
        self.box = (max(0, bb[0] - 24 * S), max(0, bb[1] - int((rise + 24) * S)), min(CW, bb[2] + 24 * S), min(CH, bb[3] + 12 * S))
        sm = np.asarray(src_mask.crop(self.box), np.float32) / 255; h, w = sm.shape
        heat = np.zeros_like(sm); n = int(rise * S)
        for k in range(0, n, 2):
            sh = np.zeros_like(sm); sh[:h - k, :] = sm[k:, :]
            heat = np.maximum(heat, sh * (1 - k / n) ** 1.3)
        self.heat0, self.sm, self.h, self.w, self.strength = heat, sm, h, w, strength
        ex = sm if exclude is None else np.asarray(exclude.crop(self.box), np.float32) / 255
        self.clip = np.asarray(clip_mask.crop(self.box), np.float32) / 255 * (1 - ex)     # behind the letters
        self.ys, self.xs = np.mgrid[0:h, 0:w]
        self.N1 = pnoise(w, 8, 9 * S, 26 * S, seed); self.N2 = pnoise(w, 8, 14 * S, 14 * S, seed + 7); self.N3 = pnoise(w, 8, 6 * S, 30 * S, seed + 1)
    def _win(self, N, off): return N[(np.arange(self.h) + off) % N.shape[0], :]
    def render(self, img, phase):
        fx = (self._win(self.N1, int(phase * self.N1.shape[0])) - 0.5) * 20 * S
        fy = (self._win(self.N2, int(phase * self.N2.shape[0])) - 0.5) * 8 * S
        t = self._win(self.N3, int(phase * 2 * self.N3.shape[0]))
        heat = self.heat0[np.clip((self.ys + fy).astype(int), 0, self.h - 1), np.clip((self.xs + fx).astype(int), 0, self.w - 1)]
        v = np.clip(heat * (0.35 + 1.35 * t) + self.sm * 0.25, 0, 1.2)
        st = [0, 0.15, 0.4, 0.7, 1.0]
        col = np.stack([np.interp(v, st, self.r_ramp), np.interp(v, st, self.g_ramp), np.interp(v, st, self.b_ramp)], -1)
        a = (np.clip((v - 0.06) * 2.0, 0, 1) * self.strength * self.clip)[..., None]
        reg = np.asarray(img.crop(self.box), np.float32)
        img.paste(Image.fromarray(np.clip(reg * (1 - a) + col * a, 0, 255).astype(np.uint8)), self.box[:2])

class Embers:
    """Looping particles rising / drifting inside a clip shape (optionally hidden behind lettering)."""
    def __init__(self, n, xr, y0, rise, clip_mask, seed, size=(1.0, 2.6), palette=((255, 200, 120),), blur=2.0, gain=1.5, exclude=None, amp=(2, 9)):
        r = random.Random(seed); self.box = clip_mask.getbbox(); self.blur, self.gain = blur, gain
        self.p = [dict(x0=r.uniform(*xr) * S, y0=(y0 + r.uniform(-4, 4)) * S, rise=r.uniform(0.55, 1.0) * rise * S, k=r.choice([1, 1, 2]),
                       off=r.random(), amp=r.uniform(*amp) * S, fq=r.choice([1, 2, 3]), ph=r.random(), size=r.uniform(*size) * S, col=r.choice(palette)) for _ in range(n)]
        c = np.asarray(clip_mask.crop(self.box), np.float32) / 255
        if exclude is not None: c = c * (1 - np.asarray(exclude.crop(self.box), np.float32) / 255)
        self.clip = c
    def render(self, img, phase):
        x0b, y0b, x1b, y1b = self.box
        lay = Image.new("RGB", (x1b - x0b, y1b - y0b), (0, 0, 0)); d = ImageDraw.Draw(lay)
        for p in self.p:
            u = (phase * p["k"] + p["off"]) % 1.0; a = math.sin(math.pi * u) ** 0.8
            if a < 0.03: continue
            x = p["x0"] + p["amp"] * math.sin(2 * math.pi * (u * p["fq"] + p["ph"])) - x0b; y = p["y0"] - u * p["rise"] - y0b
            r_ = p["size"] * (0.6 + 0.4 * a)
            d.ellipse([x - r_, y - r_, x + r_, y + r_], fill=tuple(int(c * a) for c in p["col"]))
        arr = (np.asarray(lay, np.float32) + self.gain * np.asarray(lay.filter(ImageFilter.GaussianBlur(self.blur * S)), np.float32)) * self.clip[..., None]
        reg = np.asarray(img.crop(self.box), np.float32)
        img.paste(Image.fromarray(np.clip(reg + arr, 0, 255).astype(np.uint8)), (x0b, y0b))

# --- display contents (kept in its own style, NOT the V lettering) ---
BOX_O = (int((cx - 135) * S), int((cy - 135) * S), int((cx + 135) * S), int((cy + 135) * S))
txt = Image.new("RGBA", canvas.size, (0, 0, 0, 0)); td = ImageDraw.Draw(txt)
td.text((cx * S, (cy - 80) * S), "VOCAL PROFILE", font=font(13), fill=(205, 215, 255, 255), anchor="mm")
td.rounded_rectangle([(cx - 92) * S, (cy - 64) * S, (cx + 92) * S, (cy - 34) * S], 4 * S, fill=(8, 0, 12, 235), outline=(150, 50, 110, 255), width=S)
td.text((cx * S, (cy - 49) * S), "◀  MODERN RAP  ▶", font=font(16), fill=WHITE + (255,), anchor="mm")
td.text((cx * S, (cy + 8) * S), "MIX AMOUNT", font=font(13), fill=(205, 215, 255, 255), anchor="mm")
td.text((cx * S, (cy + 52) * S), "72 %", font=font(40), fill=(225, 230, 255, 255), anchor="mm")
TXT_O = txt.crop(BOX_O)
gl = Image.new("RGBA", canvas.size, (0, 0, 0, 0)); ImageDraw.Draw(gl).text((cx * S, (cy + 52) * S), "72 %", font=font(40), fill=RED + (255,), anchor="mm")
GLOW_O = gl.crop(BOX_O).filter(ImageFilter.GaussianBlur(6 * S))
GBOX = glass_mask.getbbox()
GCLIP = np.asarray(glass_mask.crop(GBOX), np.float32) / 255
RAD = np.clip(1 - rr[GBOX[1]:GBOX[3], GBOX[0]:GBOX[2]], 0, 1) ** 1.5

# Title fire: a source strip along the whole bottom of the black window + the letters themselves,
# so flames rise across the full width of the title plate.
fire_src = Image.new("L", canvas.size, 0)
ImageDraw.Draw(fire_src).rectangle([262 * S, 80 * S, 838 * S, 90 * S], fill=175)
FX = [
    # galaxy flames: violet -> magenta nebula in the title, violet -> blue -> cyan in AUTO MIX
    FlameFx(title_window, fire_src, rise=54, strength=0.95, seed=3, r_ramp=(60, 110, 190, 255, 255), g_ramp=(14, 24, 55, 140, 230), b_ramp=(180, 235, 255, 255, 255), exclude=vonox_mask),
    FlameFx(auto_window, auto_mask, rise=20, strength=0.6, seed=11, r_ramp=(50, 60, 90, 150, 230), g_ramp=(20, 50, 120, 220, 255), b_ramp=(200, 255, 255, 255, 255)),
]
EMBERS = [
    Embers(26, (430, 670), 484, 34, auto_window, 21, size=(0.9, 2.2), palette=((255, 255, 255), (190, 210, 255), (170, 130, 255)), exclude=auto_mask),
    Embers(80, (266, 834), 88, 60, title_window, 5, size=(0.9, 2.6), palette=((255, 255, 255), (200, 215, 255), (255, 150, 230), (150, 120, 255)), exclude=vonox_mask, gain=1.4, amp=(2, 12)),
    Embers(12, (270, 830), 90, 54, title_window, 9, size=(3, 6), palette=((90, 80, 220), (160, 60, 220)), blur=4.0, gain=2.0, exclude=vonox_mask, amp=(6, 16)),
    Embers(70, (440, 660), cy + 112, 215, glass_mask, 31, size=(1.0, 2.6), palette=((255, 90, 80), (255, 60, 110), (255, 150, 90), (190, 100, 255)), blur=2.2, gain=1.7, amp=(4, 16)),
    Embers(12, (450, 650), cy + 115, 200, glass_mask, 47, size=(4, 9), palette=((90, 80, 220), (160, 60, 220)), blur=5.0, gain=2.2, amp=(8, 22)),
]

SHINE_NP = np.asarray(SHINE, np.float32) / 255
U = ((xx / CW + 0.35 * yy / CH) / 1.35).astype(np.float32)
VIG = np.clip(1.12 - 0.4 * np.hypot((xx - CW / 2) / (CW / 2), (yy - CH / 2) / (CH / 2)) ** 2, 0.4, 1.0).astype(np.float32)

def render_frame(phase, shine=True):
    img = BASE.copy()
    pulse = 0.5 + 0.5 * math.sin(2 * math.pi * phase)
    # display: breathing red glow, then particles, then readout on top
    reg = np.asarray(img.crop(GBOX), np.float32)
    reg += RAD[..., None] * GCLIP[..., None] * np.array([110, 60, 255], np.float32) * (0.08 + 0.16 * pulse)
    img.paste(Image.fromarray(np.clip(reg, 0, 255).astype(np.uint8)), GBOX[:2])
    for em in EMBERS[3:]: em.render(img, phase)
    reg = img.crop(BOX_O).convert("RGBA")
    g = GLOW_O.copy(); g.putalpha(g.getchannel("A").point(lambda v: int(v * (0.45 + 0.55 * pulse)))); reg.alpha_composite(g); reg.alpha_composite(TXT_O)
    img.paste(reg.convert("RGB"), BOX_O[:2])
    # flames + embers for the two titles
    for fx in FX: fx.render(img, phase)
    for em in EMBERS[:3]: em.render(img, phase)
    # light sweep across all metal lettering
    arr = np.asarray(img, np.float32)
    pos = -0.15 + 1.3 * phase
    if shine:
        band = np.exp(-(((U - pos) / 0.05) ** 2)) * SHINE_NP
        arr += band[..., None] * np.array([255, 215, 230], np.float32) * 0.55
    arr *= VIG[..., None]
    return Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8)).resize((W, H), Image.LANCZOS)

if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "frames"; n = int(sys.argv[2]) if len(sys.argv) > 2 else 72
    os.makedirs(out, exist_ok=True)
    for i in range(n):
        render_frame(i / n).save(os.path.join(out, f"f{i:03d}.png"))
    print("frames:", n)
