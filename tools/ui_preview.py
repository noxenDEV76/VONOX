"""Renders a static PNG preview of the VONOX interface (original design:
dark steel panels, red/violet glow, white knobs). Preview only: the real UI
is drawn by JUCE in the plugin."""
import math, random
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

S = 2                      # supersampling
W, H = 1100, 700
RED, VIOLET, WHITE = (255, 40, 60), (150, 70, 255), (240, 240, 245)
F = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
FM = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
def font(sz, path=F): return ImageFont.truetype(path, int(sz * S))
random.seed(7); np.random.seed(7)

def steel(w, h, base=34):
    """Brushed dark metal texture."""
    n = np.random.normal(base, 7, (h, 1)).repeat(w, axis=1) + np.random.normal(0, 4, (h, w))
    n = np.clip(n, 8, 90).astype(np.uint8)
    return Image.merge("RGB", [Image.fromarray(n)] * 3)

img = steel(W * S, H * S, 22)
d = ImageDraw.Draw(img, "RGBA")

def glow(layer_fn, color, radius, strength=1.0):
    g = Image.new("RGBA", img.size, (0, 0, 0, 0))
    layer_fn(ImageDraw.Draw(g), color)
    g = g.filter(ImageFilter.GaussianBlur(radius * S))
    if strength != 1.0:
        a = g.getchannel("A").point(lambda v: min(255, int(v * strength))); g.putalpha(a)
    img.paste(g, (0, 0), g)

def bevel_panel(x, y, w, h, cut=22, title=None):
    pts = [(x+cut, y), (x+w-cut, y), (x+w, y+cut), (x+w, y+h-cut),
           (x+w-cut, y+h), (x+cut, y+h), (x, y+h-cut), (x, y+cut)]
    pts = [(px*S, py*S) for px, py in pts]
    tex = steel(W*S, H*S, 40)
    m = Image.new("L", img.size, 0); ImageDraw.Draw(m).polygon(pts, fill=255)
    img.paste(tex, (0, 0), m)
    dd = ImageDraw.Draw(img, "RGBA")
    dd.line(pts + [pts[0]], fill=(10, 10, 14, 255), width=5*S)
    dd.line(pts[:5], fill=(120, 120, 135, 200), width=2*S)            # top/right highlight
    dd.line(pts[4:] + [pts[0]], fill=(0, 0, 0, 220), width=2*S)       # bottom/left shadow
    for (px, py) in [(x+cut+4, y+8), (x+w-cut-4, y+8), (x+cut+4, y+h-8), (x+w-cut-4, y+h-8)]:
        dd.ellipse([(px-3)*S, (py-3)*S, (px+3)*S, (py+3)*S], fill=(70, 70, 80), outline=(8, 8, 10))
    if title:
        dd.text(((x+w/2)*S, (y+22)*S), title, font=font(15), fill=(235, 235, 240), anchor="mm")
        glow(lambda g, c: g.line([((x+w/2-40)*S, (y+36)*S), ((x+w/2+40)*S, (y+36)*S)], fill=c+(255,), width=2*S),
             VIOLET, 3)

def knob(cx, cy, label, value, r=30, accent=RED):
    cx *= S; cy *= S; R = r * S
    # outer glow ring (value arc)
    def arc(g, c):
        g.arc([cx-R-6*S, cy-R-6*S, cx+R+6*S, cy+R+6*S], 135, 135 + 270*value, fill=c+(255,), width=3*S)
    glow(arc, accent, 4, 1.4)
    d2 = ImageDraw.Draw(img, "RGBA")
    d2.arc([cx-R-6*S, cy-R-6*S, cx+R+6*S, cy+R+6*S], 135, 405, fill=(0, 0, 0, 200), width=3*S)
    d2.arc([cx-R-6*S, cy-R-6*S, cx+R+6*S, cy+R+6*S], 135, 135 + 270*value, fill=accent+(255,), width=3*S)
    # body: white knob with radial shading
    k = Image.new("RGBA", (int(2*R+2), int(2*R+2)), (0, 0, 0, 0)); kd = ImageDraw.Draw(k)
    for i in range(int(R), 0, -1):
        t = i / R; v = int(150 + 100 * (1 - t) ** 0.8)
        kd.ellipse([R-i, R-i, R+i, R+i], fill=(v, v, min(255, v+6), 255))
    kd.ellipse([2, 2, 2*R-2, 2*R-2], outline=(25, 25, 30, 255), width=3*S)
    for i in range(36):                                               # knurled edge
        a = i * 10 * math.pi / 180
        kd.line([R+math.cos(a)*(R-6*S), R+math.sin(a)*(R-6*S), R+math.cos(a)*(R-2), R+math.sin(a)*(R-2)],
                fill=(120, 120, 130, 255), width=1)
    ang = math.radians(135 + 270 * value)
    kd.line([R+math.cos(ang)*R*0.25, R+math.sin(ang)*R*0.25, R+math.cos(ang)*R*0.82, R+math.sin(ang)*R*0.82],
            fill=accent+(255,), width=3*S)
    shadow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(shadow).ellipse([cx-R+4*S, cy-R+8*S, cx+R+4*S, cy+R+8*S], fill=(0, 0, 0, 190))
    img.paste(shadow.filter(ImageFilter.GaussianBlur(5*S)), (0, 0), shadow.filter(ImageFilter.GaussianBlur(5*S)))
    img.paste(k, (int(cx-R), int(cy-R)), k)
    # label plate
    ImageDraw.Draw(img, "RGBA").rounded_rectangle([cx-34*S, cy+R+12*S, cx+34*S, cy+R+30*S], 4*S,
                                                  fill=(14, 14, 18, 230), outline=(90, 90, 100, 255))
    ImageDraw.Draw(img, "RGBA").text((cx, cy+R+21*S), label, font=font(11), fill=(225, 225, 232), anchor="mm")

def button(x, y, w, h, text, active=False, color=RED, fs=14):
    def g(gd, c): gd.rounded_rectangle([x*S, y*S, (x+w)*S, (y+h)*S], 8*S, fill=c+((200 if active else 0),))
    if active: glow(g, color, 8, 1.2)
    dd = ImageDraw.Draw(img, "RGBA")
    dd.rounded_rectangle([x*S, y*S, (x+w)*S, (y+h)*S], 8*S,
                         fill=(color[0]//3, color[1]//3, color[2]//3, 255) if active else (22, 22, 28, 255),
                         outline=color+(255,) if active else (95, 95, 108, 255), width=2*S)
    dd.text(((x+w/2)*S, (y+h/2)*S), text, font=font(fs), fill=WHITE if active else (170, 170, 182), anchor="mm")

def meter(x, y, w, h, label, level, gr=False):
    dd = ImageDraw.Draw(img, "RGBA")
    dd.rounded_rectangle([x*S, y*S, (x+w)*S, (y+h)*S], 4*S, fill=(6, 6, 9, 255), outline=(80, 80, 92, 255), width=S)
    segs = 24; sw = (w - 8) / segs
    for i in range(segs):
        on = (i / segs) < level
        col = VIOLET if i < segs*0.6 else ((255, 90, 150) if i < segs*0.85 else RED)
        c = col + (255,) if on else (36, 30, 44, 255)
        dd.rectangle([(x+4+i*sw)*S, (y+4)*S, (x+4+(i+1)*sw-2)*S, (y+h-4)*S], fill=c)
    dd.text((x*S, (y-9)*S), label, font=font(10), fill=(200, 200, 210), anchor="lm")

# ---------- background tech lines (original circuit-like pattern) ----------
glow(lambda g, c: [g.line([(550*S, 330*S), (550*S + dx*S, 330*S + dy*S)], fill=c+(150,), width=2*S)
                   for dx, dy in [(-250, -150), (250, -150), (-250, 120), (250, 120), (-300, 0), (300, 0)]],
     VIOLET, 3, 0.8)

# ---------- title plate ----------
bevel_panel(250, 14, 600, 82, cut=26)
d = ImageDraw.Draw(img, "RGBA")
def title(g, c): g.text((550*S, 56*S), "VONOX", font=font(54), fill=c+(255,), anchor="mm")
glow(title, RED, 10, 1.6); glow(title, VIOLET, 18, 0.9)
d = ImageDraw.Draw(img, "RGBA")
d.text((550*S, 56*S), "VONOX", font=font(54), fill=(235, 190, 200, 255), anchor="mm")
d.text((550*S, 86*S), "AUTOMATIC VOCAL MIX ENGINE", font=font(10), fill=(190, 150, 230), anchor="mm")

# ---------- left panel: TONE ----------
bevel_panel(18, 120, 255, 330, title="TONE")
knob(80, 215, "LOW-MID", 0.35); knob(210, 215, "PRESENCE", 0.62, accent=VIOLET)
knob(80, 360, "AIR", 0.55, accent=VIOLET); knob(210, 360, "WARMTH", 0.4)

# ---------- right panel: DYNAMICS ----------
bevel_panel(827, 120, 255, 330, title="DYNAMICS")
knob(890, 215, "COMPRESS", 0.6); knob(1020, 215, "DE-ESS", 0.45, accent=VIOLET)
knob(890, 360, "SATURATE", 0.38, accent=VIOLET); knob(1020, 360, "CEILING", 0.8)

# ---------- center display ----------
cx, cy = 550, 262
oct_pts = [(cx + 150*math.cos(math.radians(22.5 + 45*i)), cy + 150*math.sin(math.radians(22.5 + 45*i))) for i in range(8)]
oct_pts = [(px*S, py*S) for px, py in oct_pts]
glow(lambda g, c: g.polygon(oct_pts, outline=c+(255,), width=6*S), RED, 12, 1.5)
tex = steel(W*S, H*S, 38); m = Image.new("L", img.size, 0); ImageDraw.Draw(m).polygon(oct_pts, fill=255)
img.paste(tex, (0, 0), m)
d = ImageDraw.Draw(img, "RGBA"); d.line(oct_pts + [oct_pts[0]], fill=(10, 10, 14, 255), width=6*S)
inner = [(cx*S + (px-cx*S)*0.84, cy*S + (py-cy*S)*0.84) for px, py in oct_pts]
disp = np.zeros((H*S, W*S, 3), np.float32)
yy, xx = np.mgrid[0:H*S, 0:W*S]; r = np.hypot(xx-cx*S, yy-cy*S) / (126*S)
disp[..., 0] = np.clip(150 - 120*r, 20, 200) ; disp[..., 1] = np.clip(15 - 10*r, 3, 15) ; disp[..., 2] = np.clip(70 - 50*r, 10, 100)
dimg = Image.fromarray(disp.astype(np.uint8)); m2 = Image.new("L", img.size, 0); ImageDraw.Draw(m2).polygon(inner, fill=255)
img.paste(dimg, (0, 0), m2)
d = ImageDraw.Draw(img, "RGBA"); d.line(inner + [inner[0]], fill=(0, 0, 0, 255), width=3*S)
d.text((cx*S, (cy-78)*S), "VOCAL PROFILE", font=font(13), fill=(235, 190, 220), anchor="mm")
d.rounded_rectangle([(cx-95)*S, (cy-62)*S, (cx+95)*S, (cy-30)*S], 4*S, fill=(8, 0, 10, 235), outline=(110, 40, 90, 255), width=S)
d.text((cx*S, (cy-46)*S), "◀  MODERN RAP  ▶", font=font(16), fill=WHITE, anchor="mm")
d.rounded_rectangle([(cx-105)*S, (cy-18)*S, (cx+105)*S, (cy+52)*S], 4*S, fill=(8, 0, 10, 235), outline=(110, 40, 90, 255), width=S)
for i, line in enumerate(["Analysis complete:", "moderate low-mid, strong", "sibilance, variable dynamics"]):
    d.text((cx*S, (cy-2+i*17)*S), line, font=font(11, FM), fill=(230, 200, 230), anchor="mm")
d.text((cx*S, (cy+82)*S), "MIX AMOUNT  72 %", font=font(13), fill=(235, 190, 220), anchor="mm")

# ---------- AUTO MIX button ----------
bx, by, bw, bh = 440, 438, 220, 62
def am(g, c): g.rounded_rectangle([bx*S, by*S, (bx+bw)*S, (by+bh)*S], 18*S, fill=c+(230,))
glow(am, RED, 16, 1.3)
d = ImageDraw.Draw(img, "RGBA")
d.rounded_rectangle([bx*S, by*S, (bx+bw)*S, (by+bh)*S], 18*S, fill=(40, 6, 14, 255), outline=(255, 60, 80, 255), width=3*S)
def amt(g, c): g.text(((bx+bw/2)*S, (by+bh/2)*S), "AUTO MIX", font=font(26), fill=c+(255,), anchor="mm")
glow(amt, RED, 6, 1.5); d = ImageDraw.Draw(img, "RGBA")
d.text(((bx+bw/2)*S, (by+bh/2)*S), "AUTO MIX", font=font(26), fill=(255, 215, 220, 255), anchor="mm")

# ---------- bottom panels ----------
bevel_panel(18, 478, 400, 200, cut=18)
meter(40, 510, 350, 22, "INPUT", 0.55); meter(40, 556, 350, 22, "OUTPUT", 0.68)
meter(40, 602, 350, 22, "GAIN REDUCTION", 0.30)
ImageDraw.Draw(img, "RGBA").text((40*S, 650*S), "PEAK -6.2 dB    RMS -17.8 dB", font=font(11, FM), fill=(200, 190, 215), anchor="lm")

bevel_panel(682, 478, 400, 200, cut=18)
knob(745, 560, "MIX", 0.72, r=26, accent=VIOLET); knob(840, 560, "OUTPUT", 0.5, r=26)
button(905, 512, 150, 34, "BYPASS", False, RED, 13)
button(905, 556, 70, 34, "A", True, VIOLET, 14); button(985, 556, 70, 34, "B", False, VIOLET, 14)
button(905, 600, 150, 34, "LEVEL MATCH", True, RED, 12)

# center logo plate
bevel_panel(430, 560, 240, 118, cut=16)
ImageDraw.Draw(img, "RGBA").text((550*S, 600*S), "ADVANCED  ▾", font=font(13), fill=(210, 210, 222), anchor="mm")
ImageDraw.Draw(img, "RGBA").text((550*S, 640*S), "v0.1 PREVIEW", font=font(10, FM), fill=(140, 130, 160), anchor="mm")

# vignette
v = np.hypot((xx - W*S/2) / (W*S/2), (yy - H*S/2) / (H*S/2)); vig = np.clip(1.15 - 0.45*v**2, 0.35, 1.0)
arr = np.asarray(img.convert("RGB")).astype(np.float32) * vig[..., None]
Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8)).resize((W, H), Image.LANCZOS).save("vonox_ui_preview.png")
print("saved vonox_ui_preview.png")
