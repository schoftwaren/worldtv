#!/usr/bin/env python3
"""itch kit art for worldtv v1.0.0 — one master composition, all specs.

Real game renders only (shot driver: build-embed/worldtv --screenshot under
xvfb, WORLDTV_AUTOPLAY=RedBullTV.at). Re-run: python3 tools/make_itch_art.py
Outputs into itch/: cover_630x500.png, banner_1200x480.png, icon_512.png,
shots/ (the 3 kit screenshots).
"""
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOT = os.path.join(ROOT, "itch", "shots")
KIT = "/tmp/wtv-kit"  # fresh capture dir (see itch/README.md)
OUT = os.path.join(ROOT, "itch")
LOGO = os.path.join(ROOT, "res", "worldtv_512.png")

BROWSE = os.path.join(KIT, "wtv_browse.png")
PLAY = os.path.join(KIT, "wtv_play.png")

ACCENT = (79, 195, 247)   # tk.accent
BG = (16, 20, 24)         # tk.bg
PANEL = (24, 30, 36)      # tk.panel
TEXT = (232, 237, 242)    # tk.text

F_REG = "/usr/share/fonts/noto/NotoSans-Regular.ttf"
F_BOLD = "/usr/share/fonts/noto/NotoSans-Bold.ttf"
F_BLACK = "/usr/share/fonts/noto/NotoSans-Black.ttf"


def fit_font(path, text, target_w, start):
    """Shrink-to-fit by getbbox (house rule: fixed columns always fit)."""
    size = start
    while size > 10:
        f = ImageFont.truetype(path, size)
        w = f.getbbox(text)[2] - f.getbbox(text)[0]
        if w <= target_w:
            return f
        size -= 2
    return ImageFont.truetype(path, 12)


def cover_crop(img, w, h):
    """Center-crop to w:h aspect, then scale."""
    iw, ih = img.size
    ta = w / h
    ia = iw / ih
    if ia > ta:
        nw = int(ih * ta)
        img = img.crop(((iw - nw) // 2, 0, (iw - nw) // 2 + nw, ih))
    else:
        nh = int(iw / ta)
        img = img.crop((0, (ih - nh) // 2, iw, (ih - nh) // 2 + nh))
    return img.resize((w, h), Image.LANCZOS)


def rounded(img, radius):
    mask = Image.new("L", img.size, 0)
    d = ImageDraw.Draw(mask)
    d.rounded_rectangle([0, 0, img.size[0] - 1, img.size[1] - 1],
                        radius=radius, fill=255)
    out = Image.new("RGBA", img.size, (0, 0, 0, 0))
    out.paste(img, (0, 0), mask)
    return out


def drop_shadow(base, card, xy):
    """Paste card with a soft shadow at xy."""
    x, y = xy
    sh = Image.new("RGBA", base.size, (0, 0, 0, 0))
    amask = card.split()[3].point(lambda a: int(a * 0.55))
    sh.paste((0, 0, 0, 255), (x + 6, y + 8), amask)
    sh = sh.filter(ImageFilter.GaussianBlur(10))
    base.alpha_composite(sh)
    base.alpha_composite(card, (x, y))


def draw_globe(d, cx, cy, r):
    """Flat globe glyph matching the app's brand mark."""
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=ACCENT)
    # meridians / parallels cut out in bg color
    d.ellipse([cx - r * 0.45, cy - r, cx + r * 0.45, cy + r],
              outline=BG, width=max(2, r // 12))
    d.line([cx - r, cy, cx + r, cy], fill=BG, width=max(2, r // 12))
    d.arc([cx - r, cy - r, cx + r, cy + r], 200, 340, fill=BG,
          width=max(2, r // 12))


def title_lockup(canvas, cx, cy, size, word="WORLD TV"):
    """Globe + two-tone wordmark, returns total width."""
    f = ImageFont.truetype(F_BLACK, size)
    d = ImageDraw.Draw(canvas)
    w_world = f.getbbox("WORLD")[2] - f.getbbox("WORLD")[0]
    w_tv = f.getbbox("TV")[2] - f.getbbox("TV")[0]
    gap = int(size * 0.18)
    gr = int(size * 0.42)
    total = gr * 2 + int(size * 0.22) + w_world + gap + w_tv
    x = cx - total // 2
    draw_globe(d, x + gr, cy, gr)
    d.text((x + gr * 2 + int(size * 0.22), cy), "WORLD",
           font=f, fill=TEXT, anchor="lm")
    d.text((x + gr * 2 + int(size * 0.22) + w_world + gap, cy), "TV",
           font=f, fill=ACCENT, anchor="lm")
    return total


def tagline(canvas, cx, y, text, size, width):
    f = fit_font(F_REG, text, width, size)
    ImageDraw.Draw(canvas).text((cx, y), text, font=f, fill=(143, 161, 179),
                                anchor="mm")


def make_cover():
    c = Image.new("RGBA", (630, 500), BG)
    # hero: the live-play frame as a big card
    hero = cover_crop(Image.open(PLAY).convert("RGBA"), 560, 400)
    hero = rounded(hero, 18)
    drop_shadow(c, hero, (35, 62))
    # lockup on top band
    title_lockup(c, 315, 42, 40)
    tagline(c, 315, 470, "10,288 live channels. One couch.", 26, 560)
    c.convert("RGB").save(os.path.join(OUT, "cover_630x500.png"))
    print("cover_630x500.png")


def make_banner():
    c = Image.new("RGBA", (1200, 480), BG)
    # wide browse-grid strip as the backdrop card
    strip = cover_crop(Image.open(BROWSE).convert("RGBA"), 1130, 390)
    strip = rounded(strip, 18)
    drop_shadow(c, strip, (35, 62))
    # translucent scrim for the lockup
    scrim = Image.new("RGBA", c.size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(scrim)
    sd.rounded_rectangle([35, 62, 1165, 452], radius=18, fill=(16, 20, 24, 150))
    c.alpha_composite(scrim)
    title_lockup(c, 600, 205, 84)
    tagline(c, 600, 300, "10,288 live channels. One couch.", 34, 900)
    tagline(c, 600, 360, "gamepad-ready player with mirror failover", 24, 900)
    c.convert("RGB").save(os.path.join(OUT, "banner_1200x480.png"))
    print("banner_1200x480.png")


def make_icon():
    src = Image.open(LOGO).convert("RGBA")
    src = src.resize((512, 512), Image.LANCZOS)
    src.save(os.path.join(OUT, "icon_512.png"))
    print("icon_512.png")


def stage_shots():
    os.makedirs(SHOT, exist_ok=True)
    names = [("wtv_browse.png", "01_browse_grid.png"),
             ("wtv_play.png", "02_live_player_redbull.png"),
             ("wtv_back.png", "03_back_to_grid.png")]
    for src, dst in names:
        p = os.path.join(KIT, src)
        if os.path.exists(p):
            Image.open(p).convert("RGB").save(os.path.join(SHOT, dst))
            print("shots/" + dst)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    make_cover()
    make_banner()
    make_icon()
    stage_shots()
