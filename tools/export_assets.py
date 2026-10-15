"""Exports the VONOX UI art as plugin assets (plugin/assets).

Runs the same renderer as the preview in asset-export mode:
  background.png       full 1100x700 artwork on a TRANSPARENT background (panels, chains, plates) without knob bodies / arcs / meter segments / dynamic text
  knob_body_29/26.png  white knob bodies (RGBA, rotated live by the plugin)
  btn_*_on/off.png     button sprites (both states)
  fire_title.jpg       72-frame vertical strip of the animated title fire (630x88 per frame)
  fire_auto.jpg        72-frame vertical strip of the animated AUTO MIX fire (252x82 per frame)
Usage: VONOX_EXPORT=1 python3 tools/export_assets.py plugin/assets
"""
import os, sys
os.environ["VONOX_EXPORT"] = "1"
sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from PIL import Image
import ui_animate as U

out = sys.argv[1] if len(sys.argv) > 1 else "assets"
os.makedirs(out, exist_ok=True)
S, W, H = U.S, U.W, U.H

def vignetted(img_s_res, rect=None):
    arr = np.asarray(img_s_res, np.float32)
    vig = U.VIG if rect is None else U.VIG[rect[1]:rect[3], rect[0]:rect[2]]
    return Image.fromarray(np.clip(arr * vig[..., None], 0, 255).astype(np.uint8))

# ---- transparent panel layer: the scene is rendered on a black and on a white backdrop; difference matting gives alpha
if os.environ.get("VONOX_BG") == "white":
    import pickle
    np.save(os.path.join(out, "_white_base.npy"), np.asarray(U.BASE, np.uint8))
    pickle.dump({k: (np.asarray(v[0], np.uint8), v[1]) for k, v in U.SPRITES.items()}, open(os.path.join(out, "_white_sprites.pkl"), "wb"))
    sys.exit(0)
import subprocess, pickle
subprocess.run([sys.executable, os.path.abspath(__file__), out], env=dict(os.environ, VONOX_BG="white"), check=True)
WB = np.load(os.path.join(out, "_white_base.npy")).astype(np.float32); WS = pickle.load(open(os.path.join(out, "_white_sprites.pkl"), "rb"))

def matte(black_img, white_arr, rect=None, vig=True):
    """black/white renders of the same layer -> straight-alpha RGBA PIL image at final size."""
    Bk = np.asarray(black_img, np.float32); Wt = white_arr.astype(np.float32)
    alpha = np.clip(1.0 - (Wt - Bk).mean(axis=2) / 255.0, 0, 1)
    v = np.ones(Bk.shape[:2], np.float32)
    if vig: v = U.VIG if rect is None else U.VIG[rect[1]:rect[3], rect[0]:rect[2]]
    pre = np.dstack([Bk * v[..., None], alpha * 255.0])              # premultiplied colour (with vignette) + alpha
    h, w = pre.shape[:2]
    chans = [Image.fromarray(np.clip(pre[..., i], 0, 255).astype(np.uint8)).resize((w // S, h // S), Image.LANCZOS) for i in range(4)]
    rgba = np.dstack([np.asarray(c, np.float32) for c in chans])
    a = rgba[..., 3:4] / 255.0
    rgb = np.where(a > 0.004, rgba[..., :3] / np.maximum(a, 0.004), 0.0)
    return Image.fromarray(np.dstack([np.clip(rgb, 0, 255), rgba[..., 3:4]]).astype(np.uint8), "RGBA")

matte(U.BASE, WB).save(os.path.join(out, "background.png"), optimize=True)

# knob bodies
for r in (29, 26):
    U.knob_body(r).save(os.path.join(out, f"knob_body_{r}.png"), optimize=True)

# button sprites: RGBA (transparent around the button so the star field shows through)
for name, (img, rect) in U.SPRITES.items():
    matte(img, WS[name][0].astype(np.float32), rect).save(os.path.join(out, name + ".png"), optimize=True)

# animated fire strips (72 frames, 24 fps loop = 3 s), opaque JPEG crops of the finished frames
N = 72
title_rect, auto_rect = (235, 14, 865, 102), (424, 424, 676, 506)
ts = Image.new("RGB", (title_rect[2] - title_rect[0], (title_rect[3] - title_rect[1]) * N))
as_ = Image.new("RGB", (auto_rect[2] - auto_rect[0], (auto_rect[3] - auto_rect[1]) * N))
for i in range(N):
    f = U.render_frame(i / N, shine=False)
    ts.paste(f.crop(title_rect), (0, i * (title_rect[3] - title_rect[1])))
    as_.paste(f.crop(auto_rect), (0, i * (auto_rect[3] - auto_rect[1])))
ts.save(os.path.join(out, "fire_title.jpg"), quality=88)
as_.save(os.path.join(out, "fire_auto.jpg"), quality=90)
for tmp in ("_white_base.npy", "_white_sprites.pkl"): os.remove(os.path.join(out, tmp))
for fn in sorted(os.listdir(out)): print(fn, os.path.getsize(os.path.join(out, fn)) // 1024, "KB")
