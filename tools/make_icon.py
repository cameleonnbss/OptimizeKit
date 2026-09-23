#!/usr/bin/env python3
"""Generate the OptimizeKit icon set (assets/optimizekit.ico + png sizes).

Design: rounded-square dark-glass tile, blue->violet diagonal gradient,
white lightning bolt with a subtle glow, thin top highlight. Pure PIL, no
external assets, deterministic output.
"""
import os

from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(ROOT, "assets")
S = 1024  # master size


def rounded_mask(size, radius):
    m = Image.new("L", (size, size), 0)
    d = ImageDraw.Draw(m)
    d.rounded_rectangle([0, 0, size - 1, size - 1], radius=radius, fill=255)
    return m


def base_tile():
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    # background gradient (deep navy -> darker) inside the rounded tile
    grad = Image.new("RGBA", (S, S))
    px = grad.load()
    c0 = (10, 12, 22)     # top-left
    c1 = (5, 6, 12)       # bottom-right
    for y in range(S):
        for x in range(0, S, 4):
            t = (x + y) / (2 * S)
            r = int(c0[0] + (c1[0] - c0[0]) * t)
            g = int(c0[1] + (c1[1] - c0[1]) * t)
            b = int(c0[2] + (c1[2] - c0[2]) * t)
            for dx in range(4):
                if x + dx < S:
                    px[x + dx, y] = (r, g, b, 255)
    img.paste(grad, (0, 0), rounded_mask(S, 230))

    # accent glow blob (blue) bottom-left + violet top-right, drawn then blurred
    glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse([-260, 560, 480, 1300], fill=(38, 132, 255, 110))
    gd.ellipse([560, -260, 1300, 480], fill=(148, 84, 255, 90))
    glow = glow.filter(ImageFilter.GaussianBlur(120))
    img.paste(glow, (0, 0), rounded_mask(S, 230))

    # top glass highlight band
    hi = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    hd = ImageDraw.Draw(hi)
    hd.rounded_rectangle([0, 0, S - 1, 300], radius=200, fill=(255, 255, 255, 26))
    hi = hi.filter(ImageFilter.GaussianBlur(40))
    img.alpha_composite(hi)

    # thin border
    bd = ImageDraw.Draw(img)
    bd.rounded_rectangle([2, 2, S - 3, S - 3], radius=228, outline=(255, 255, 255, 46), width=6)
    return img


def bolt_mask(size):
    """Lightning bolt path normalized in a 0..1 space, scaled to `size`."""
    pts = [
        (0.585, 0.060), (0.205, 0.540), (0.445, 0.540),
        (0.380, 0.940), (0.800, 0.400), (0.545, 0.400), (0.660, 0.060),
    ]
    m = Image.new("L", (size, size), 0)
    d = ImageDraw.Draw(m)
    d.polygon([(x * size, y * size) for x, y in pts], fill=255)
    return m


def main():
    os.makedirs(ASSETS, exist_ok=True)
    tile = base_tile()

    # white bolt with a soft colored glow behind it
    glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    gm = bolt_mask(S)
    tinted = Image.new("RGBA", (S, S), (90, 160, 255, 210))
    glow.paste(tinted, (0, 0), gm)
    glow = glow.filter(ImageFilter.GaussianBlur(26))
    img = tile.copy()
    img.alpha_composite(glow, (-10, 14))

    white = Image.new("RGBA", (S, S), (255, 255, 255, 255))
    img.paste(white, (0, 0), gm)

    # ---- write the master PNG + the web/window sizes
    for n in (256, 48, 32):
        img.resize((n, n), Image.LANCZOS).save(os.path.join(ASSETS, f"icon{n}.png"))
    # window icon stays crisp at 16 px too
    img.resize((16, 16), Image.LANCZOS).save(os.path.join(ASSETS, "icon16.png"))

    # ---- multi-resolution .ico for the exe resource
    ico_sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
    img.resize((256, 256), Image.LANCZOS).save(
        os.path.join(ASSETS, "optimizekit.ico"), sizes=ico_sizes
    )

    print("[icon] wrote assets/icon256.png icon48.png icon32.png icon16.png optimizekit.ico")


if __name__ == "__main__":
    main()
