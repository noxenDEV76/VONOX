"""Renders a static PNG preview of the VONOX interface (original "machine" design:
lit bevelled steel, pipes, cubes, embossed text, red/violet glow, white knobs).
Preview only: the real UI is drawn by JUCE in the plugin."""
import math, random, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

S = 2
W, H = 1100, 700
RED, VIOLET, WHITE = (255, 40, 60), (150, 70, 255), (240, 240, 245)
FB = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf"
FM = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
def font(sz, path=FB): return ImageFont.truetype(path, int(sz * S))
random.seed(7); np.random.seed(7)

def steel(w, h, base=40, tint=(1.0, 1.0, 1.04)):
    n = np.random.normal(base, 6, (h, 1)).repeat(w, axis=1) + np.random.normal(0, 5, (h, w))
    # scratches
    for _ in range(max(1, w * h // 40000)):
        y = random.randrange(h); x0 = random.randrange(w); L = random.randrange(20, 120)
        n[y, x0:x0 + L] += random.choice([-12, 14])
    n = np.clip(n, 6, 255)
    return np.stack([n * tint[0], n * tint[1], n * tint[2]], axis=-1)

canvas = Image.fromarray(np.clip(steel(W * S, H * S, 20), 0, 255).astype(np.uint8))

def lit(mask, base=46, bevel=8, k=2.4, tint=(1, 1, 1.04), invert=False, light=(-0.7, -0.7), mul=1.0):
    """Paste brushed steel into `mask` with real height-map lighting (3D bevel)."""
    bb = mask.getbbox()
    if not bb: return
    pad = int(bevel * S * 3)
    box = (max(0, bb[0] - pad), max(0, bb[1] - pad), min(mask.width, bb[2] + pad), min(mask.height, bb[3] + pad))
    m = mask.crop(box)
    hgt = np.asarray(m.filter(ImageFilter.GaussianBlur(bevel * S)), np.float32) / 255.0 * bevel * S
    gy, gx = np.gradient(hgt)
    s = -(gx * light[0] + gy * light[1]) / max(1e-6, bevel * S * 0.18)
    s = -s if invert else s
    tex = steel(m.width, m.height, base, tint)
    col = tex * np.maximum(0.62 + k * 0.5 * s[..., None], 0.3) * mul
    col = np.clip(col, 0, 255).astype(np.uint8)
    region = canvas.crop(box); region.paste(Image.fromarray(col), (0, 0), m); canvas.paste(region, box[:2])

def poly_mask(pts):
    m = Image.new("L", canvas.size, 0)
    ImageDraw.Draw(m).polygon([(x * S, y * S) for x, y in pts], fill=255); return m

def oct_pts(x, y, w, h, cut):
    return [(x + cut, y), (x + w - cut, y), (x + w, y + cut), (x + w, y + h - cut),
            (x + w - cut, y + h), (x + cut, y + h), (x, y + h - cut), (x, y + cut)]

def glow(fn, color, radius, strength=1.0):
    g = Image.new("RGBA", canvas.size, (0, 0, 0, 0)); fn(ImageDraw.Draw(g), color)
    g = g.filter(ImageFilter.GaussianBlur(radius * S))
    if strength != 1.0: g.putalpha(g.getchannel("A").point(lambda v: min(255, int(v * strength))))
    canvas.paste(g, (0, 0), g)

def D(): return ImageDraw.Draw(canvas, "RGBA")

def dropshadow(pts, off=6, blur=8, alpha=200):
    g = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ImageDraw.Draw(g).polygon([((x + off) * S, (y + off) * S) for x, y in pts], fill=(0, 0, 0, alpha))
    g = g.filter(ImageFilter.GaussianBlur(blur * S)); canvas.paste(g, (0, 0), g)

def screw(x, y, r=5):
    x *= S; y *= S; R = r * S
    k = Image.new("L", canvas.size, 0); ImageDraw.Draw(k).ellipse([x - R, y - R, x + R, y + R], fill=255)
    lit(k, base=150, bevel=r * 0.5, k=3.0)
    d = D(); d.ellipse([x - R, y - R, x + R, y + R], outline=(5, 5, 8, 255), width=S)
    a = random.random() * math.pi
    d.line([x - math.cos(a) * R * .7, y - math.sin(a) * R * .7, x + math.cos(a) * R * .7, y + math.sin(a) * R * .7],
           fill=(8, 8, 10, 255), width=S + 1)

def cube(x, y, s, tone=1.0):
    """Isometric bolted metal block, (x,y) = top vertex."""
    top = [(x, y), (x + s, y + s * .5), (x, y + s), (x - s, y + s * .5)]
    left = [(x - s, y + s * .5), (x, y + s), (x, y + s * 2), (x - s, y + s * 1.5)]
    right = [(x, y + s), (x + s, y + s * .5), (x + s, y + s * 1.5), (x, y + s * 2)]
    dropshadow(right + [(x + s + 4, y + s * 1.5 + 4)], off=5, blur=5, alpha=170)
    for face, sh, bv in ((left, 0.62, 3), (right, 0.38, 3), (top, 1.15, 3)):
        lit(poly_mask(face), base=120, bevel=bv, k=1.6, mul=sh * tone)
    d = D()
    for face in (left, right, top):
        d.line([(px * S, py * S) for px, py in face + [face[0]]], fill=(4, 4, 8, 255), width=S + 1)
    d.line([(x * S, (y + s) * S), ((x - s) * S, (y + s * .5) * S)], fill=(190, 190, 205, 200), width=S)
    d.line([(x * S, (y + s) * S), ((x + s) * S, (y + s * .5) * S)], fill=(150, 150, 165, 160), width=S)
    screw(x, y + s * .5, max(3, s * .22))

def bezier(p0, p1, p2, p3, n=60):
    out = []
    for i in range(n + 1):
        t = i / n; u = 1 - t
        out.append((u**3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t**3 * p3[0],
                    u**3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t**3 * p3[1]))
    return out

def pipe(p0, p1, p2, p3, w=12, glow_col=None):
    pts = bezier(p0, p1, p2, p3)
    nrm = []
    for i in range(len(pts)):
        a = pts[max(0, i - 1)]; b = pts[min(len(pts) - 1, i + 1)]
        dx, dy = b[0] - a[0], b[1] - a[1]; L = math.hypot(dx, dy) or 1
        nrm.append((-dy / L, dx / L))
    d = ImageDraw.Draw(canvas, "RGBA")
    # shadow
    d.line([((x + 4) * S, (y + 7) * S) for x, y in pts], fill=(0, 0, 0, 120), width=int(w * S + 6), joint="curve")
    steps = int(w * S)
    for j in range(steps + 1):
        o = (j / steps - 0.5) * w
        prof = math.cos((o / (w / 2)) * math.pi / 2.1)
        b = 30 + 150 * max(0, prof) ** 1.8 + (35 if -0.45 < o / (w / 2) < -0.15 else 0)
        d.line([(x + nx * o, y + ny * o) if False else ((x + nx * o) * S, (y + ny * o) * S)
                for (x, y), (nx, ny) in zip(pts, nrm)], fill=(int(b), int(b), int(b * 1.1), 255), width=2)
    if glow_col:
        glow(lambda g, c: g.line([(x * S, y * S) for x, y in pts], fill=c + (255,), width=int(2.2 * S)), glow_col, 3, 1.6)
        d.line([(x * S, y * S) for x, y in pts], fill=(255, 200, 215, 200), width=S)
    # clamp rings
    for t in (0.18, 0.82):
        i = int(t * (len(pts) - 1)); x, y = pts[i]; nx, ny = nrm[i]
        d.line([((x - nx * (w / 2 + 3)) * S, (y - ny * (w / 2 + 3)) * S), ((x + nx * (w / 2 + 3)) * S, (y + ny * (w / 2 + 3)) * S)],
               fill=(96, 96, 110, 255), width=int(5 * S))
        d.line([((x - nx * (w / 2 + 3)) * S, (y - ny * (w / 2 + 3)) * S), ((x + nx * (w / 2 + 3)) * S, (y + ny * (w / 2 + 3)) * S)],
               fill=(18, 18, 24, 255), width=S)

def panel(x, y, w, h, cut=24, title=None, corners=True):
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
        emboss_text((x + w / 2, y + 36), title, 15, color=(225, 225, 235), underline=True)

def emboss_text(c, text, size, color=(230, 230, 238), underline=False, f=FB):
    """Engraved/embossed look: dark offset below, light edge above, face on top."""
    x, y = c[0] * S, c[1] * S; d = D(); ft = font(size, f)
    d.text((x + 1.5 * S, y + 1.5 * S), text, font=ft, fill=(0, 0, 0, 230), anchor="mm")
    d.text((x - 1 * S, y - 1 * S), text, font=ft, fill=(255, 255, 255, 70), anchor="mm")
    d.text((x, y), text, font=ft, fill=color + (255,), anchor="mm")
    if underline:
        glow(lambda g, cc: g.line([(x - 38 * S, y + 13 * S), (x + 38 * S, y + 13 * S)], fill=cc + (255,), width=2 * S), VIOLET, 3)
        D().line([(x - 38 * S, y + 13 * S), (x + 38 * S, y + 13 * S)], fill=(210, 170, 255, 255), width=S)

def metal_text(c, text, size, glow_cols=(RED, VIOLET)):
    x, y = c[0] * S, c[1] * S
    for col, r, st in ((glow_cols[1], 20, 0.9), (glow_cols[0], 9, 1.6)):
        glow(lambda g, cc: g.text((x, y), text, font=font(size), fill=cc + (255,), anchor="mm", stroke_width=3 * S), col, r, st)
    m = Image.new("L", canvas.size, 0); ImageDraw.Draw(m).text((x, y), text, font=font(size), fill=255, anchor="mm")
    D().text((x + 3 * S, y + 4 * S), text, font=font(size), fill=(0, 0, 0, 255), anchor="mm")
    lit(m, base=235, bevel=1.4, k=2.2, tint=(1, 0.92, 0.98))

def knob(cx, cy, label, value, r=29, accent=RED):
    cx *= S; cy *= S; R = r * S
    # dark metal socket (3D ring)
    ring = Image.new("L", canvas.size, 0)
    ImageDraw.Draw(ring).ellipse([cx - R - 9 * S, cy - R - 9 * S, cx + R + 9 * S, cy + R + 9 * S], fill=255)
    dropshadow([(cx / S + math.cos(a / 8 * math.pi) * (r + 9), cy / S + math.sin(a / 8 * math.pi) * (r + 9)) for a in range(16)], 5, 5, 200)
    lit(ring, base=100, bevel=5, k=2.6)
    inner = Image.new("L", canvas.size, 0)
    ImageDraw.Draw(inner).ellipse([cx - R - 4 * S, cy - R - 4 * S, cx + R + 4 * S, cy + R + 4 * S], fill=255)
    lit(inner, base=24, bevel=3, k=2.0, invert=True, mul=0.7)
    # value arc glow
    box = [cx - R - 7 * S, cy - R - 7 * S, cx + R + 7 * S, cy + R + 7 * S]
    glow(lambda g, c: g.arc(box, 135, 135 + 270 * value, fill=c + (255,), width=3 * S), accent, 4, 1.5)
    d = D(); d.arc(box, 135, 405, fill=(0, 0, 0, 170), width=3 * S)
    d.arc(box, 135, 135 + 270 * value, fill=accent + (255,), width=3 * S)
    # white knob body (radial shading + knurled edge + highlight)
    k = Image.new("RGBA", (int(2 * R + 2), int(2 * R + 2)), (0, 0, 0, 0)); kd = ImageDraw.Draw(k)
    for i in range(int(R), 0, -1):
        t = i / R; v = int(135 + 120 * (1 - t) ** 0.9); kd.ellipse([R - i, R - i, R + i, R + i], fill=(v, v, min(255, v + 6), 255))
    kd.ellipse([1, 1, 2 * R - 1, 2 * R - 1], outline=(25, 25, 30, 255), width=3 * S)
    for i in range(48):
        a = i * 7.5 * math.pi / 180
        kd.line([R + math.cos(a) * (R - 7 * S), R + math.sin(a) * (R - 7 * S), R + math.cos(a) * (R - 2), R + math.sin(a) * (R - 2)],
                fill=(105, 105, 118, 255), width=S)
    kd.ellipse([R * .42, R * .3, R * 1.1, R * .78], outline=(255, 255, 255, 90), width=S)
    ang = math.radians(135 + 270 * value)
    kd.line([R + math.cos(ang) * R * .22, R + math.sin(ang) * R * .22, R + math.cos(ang) * R * .84, R + math.sin(ang) * R * .84],
            fill=accent + (255,), width=3 * S)
    canvas.paste(k, (int(cx - R), int(cy - R)), k)
    # label plate
    pl = Image.new("L", canvas.size, 0)
    ImageDraw.Draw(pl).rounded_rectangle([cx - 37 * S, cy + R + 13 * S, cx + 37 * S, cy + R + 33 * S], 4 * S, fill=255)
    lit(pl, base=30, bevel=2.5, k=2.0, invert=True, mul=0.8)
    D().text((cx, cy + R + 23 * S), label, font=font(11), fill=(232, 232, 240), anchor="mm")

def button(x, y, w, h, text, active=False, color=RED, fs=14):
    shape = Image.new("L", canvas.size, 0)
    ImageDraw.Draw(shape).rounded_rectangle([x * S, y * S, (x + w) * S, (y + h) * S], 9 * S, fill=255)
    if active:
        glow(lambda g, c: g.rounded_rectangle([x * S, y * S, (x + w) * S, (y + h) * S], 9 * S, fill=c + (210,)), color, 8, 1.1)
    lit(shape, base=38 if not active else 44, bevel=4, k=2.6, tint=(1, .8, .85) if active and color == RED else (1, 1, 1.1),
        mul=1.0 if not active else 1.4)
    d = D()
    d.rounded_rectangle([x * S, y * S, (x + w) * S, (y + h) * S], 9 * S,
                        outline=(color + (255,)) if active else (3, 3, 6, 255), width=int(2 * S))
    d.text(((x + w / 2) * S + S, (y + h / 2) * S + S), text, font=font(fs), fill=(0, 0, 0, 220), anchor="mm")
    d.text(((x + w / 2) * S, (y + h / 2) * S), text, font=font(fs), fill=WHITE if active else (165, 165, 178), anchor="mm")

def hexmesh(x, y, w, h):
    m = Image.new("L", canvas.size, 0)
    ImageDraw.Draw(m).rounded_rectangle([x * S, y * S, (x + w) * S, (y + h) * S], 6 * S, fill=255)
    lit(m, base=20, bevel=4, k=2.0, invert=True, mul=0.6)
    layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0)); ld = ImageDraw.Draw(layer)
    r = 6 * S; dx = r * 1.732; yy = y * S + r
    row = 0
    while yy < (y + h) * S:
        xx = x * S + (dx / 2 if row % 2 else 0) + r
        while xx < (x + w) * S:
            pts = [(xx + r * .8 * math.cos(math.radians(60 * i + 30)), yy + r * .8 * math.sin(math.radians(60 * i + 30))) for i in range(6)]
            ld.polygon(pts, outline=(70, 60, 90, 200), fill=(6, 4, 10, 255)); xx += dx
        yy += r * 1.5; row += 1
    clip = Image.new("L", canvas.size, 0)
    ImageDraw.Draw(clip).rounded_rectangle([(x + 3) * S, (y + 3) * S, (x + w - 3) * S, (y + h - 3) * S], 5 * S, fill=255)
    canvas.paste(layer, (0, 0), Image.composite(layer.getchannel("A"), Image.new("L", canvas.size, 0), clip))

def meter(x, y, w, h, label, level):
    d = D()
    d.rounded_rectangle([x * S, y * S, (x + w) * S, (y + h) * S], 4 * S, fill=(5, 4, 9, 255), outline=(0, 0, 0, 255), width=2 * S)
    d.rounded_rectangle([(x - 2) * S, (y - 2) * S, (x + w + 2) * S, (y + h + 2) * S], 5 * S, outline=(110, 110, 125, 255), width=S)
    segs = 26; sw = (w - 8) / segs
    for i in range(segs):
        on = (i / segs) < level
        col = VIOLET if i < segs * 0.6 else ((255, 80, 150) if i < segs * 0.85 else RED)
        if on: glow(lambda g, c, i=i: g.rectangle([(x + 4 + i * sw) * S, (y + 4) * S, (x + 4 + (i + 1) * sw - 2) * S, (y + h - 4) * S], fill=c + (160,)), col, 2, 0.8)
        D().rectangle([(x + 4 + i * sw) * S, (y + 4) * S, (x + 4 + (i + 1) * sw - 2) * S, (y + h - 4) * S],
                      fill=(col + (255,)) if on else (34, 28, 44, 255))
    D().text((x * S, (y - 10) * S), label, font=font(10), fill=(205, 205, 215), anchor="lm")

# ======================= SCENE =======================
# background energy lines + big back plate
backplate = oct_pts(150, 105, 800, 420, 90)
lit(poly_mask(backplate), base=36, bevel=18, k=1.6, invert=True, mul=0.7)
glow(lambda g, c: [g.line([(550 * S, 300 * S), ((550 + dx) * S, (300 + dy) * S)], fill=c + (170,), width=2 * S)
                   for dx, dy in [(-330, -120), (330, -120), (-330, 130), (330, 130), (-380, 0), (380, 0), (-200, -190), (200, -190)]], VIOLET, 3, 0.9)

# PIPES & CABLES (behind panels)
pipe((255, 200), (330, 190), (380, 200), (420, 230), 13)
pipe((255, 370), (330, 390), (380, 330), (430, 300), 13, glow_col=RED)
pipe((845, 200), (770, 190), (720, 200), (680, 230), 13)
pipe((845, 370), (770, 390), (720, 330), (670, 300), 13, glow_col=VIOLET)
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
inner = oct_pts(252, 28, 596, 60, 22); lit(poly_mask(inner), base=24, bevel=6, k=2.4, invert=True, mul=0.7)
D().line([(px * S, py * S) for px, py in inner + [inner[0]]], fill=(0, 0, 0, 255), width=2 * S)
metal_text((550, 56), "VONOX", 50)
emboss_text((550, 84), "AUTOMATIC VOCAL MIX ENGINE", 9, color=(205, 165, 240))
for px in (258, 842, 258, 842):
    pass
for px, py in [(262, 24), (838, 24), (262, 92), (838, 92)]: screw(px, py, 4.5)

# SIDE PANELS
panel(18, 118, 255, 330, title="TONE")
knob(80, 215, "LOW-MID", 0.35); knob(211, 215, "PRESENCE", 0.62, accent=VIOLET)
knob(80, 360, "AIR", 0.55, accent=VIOLET); knob(211, 360, "WARMTH", 0.4)
panel(827, 118, 255, 330, title="DYNAMICS")
knob(889, 215, "COMPRESS", 0.6); knob(1020, 215, "DE-ESS", 0.45, accent=VIOLET)
knob(889, 360, "SATURATE", 0.38, accent=VIOLET); knob(1020, 360, "CEILING", 0.8)

# CENTER: octagon frame, glass display, clamp cubes, vents
cx, cy = 550, 262
frame = [(cx + 158 * math.cos(math.radians(22.5 + 45 * i)), cy + 158 * math.sin(math.radians(22.5 + 45 * i))) for i in range(8)]
glow(lambda g, c: g.polygon([(px * S, py * S) for px, py in frame], outline=c + (255,), width=8 * S), RED, 14, 1.4)
dropshadow(frame, 9, 11, 235); lit(poly_mask(frame), base=125, bevel=14, k=2.8)
D().line([(px * S, py * S) for px, py in frame + [frame[0]]], fill=(3, 3, 6, 255), width=int(4 * S))
mid = [(cx + 130 * math.cos(math.radians(22.5 + 45 * i)), cy + 130 * math.sin(math.radians(22.5 + 45 * i))) for i in range(8)]
lit(poly_mask(mid), base=34, bevel=7, k=2.4, invert=True, mul=0.8)
D().line([(px * S, py * S) for px, py in mid + [mid[0]]], fill=(0, 0, 0, 255), width=3 * S)
glass = [(cx + 118 * math.cos(math.radians(22.5 + 45 * i)), cy + 118 * math.sin(math.radians(22.5 + 45 * i))) for i in range(8)]
yy, xx = np.mgrid[0:H * S, 0:W * S]; rr = np.hypot(xx - cx * S, yy - (cy - 8) * S) / (118 * S)
disp = np.zeros((H * S, W * S, 3), np.float32)
disp[..., 0] = np.clip(190 - 150 * rr, 25, 230); disp[..., 1] = np.clip(18 - 12 * rr, 3, 18); disp[..., 2] = np.clip(95 - 70 * rr, 12, 120)
gm = poly_mask(glass); canvas.paste(Image.fromarray(disp.astype(np.uint8)), (0, 0), gm)
# circuit veins in glass
dd = D()
for a in range(0, 360, 30):
    r0, r1 = 40, 112; x0 = cx + r0 * math.cos(math.radians(a)); y0 = cy + r0 * math.sin(math.radians(a))
    x1 = cx + r1 * math.cos(math.radians(a + 8)); y1 = cy + r1 * math.sin(math.radians(a + 8))
    dd.line([(x0 * S, y0 * S), (x1 * S, y1 * S)], fill=(255, 90, 120, 55), width=S)
# glass reflection
refl = Image.new("RGBA", canvas.size, (0, 0, 0, 0)); rd = ImageDraw.Draw(refl)
rd.pieslice([(cx - 105) * S, (cy - 125) * S, (cx + 105) * S, (cy + 40) * S], 200, 340, fill=(255, 255, 255, 38))
canvas.paste(refl, (0, 0), Image.composite(refl.getchannel("A"), Image.new("L", canvas.size, 0), gm))
D().line([(px * S, py * S) for px, py in glass + [glass[0]]], fill=(0, 0, 0, 255), width=2 * S)
d = D()
d.text((cx * S, (cy - 80) * S), "VOCAL PROFILE", font=font(13), fill=(255, 205, 225), anchor="mm")
d.rounded_rectangle([(cx - 92) * S, (cy - 64) * S, (cx + 92) * S, (cy - 34) * S], 4 * S, fill=(8, 0, 12, 235), outline=(150, 50, 110, 255), width=S)
d.text((cx * S, (cy - 49) * S), "◀  MODERN RAP  ▶", font=font(16), fill=WHITE, anchor="mm")
d.rounded_rectangle([(cx - 100) * S, (cy - 24) * S, (cx + 100) * S, (cy + 40) * S], 4 * S, fill=(8, 0, 12, 235), outline=(150, 50, 110, 255), width=S)
for i, line in enumerate(["Analysis complete:", "moderate low-mid, strong", "sibilance, variable dynamics"]):
    d.text((cx * S, (cy - 8 + i * 16) * S), line, font=font(10.5, FM), fill=(240, 210, 235), anchor="mm")
d.text((cx * S, (cy + 66) * S), "MIX AMOUNT  72 %", font=font(13), fill=(255, 205, 225), anchor="mm")
# side clamp cubes + vent slats around the octagon
cube(367, 212, 17); cube(733, 212, 17)
for side in (-1, 1):
    for k in range(5):
        sl = [(cx + side * (172 + 0), 330 - 24 + k * 9), (cx + side * 196, 330 - 24 + k * 9), (cx + side * 196, 330 - 20 + k * 9), (cx + side * 172, 330 - 20 + k * 9)]
        lit(poly_mask(sl), base=115, bevel=1.5, k=2.2)
        glow(lambda g, c, sl=sl: g.line([(sl[0][0] * S, (sl[0][1] + 5) * S), (sl[1][0] * S, (sl[1][1] + 5) * S)], fill=c + (230,), width=S), RED, 2, 1.2)

# AUTO MIX: chunky machine button
bx, by, bw, bh = 428, 428, 244, 74
btn = [(bx + 30, by), (bx + bw - 30, by), (bx + bw, by + 20), (bx + bw - 14, by + bh), (bx + 14, by + bh), (bx, by + 20)]
dropshadow(btn, 8, 10, 230); lit(poly_mask(btn), base=118, bevel=12, k=2.8)
D().line([(px * S, py * S) for px, py in btn + [btn[0]]], fill=(3, 3, 6, 255), width=int(3.5 * S))
bi = [(bx + 44, by + 11), (bx + bw - 44, by + 11), (bx + bw - 22, by + 24), (bx + bw - 30, by + bh - 11), (bx + 30, by + bh - 11), (bx + 22, by + 24)]
glow(lambda g, c: g.polygon([(px * S, py * S) for px, py in bi], fill=c + (230,)), RED, 14, 1.2)
lit(poly_mask(bi), base=60, bevel=6, k=2.4, invert=True, tint=(1.5, 0.55, 0.65), mul=1.2)
D().line([(px * S, py * S) for px, py in bi + [bi[0]]], fill=(255, 60, 80, 255), width=int(2.5 * S))
glow(lambda g, c: g.text(((bx + bw / 2) * S, (by + bh / 2 - 1) * S), "AUTO MIX", font=font(28), fill=c + (255,), anchor="mm"), RED, 6, 1.7)
D().text(((bx + bw / 2) * S, (by + bh / 2 - 1) * S), "AUTO MIX", font=font(28), fill=(255, 222, 228, 255), anchor="mm")
for px, py in [(bx + 18, by + 12), (bx + bw - 18, by + 12), (bx + 10, by + bh - 10), (bx + bw - 10, by + bh - 10)]: screw(px, py, 4)

# BOTTOM LEFT: meters in a bar with hex mesh
panel(18, 478, 400, 200, cut=18)
meter(44, 515, 346, 22, "INPUT", 0.55); meter(44, 561, 346, 22, "OUTPUT", 0.68); meter(44, 607, 346, 22, "GAIN REDUCTION", 0.30)
D().text((44 * S, 652 * S), "PEAK -6.2 dB     RMS -17.8 dB", font=font(11, FM), fill=(205, 195, 225), anchor="lm")

# BOTTOM RIGHT: controls
panel(682, 478, 400, 200, cut=18)
knob(746, 566, "MIX", 0.72, r=26, accent=VIOLET); knob(841, 566, "OUTPUT", 0.5, r=26)
button(905, 512, 150, 34, "BYPASS", False, RED, 13)
button(905, 556, 70, 34, "A", True, VIOLET, 14); button(985, 556, 70, 34, "B", False, VIOLET, 14)
button(905, 600, 150, 34, "LEVEL MATCH", True, RED, 12)

# BOTTOM CENTER: hexagon emblem plate with cubes
hexp = [(550 + 112 * math.cos(math.radians(60 * i + 0)), 598 + 78 * math.sin(math.radians(60 * i + 0))) for i in range(6)]
dropshadow(hexp, 8, 10, 230); lit(poly_mask(hexp), base=125, bevel=12, k=2.8)
D().line([(px * S, py * S) for px, py in hexp + [hexp[0]]], fill=(3, 3, 6, 255), width=int(3.5 * S))
hi = [(550 + 90 * math.cos(math.radians(60 * i)), 598 + 60 * math.sin(math.radians(60 * i))) for i in range(6)]
lit(poly_mask(hi), base=24, bevel=6, k=2.2, invert=True, mul=0.7)
D().line([(px * S, py * S) for px, py in hi + [hi[0]]], fill=(0, 0, 0, 255), width=2 * S)
emboss_text((550, 584), "ADVANCED  ▾", 14, color=(215, 215, 228))
glow(lambda g, c: g.line([(500 * S, 604 * S), (600 * S, 604 * S)], fill=c + (255,), width=2 * S), VIOLET, 3, 1.2)
emboss_text((550, 624), "VONOX  ·  v0.1 PREVIEW", 9, color=(150, 140, 175), f=FM)
cube(442, 570, 11, 0.9); cube(658, 570, 11, 0.9)

# vignette + save
arr = np.asarray(canvas).astype(np.float32)
v = np.hypot((xx - W * S / 2) / (W * S / 2), (yy - H * S / 2) / (H * S / 2)); vig = np.clip(1.12 - 0.4 * v ** 2, 0.4, 1.0)
out = Image.fromarray(np.clip(arr * vig[..., None], 0, 255).astype(np.uint8)).resize((W, H), Image.LANCZOS)
out.save(sys.argv[1] if len(sys.argv) > 1 else "vonox_ui_preview.png"); print("saved")
