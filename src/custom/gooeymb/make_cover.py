"""GooeyMB cover: the live meter (src/custom/common/zdl_meter.h) under Gooey's title with "MB".

Run from anywhere:  py src/custom/gooeymb/make_cover.py
The build only reads title.txt (Gooey's title rows with MB on the right). This writes
covers/GooeyMB.json (frame A: default knobs, empty history) and cover_preview.png next to it.
"""
import json, math, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]                            # src/custom/gooeymb -> repo root
sys.path.insert(0, str(ROOT / "build"))
sys.path.insert(0, str(ROOT / "src" / "airwindows" / "common"))
import custom_covers as cc

NAME, FOLDER = "GooeyMB", "gooeymb"

if __name__ == "__main__":
    m = json.loads((HERE / "manifest_pedal.json").read_text(encoding="utf-8"))
    by, tags = cc.meter_geometry(FOLDER, [p["default"] for p in m["params"]])
    c = cc.meter_frame(cc.meter_title(FOLDER), by, tags)
    out = ROOT / "src" / "airwindows" / "common" / "covers" / (NAME + ".json")
    out.write_text(json.dumps({"name": NAME, "w": 128, "h": 64, "grid": c.pixels}))
    print("wrote", out)
    from lcd_render import sheet
    # preview = stock frame A | one plausible live frame (the real one is drawn by the C meter)
    top = len(cc.meter_title(FOLDER))
    rows = [64 if x < 40 else min(63, max(top, round(60 - 16 * (1 + math.sin(x / 9)) * (x - 40) / 88))) for x in range(128)]
    live = cc.meter_frame(cc.meter_title(FOLDER), by, tags, rows)
    sheet([c.pixels, live.pixels], str(HERE / "cover_preview.png"), cols=2)
