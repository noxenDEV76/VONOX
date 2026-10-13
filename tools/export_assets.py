"""Exports the VONOX UI art as plugin assets (plugin/assets).

Runs the same renderer as the preview in asset-export mode:
  background.png       full 1100x700 artwork without knob bodies / arcs / meter segments / dynamic text
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

# background (static artwork: BASE already contains frames/plates/letters/glass gradient)
bg = vignetted(U.BASE).resize((W, H), Image.LANCZOS)
bg.save(os.path.join(out, "background.png"), optimize=True)

# knob bodies
for r in (29, 26):
    U.knob_body(r).save(os.path.join(out, f"knob_body_{r}.png"), optimize=True)

# button sprites (apply the same vignette so they blend into the background)
for name, (img, rect) in U.SPRITES.items():
    vignetted(img, rect).resize(((rect[2] - rect[0]) // S, (rect[3] - rect[1]) // S), Image.LANCZOS).save(os.path.join(out, name + ".png"), optimize=True)

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
for fn in sorted(os.listdir(out)): print(fn, os.path.getsize(os.path.join(out, fn)) // 1024, "KB")
