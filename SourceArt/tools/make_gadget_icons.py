"""Builds the 256x256 gadget icons in SourceArt/icons/ from the CC0 Poly Haven preview renders
(torch, pistol) plus hand-drawn shading for the thermometer and EMF meter.
Run with: python3 -I SourceArt/tools/make_gadget_icons.py"""
import colorsys
import os

from PIL import Image, ImageDraw, ImageFilter, ImageEnhance

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
DL = os.path.join(ROOT, 'downloads')
OUT = os.path.join(ROOT, 'icons')
S = 256


def fit(im, size=S, margin=14):
    """Crop to alpha bounds and centre on a transparent square."""
    im = im.crop(im.getbbox())
    k = (size - 2 * margin) / max(im.size)
    im = im.resize((max(1, int(im.width * k)), max(1, int(im.height * k))), Image.LANCZOS)
    canvas = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    canvas.alpha_composite(im, ((size - im.width) // 2, (size - im.height) // 2))
    return canvas


def glow(icon, color, radius=10, strength=0.55):
    """Soft coloured halo behind the object so it reads on a dark HUD."""
    a = icon.getchannel('A').filter(ImageFilter.GaussianBlur(radius))
    a = a.point(lambda v: int(v * strength))
    halo = Image.new('RGBA', icon.size, color + (0,))
    halo.putalpha(a)
    out = Image.new('RGBA', icon.size, (0, 0, 0, 0))
    out.alpha_composite(halo)
    out.alpha_composite(icon)
    return out


def hue_shift(im, hue, sat_mul=1.0):
    px = im.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = px[x, y]
            if a == 0:
                continue
            h, l, s = colorsys.rgb_to_hls(r / 255, g / 255, b / 255)
            if s > 0.25:  # only the coloured plastic, keep black rubber / chrome
                h = hue
                s = min(1.0, s * sat_mul)
                l = l * 0.75
            r2, g2, b2 = colorsys.hls_to_rgb(h, l, s)
            px[x, y] = (int(r2 * 255), int(g2 * 255), int(b2 * 255), a)
    return im


def torch_src():
    return Image.open(os.path.join(DL, 'small_plastic_torch', 'preview.png')).convert('RGBA')


def make_torch():
    return glow(fit(torch_src()), (255, 220, 150), strength=0.35)


def make_uv():
    im = hue_shift(torch_src(), 0.76, 0.9)
    icon = fit(im)
    # Violet lens glow at the head (bottom-right of the render).
    lens = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(lens)
    d.ellipse((150, 120, 236, 206), fill=(170, 90, 255, 150))
    lens = lens.filter(ImageFilter.GaussianBlur(14))
    out = glow(icon, (150, 80, 255), strength=0.5)
    out.alpha_composite(lens)
    return out


def make_gun():
    src = Image.open(os.path.join(DL, 'service_pistol', 'preview.png')).convert('RGBA')
    pistol = src.crop((135, 255, 512, 512))
    # Drop the loose bullet sitting next to the grip.
    px = pistol.load()
    for y in range(140, pistol.height):
        for x in range(150, 190):
            px[x, y] = (0, 0, 0, 0)
    pistol = pistol.crop(pistol.getbbox())
    # Small black torch slung under the barrel: the torch render, desaturated and darkened.
    t = ImageEnhance.Color(torch_src()).enhance(0.0)
    t = ImageEnhance.Brightness(t).enhance(0.35)
    t = t.crop(t.getbbox()).rotate(24, expand=True, resample=Image.BICUBIC)
    tw = int(pistol.width * 0.40)
    t = t.resize((tw, int(t.height * tw / t.width)), Image.LANCZOS)
    canvas = Image.new('RGBA', (pistol.width + 10, pistol.height + 10), (0, 0, 0, 0))
    canvas.alpha_composite(pistol, (0, 0))
    canvas.alpha_composite(t, (pistol.width - tw - 22, int(pistol.height * 0.12)))
    canvas.alpha_composite(pistol.crop((0, 0, pistol.width, int(pistol.height * 0.24))), (0, 0))
    return glow(fit(canvas), (255, 150, 70), strength=0.4)


def shade_box(d, box, top, bottom, radius):
    """Vertical gradient rounded rectangle."""
    x0, y0, x1, y1 = box
    for i in range(y1 - y0):
        t = i / max(1, y1 - y0 - 1)
        c = tuple(int(top[k] + (bottom[k] - top[k]) * t) for k in range(3)) + (255,)
        d.line((x0, y0 + i, x1, y0 + i), fill=c)


def rounded(img_size, box, radius):
    m = Image.new('L', img_size, 0)
    ImageDraw.Draw(m).rounded_rectangle(box, radius, fill=255)
    return m


def device(box, top, bottom, radius=18):
    layer = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    shade_box(ImageDraw.Draw(layer), box, top, bottom, radius)
    layer.putalpha(rounded((S, S), box, radius))
    # Edge highlight.
    hl = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(hl).rounded_rectangle(box, radius, outline=(255, 255, 255, 45), width=2)
    layer.alpha_composite(hl)
    return layer


def make_thermo():
    img = device((78, 40, 178, 236), (215, 150, 25), (120, 78, 8), 22)   # yellow rubber holster
    face = device((88, 52, 168, 226), (64, 66, 70), (30, 31, 34), 14)   # grey face plate
    img.alpha_composite(face)
    d = ImageDraw.Draw(img)
    # Probe on top.
    d.rounded_rectangle((121, 6, 135, 46), 6, fill=(170, 172, 176, 255))
    d.line((124, 8, 124, 44), fill=(220, 222, 226, 255), width=2)
    # Screen.
    d.rounded_rectangle((90, 62, 166, 124), 8, fill=(15, 16, 18, 255))
    d.rounded_rectangle((95, 67, 161, 119), 5, fill=(120, 200, 230, 255))
    # 7-segment-ish "4.2"
    seg = (10, 30, 40, 255)
    d.rectangle((103, 78, 109, 104), fill=seg)
    d.rectangle((103, 90, 121, 95), fill=seg)
    d.rectangle((115, 76, 121, 110), fill=seg)
    d.rectangle((126, 104, 130, 110), fill=seg)
    for r in ((135, 76, 153, 81), (148, 76, 153, 94), (135, 90, 153, 95), (135, 90, 140, 110), (135, 105, 153, 110)):
        d.rectangle(r, fill=seg)
    # Buttons.
    for cx, col in ((104, (220, 160, 30)), (128, (90, 92, 98)), (152, (90, 92, 98))):
        d.ellipse((cx - 9, 146, cx + 9, 164), fill=col + (255,), outline=(20, 20, 20, 255), width=2)
    # Grip ribs.
    d.rectangle((104, 190, 152, 196), fill=(150, 110, 40, 255))  # label strip
    return glow(img, (110, 200, 255), strength=0.35)


def make_emf():
    img = device((72, 22, 184, 238), (40, 44, 42), (14, 15, 15), 20)
    d = ImageDraw.Draw(img)
    # LED row.
    leds = [(60, 230, 70), (160, 240, 40), (250, 220, 30), (255, 130, 20), (240, 40, 30)]
    d.rounded_rectangle((82, 36, 174, 70), 6, fill=(8, 8, 8, 255))
    for i, c in enumerate(leds):
        cx = 92 + i * 18
        lit = i < 4
        col = c if lit else tuple(v // 6 for v in c)
        d.ellipse((cx - 7, 46, cx + 7, 60), fill=col + (255,))
        if lit:
            d.ellipse((cx - 3, 48, cx + 1, 52), fill=(255, 255, 230, 200))
    # Label panel.
    d.rounded_rectangle((86, 86, 170, 136), 4, fill=(190, 182, 150, 255))
    d.text((98, 96), "EMF", fill=(30, 30, 30, 255))
    d.text((98, 114), "K-II", fill=(30, 30, 30, 255))
    # Button and grip.
    d.ellipse((116, 150, 140, 174), fill=(170, 30, 30, 255), outline=(10, 10, 10, 255), width=2)
    for y in range(190, 230, 8):
        d.line((86, y, 170, y), fill=(6, 6, 6, 255), width=3)
    out = glow(img, (100, 255, 110), strength=0.3)
    halo = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(halo).rectangle((84, 40, 160, 66), fill=(255, 200, 60, 120))
    out.alpha_composite(halo.filter(ImageFilter.GaussianBlur(9)))
    out.alpha_composite(img.crop((82, 36, 175, 71)), (82, 36))
    return out


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    for name, fn in (('Torch', make_torch), ('GunTorch', make_gun), ('UVTorch', make_uv),
                     ('Thermometer', make_thermo), ('EMFMeter', make_emf)):
        fn().save(os.path.join(OUT, 'T_Icon_%s.png' % name))
        print('wrote', name)
