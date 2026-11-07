#!/usr/bin/env python3
"""Draw the Mosaik brand assets after the Shei Designs product rules
(sheidesigns.com design-reference/PRODUCTS.md).

    python3 docs/brand/draw.py

Writes, relative to the repo root:
    docs/brand/icon.svg            product tile (100-unit box), as used on sheidesigns.com
    docs/brand/icon-512.png        the same, rasterized
    docs/banner.svg                README banner, 1280 x 360
    docs/brand/social-preview.svg  GitHub social preview, 1280 x 640 (+ .png)
    components/webctl/web/icon.svg, icon-180.png
                                   the tile served by the panel's web UI
                                   (favicon, header, home-screen icon)

Text is converted to outlines, so the SVGs render the same everywhere. The
fonts are the brand's (Inter, Space Grotesk, JetBrains Mono, all SIL OFL),
fetched once from the pinned @fontsource packages into docs/brand/.fonts/.
Needs fontTools (`pip install fonttools`); PNGs need `rsvg-convert`.
"""
import shutil
import subprocess
import urllib.request
from pathlib import Path

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent

# House tokens (DESIGN.md) and Mosaik's register entry (PRODUCTS.md §3):
# hue 285 between TrioX violet and MonoCycle pink, S 100 %, L 54 %.
CARBON, TAPE, LINEN, DUST = "#0A0907", "#FF6B1A", "#E8E4D8", "#8A8580"
PURPLE = "#C414FF"
SHADE = "#390C45"  # 0.75 x Carbon + 0.25 x tile
TILE = "M26 6 H74 A20 20 0 0 1 94 26 V74 A20 20 0 0 1 74 94 H26 A20 20 0 0 1 6 74 V26 A20 20 0 0 1 26 6 Z"

NAME = "Mosaik"
ONE_LINER = "A 32 × 32 LED pixel panel you run from your phone."
META = "esp32-s3 · 1024 ws2812b      mit      wifi · ota · 3d-printed"
TITLE = "Mosaik: a 32 × 32 LED pixel panel you run from your phone. A Shei Designs project."

FONTSOURCE = "https://cdn.jsdelivr.net/npm/@fontsource/{pkg}@5.3.0/files/{file}"
FONTS = {
    "inter800": ("inter", "inter-latin-800-normal.woff"),
    "inter400": ("inter", "inter-latin-400-normal.woff"),
    "grotesk": ("space-grotesk", "space-grotesk-latin-500-normal.woff"),
    "mono": ("jetbrains-mono", "jetbrains-mono-latin-400-normal.woff"),
}


def font(key):
    pkg, file = FONTS[key]
    path = HERE / ".fonts" / file
    if not path.exists():
        path.parent.mkdir(exist_ok=True)
        with urllib.request.urlopen(FONTSOURCE.format(pkg=pkg, file=file)) as r:
            path.write_bytes(r.read())
    return TTFont(path)


def n(v):
    return f"{round(v, 2):g}"


# --- text as outlines -------------------------------------------------------------------------

def pair_kerning(f):
    """Pair adjustments (x-advance of the first glyph) from GPOS 'kern', in font units."""
    pairs, classes = {}, []
    if "GPOS" not in f:
        return pairs, classes
    gpos = f["GPOS"].table
    idx = {i for fr in gpos.FeatureList.FeatureRecord if fr.FeatureTag == "kern"
           for i in fr.Feature.LookupListIndex}
    for i in sorted(idx):
        lookup = gpos.LookupList.Lookup[i]
        for st in lookup.SubTable:
            if lookup.LookupType == 9:  # extension: unwrap to the real subtable
                if st.ExtensionLookupType != 2:
                    continue
                st = st.ExtSubTable
            elif lookup.LookupType != 2:  # only pair adjustment carries kerning
                continue
            cov = st.Coverage.glyphs
            if st.Format == 1:
                for g1, ps in zip(cov, st.PairSet):
                    for rec in ps.PairValueRecord:
                        v = getattr(rec.Value1, "XAdvance", 0) if rec.Value1 else 0
                        if v:
                            pairs.setdefault((g1, rec.SecondGlyph), v)
            elif st.Format == 2:
                classes.append((set(cov), st.ClassDef1.classDefs, st.ClassDef2.classDefs, st.Class1Record))
    return pairs, classes


class Face:
    def __init__(self, key):
        self.f = font(key)
        self.upem = self.f["head"].unitsPerEm
        self.cmap = self.f.getBestCmap()
        self.glyphs = self.f.getGlyphSet()
        self.hmtx = self.f["hmtx"]
        self.pairs, self.classes = pair_kerning(self.f)

    def kern(self, a, b):
        if (a, b) in self.pairs:
            return self.pairs[(a, b)]
        for cov, c1, c2, recs in self.classes:
            if a in cov:
                rec = recs[c1.get(a, 0)].Class2Record[c2.get(b, 0)]
                v = getattr(rec.Value1, "XAdvance", 0) if rec.Value1 else 0
                if v:
                    return v
                break
        return 0

    def text(self, s, x, y, size, tracking=0.0):
        """Returns (svg path data, advance width) for `s` with its baseline at y."""
        k = size / self.upem
        pen = SVGPathPen(self.glyphs, ntos=n)
        cx, prev = x, None
        for ch in s:
            g = self.cmap[ord(ch)]
            if prev:
                cx += self.kern(prev, g) * k
            self.glyphs[g].draw(TransformPen(pen, (k, 0, 0, -k, cx, y)))
            cx += self.hmtx[g][0] * k + tracking * size
            prev = g
        return pen.getCommands(), cx - x


# --- the mark: the panel face, one pixel lit ----------------------------------------------------
# A Carbon slab filling the 64-unit safe square (18 to 82), a 3 x 3 grid of
# pixels in the shade (the one plane the mark may add), and the one cutout:
# the lit pixel, top right, through which the tile colour shows. A 4 x 4 grid
# reads better as a matrix but its lit pixel closes up at 16 px.

def rrect(x, y, w, h, r):
    return (f"M{n(x + r)} {n(y)}H{n(x + w - r)}A{n(r)} {n(r)} 0 0 1 {n(x + w)} {n(y + r)}"
            f"V{n(y + h - r)}A{n(r)} {n(r)} 0 0 1 {n(x + w - r)} {n(y + h)}"
            f"H{n(x + r)}A{n(r)} {n(r)} 0 0 1 {n(x)} {n(y + h - r)}"
            f"V{n(y + r)}A{n(r)} {n(r)} 0 0 1 {n(x + r)} {n(y)}Z")


def mark():
    x0, edge, pad, gap, grid, lit = 18, 64, 7, 5, 3, (0, 2)
    cell = (edge - 2 * pad - (grid - 1) * gap) / grid
    pos = lambda i: x0 + pad + i * (cell + gap)
    cells, hole = [], ""
    for r in range(grid):
        for c in range(grid):
            p = rrect(pos(c), pos(r), cell, cell, 2)
            if (r, c) == lit:
                hole = p
            else:
                cells.append(p)
    return (f'<path d="{rrect(x0, x0, edge, edge, 4)}{hole}" fill="{CARBON}" fill-rule="evenodd"/>'
            f'<path d="{"".join(cells)}" fill="{SHADE}"/>')


def tile_group(x, y, size):
    return f'<g transform="translate({n(x)} {n(y)}) scale({n(size / 100)})"><path d="{TILE}" fill="{PURPLE}"/>{mark()}</g>'


def signature(face, right, baseline):
    """'Shei Designs' in Inter 800 ending `right`, with the Tape dot after it."""
    _, w = face.text("Shei Designs", 0, 0, 22, tracking=-0.02)
    x = right - w - 14
    d, _ = face.text("Shei Designs", x, baseline, 22, tracking=-0.02)
    return f'<path d="{d}" fill="{LINEN}"/><circle cx="{n(x + w + 8)}" cy="{n(baseline - 4)}" r="4" fill="{TAPE}"/>'


def main():
    inter800, inter400, grotesk, mono = (Face(k) for k in FONTS)

    icon = f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><path d="{TILE}" fill="{PURPLE}"/>{mark()}</svg>\n'
    (HERE / "icon.svg").write_text(icon)
    web = ROOT / "components" / "webctl" / "web"
    (web / "icon.svg").write_text(icon)

    # README banner (PRODUCTS.md §2): 1280 x 360, Carbon, radius 28, tile at 176 px.
    name, _ = grotesk.text(NAME, 328, 172, 68)
    line, _ = inter400.text(ONE_LINER, 330, 226, 30)
    meta, _ = mono.text(META, 332, 274, 17)
    (ROOT / "docs" / "banner.svg").write_text(f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1280 360" role="img" aria-labelledby="t">
  <title id="t">{TITLE}</title>
  <rect width="1280" height="360" rx="28" fill="{CARBON}"/>
  {tile_group(80, 80, 200)}
  <path d="{name}" fill="{LINEN}"/>
  <path d="{line}" fill="{LINEN}" opacity="0.86"/>
  <path d="{meta}" fill="{DUST}"/>
  {signature(inter800, 1184, 318)}
</svg>
""")

    # GitHub social preview (DESIGN.md §8): 1280 x 640, solid Carbon, lockup centred.
    t, gap = 200, 56
    _, wn = grotesk.text(NAME, 0, 0, 84)
    _, wl = inter400.text(ONE_LINER, 0, 0, 32)
    _, wm = mono.text(META, 0, 0, 18)
    width = t + gap + max(wn, wl, wm)
    x, y = (1280 - width) / 2, (640 - t) / 2
    tx = x + t + gap
    (HERE / "social-preview.svg").write_text(f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1280 640" role="img" aria-labelledby="t">
  <title id="t">{TITLE}</title>
  <rect width="1280" height="640" fill="{CARBON}"/>
  {tile_group(x - t / 0.88 * 0.06, y - t / 0.88 * 0.06, t / 0.88)}
  <path d="{grotesk.text(NAME, tx, y + 92, 84)[0]}" fill="{LINEN}"/>
  <path d="{inter400.text(ONE_LINER, tx + 2, y + 146, 32)[0]}" fill="{LINEN}" opacity="0.86"/>
  <path d="{mono.text(META, tx + 4, y + 194, 18)[0]}" fill="{DUST}"/>
  {signature(inter800, 1280 - 48, 640 - 48)}
</svg>
""")

    if shutil.which("rsvg-convert"):
        for src, dst, w, h in [("icon.svg", "icon-512.png", 512, 512),
                               ("social-preview.svg", "social-preview.png", 1280, 640)]:
            subprocess.run(["rsvg-convert", "-w", str(w), "-h", str(h), "-o", HERE / dst, HERE / src], check=True)
        subprocess.run(["rsvg-convert", "-w", "180", "-h", "180", "-o", web / "icon-180.png", web / "icon.svg"], check=True)
    else:
        print("rsvg-convert not found: PNGs not drawn")


if __name__ == "__main__":
    main()
