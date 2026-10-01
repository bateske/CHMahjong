"""Generate src/assets/Assets.{h,cpp} from the art in tools/art/.

    python tools/assets.py

  * The tile faces: tools/art/tiles.txt, palette-letter text, 7 x 11 each, in
    the game's face order (dots, bamboo and characters 1-9, the winds, the
    dragons, four flowers, four seasons), then the back of a tile. A face
    may use two colours besides the tile's own.
  * The pointing hand: tools/art/hand.png (palette-exact, alpha 0 =
    transparent), else palette-letter text in tools/art/hand.txt.

A face becomes an 8 x 12 cell at 2 bits a pixel: the tile's left and top
edge, then the face. The stage draws it through four colours of its choice
(face, edge, the two inks), so one cell serves a free tile, a shaded one and
a flash.

Also writes previews to build/assets/.
"""
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
ART = HERE / "art"
OUT_H = ROOT / "src" / "assets" / "Assets.h"
OUT_C = ROOT / "src" / "assets" / "Assets.cpp"
PREVIEW = ROOT / "build" / "assets"

# Must match src/gfx/Palette.cpp.
PALETTE = [0x000, 0xFFF, 0x042, 0x173, 0x4B5, 0xBBC, 0xE12, 0x702,
           0xFC2, 0x741, 0x26E, 0x125, 0xFB8, 0x6EF, 0xF0F, 0xFC2]
# Letters used in tools/art/*.txt. ' ' / '.' = transparent.
LETTER = {"k": 0, "w": 1, "d": 2, "f": 3, "g": 4, "s": 5, "r": 6, "m": 7,
          "y": 8, "b": 9, "u": 10, "n": 11, "p": 12, "c": 13, "x": 14, "z": 15}
TRANSPARENT = 16
NAMES = ["INK", "WHITE", "FELT_DK", "FELT", "FELT_LT", "SILVER", "RED", "WINE",
         "GOLD", "WOOD", "BLUE", "NAVY", "SKIN", "CYAN", "FX_A", "FX_B"]
FACES = 42                      # then the back
FACE_W, FACE_H = 7, 11
CELL_W, CELL_H = 8, 12


def rgb(i):
    c = PALETTE[i]
    return ((c >> 8) * 17, ((c >> 4) & 15) * 17, (c & 15) * 17)


def load_png(path):
    """Palette-exact PNG -> rows of palette indices; alpha 0 is transparent."""
    lut = {rgb(i): i for i in range(15)}          # FX_B (15) shares GOLD's colour
    im = Image.open(path).convert("RGBA")
    rows = []
    for y in range(im.height):
        row = []
        for x in range(im.width):
            r, g, b, a = im.getpixel((x, y))
            if a == 0:
                row.append(TRANSPARENT)
            elif (r, g, b) in lut and a == 255:
                row.append(lut[(r, g, b)])
            else:
                raise SystemExit(f"{path.name} ({x},{y}): #{r:02X}{g:02X}{b:02X} alpha {a} is not a palette colour")
        rows.append(row)
    return rows


def load_art(name):
    """tools/art/<name>.txt: palette letters, one row per line; '#' starts a comment line.
    Several images may follow each other, separated by a blank line."""
    imgs, cur = [], []
    for ln in (ART / f"{name}.txt").read_text().splitlines():
        if ln.startswith("#"):
            continue
        if not ln.strip():
            if cur:
                imgs.append(cur)
                cur = []
            continue
        cur.append(ln.rstrip())
    if cur:
        imgs.append(cur)
    out = []
    for rows in imgs:
        w = max(len(r) for r in rows)
        out.append([[TRANSPARENT if ch in " ." else LETTER[ch] for ch in r.ljust(w)] for r in rows])
    return out


def load_hand():
    png = ART / "hand.png"
    return load_png(png) if png.exists() else load_art("hand")[0]


def load_tiles():
    """tools/art/tiles.txt -> a list of faces, each FACE_H rows of palette indices."""
    faces, block = [], []

    def flush():
        if not block:
            return
        if len(block) != FACE_H:
            raise SystemExit(f"tiles.txt: a row of faces has {len(block)} lines, not {FACE_H}")
        cols = [ln.split() for ln in block]
        n = len(cols[0])
        for k in range(n):
            face = []
            for r in range(FACE_H):
                if len(cols[r]) != n or len(cols[r][k]) != FACE_W:
                    raise SystemExit(f"tiles.txt: face {len(faces)} row {r} is not {FACE_W} wide")
                face.append([TRANSPARENT if ch == "." else LETTER[ch] for ch in cols[r][k]])
            faces.append(face)
        block.clear()

    for ln in (ART / "tiles.txt").read_text().splitlines():
        if ln.startswith("#"):
            continue
        if not ln.strip():
            flush()
            continue
        block.append(ln)
    flush()
    if len(faces) != FACES + 1:
        raise SystemExit(f"tiles.txt: {len(faces)} faces, expected {FACES} and the back")
    return faces


def pack_cell(face, k):
    """A face -> (24 bytes, ink byte). Pixel values: 0 face, 1 edge, 2 and 3 the inks;
    two bits a pixel, the left pixel in the low bits; two bytes a row."""
    inks = sorted({c for row in face for c in row if c != TRANSPARENT})
    if len(inks) > 2:
        raise SystemExit(f"tiles.txt: face {k} uses {len(inks)} colours ({', '.join(NAMES[c] for c in inks)}); two at most")
    for c in inks:
        if c in (2, 3, 14, 15):
            raise SystemExit(f"tiles.txt: face {k} uses {NAMES[c]}, which changes with the table or the animation")
    inks += [inks[0] if inks else 0] * (2 - len(inks))
    out = []
    for y in range(CELL_H):
        px = []
        for x in range(CELL_W):
            if x == 0 or y == 0:
                px.append(1)
            else:
                c = face[y - 1][x - 1]
                px.append(0 if c == TRANSPARENT else 2 + inks.index(c))
        for b in range(0, CELL_W, 4):
            out.append(px[b] | px[b + 1] << 2 | px[b + 2] << 4 | px[b + 3] << 6)
    return out, inks[0] | inks[1] << 4


def pack_span4(img, trans=TRANSPARENT):
    """Colour image -> w, h, then per row: n, then n bytes of (len-1)<<4 | colour
    (colour 15 = skip). Trailing transparency is implicit."""
    h, w = len(img), len(img[0])
    out = [w, h]
    for row in img:
        runs, x = [], 0
        while x < w:
            c = row[x]
            s = x
            while x < w and row[x] == c and x - s < 16:
                x += 1
            runs.append((x - s, c))
        while runs and runs[-1][1] == trans:
            runs.pop()
        out.append(len(runs))
        for n, c in runs:
            out.append(((n - 1) << 4) | (15 if c == trans else c))
    return out


def preview(name, img, scale=6, bg=3):
    h, w = len(img), len(img[0])
    im = Image.new("RGB", (w, h), rgb(bg))
    for y in range(h):
        for x in range(w):
            if img[y][x] != TRANSPARENT:
                im.putpixel((x, y), rgb(img[y][x]))
    im.resize((w * scale, h * scale), Image.NEAREST).save(PREVIEW / f"{name}.png")


def tile_sheet(faces, scale=8):
    """Every face as the game draws it: free (white) above, shaded (silver) below."""
    per = 9
    rows = (len(faces) + per - 1) // per
    w, h = per * 11 + 1, rows * 2 * 15 + 1
    img = [[3] * w for _ in range(h)]
    for k, face in enumerate(faces):
        for v, (bg, edge) in enumerate(((1, 9), (5, 9))):
            x0, y0 = 1 + (k % per) * 11, 1 + (k // per) * 30 + v * 15
            for y in range(CELL_H + 2):
                for x in range(CELL_W + 2):
                    if x >= 2 and y >= 2 and (x >= CELL_W or y >= CELL_H):
                        img[y0 + y][x0 + x] = 9                   # the tile's side
            for y in range(CELL_H):
                for x in range(CELL_W):
                    c = edge if x == 0 or y == 0 else face[y - 1][x - 1]
                    img[y0 + y][x0 + x] = bg if c == TRANSPARENT else c
    preview("tiles", img, scale)


def c_array(name, data, per_line=16):
    lines = [f"const uint8_t {name}[{len(data)}] = {{"]
    for i in range(0, len(data), per_line):
        lines.append("    " + ", ".join(str(v) for v in data[i:i + per_line]) + ",")
    lines.append("};")
    return "\n".join(lines)


def main():
    PREVIEW.mkdir(parents=True, exist_ok=True)
    decls, defs = [], []
    total = 0

    # Tile faces.
    faces = load_tiles()
    cells, inks = [], []
    for k, face in enumerate(faces):
        cell, ink = pack_cell(face, k)
        cells += cell
        inks.append(ink)
    lines = [f"const uint8_t TILE_CELL[{len(faces)}][{CELL_W * CELL_H // 4}] = {{"]
    for k in range(len(faces)):
        lines.append("    {" + ", ".join(str(v) for v in cells[k * 24:k * 24 + 24]) + "},")
    lines.append("};")
    defs.append("\n".join(lines))
    defs.append(c_array("TILE_INK", inks))
    decls.append(f"constexpr uint8_t TILE_BACK = {FACES};                         // the back of a tile, after the faces\n"
                 f"extern const uint8_t TILE_CELL[{len(faces)}][24];                    // 8x12, 2 bpp: 0 face, 1 edge, 2 and 3 the inks\n"
                 f"extern const uint8_t TILE_INK[{len(faces)}];                         // a face's inks: low nibble, high nibble")
    total += len(cells) + len(inks)
    tile_sheet(faces)

    # The pointing hand (span4), fingertip down and fingertip up.
    hand = load_hand()
    data = pack_span4(hand)
    defs.append(c_array("HAND", data))
    up = pack_span4(hand[::-1])
    defs.append(c_array("HAND_UP", up))
    tip = [x for x, v in enumerate(hand[-1]) if v != TRANSPARENT]
    decls.append("extern const uint8_t HAND[];                                 // span4, fingertip on the bottom row\n"
                 "extern const uint8_t HAND_UP[];                              // ... turned over: fingertip on the top row\n"
                 f"constexpr uint8_t HAND_TIP = {(tip[0] + tip[-1]) // 2};                           // its column")
    total += len(data) + len(up)
    preview("hand", hand, 8)

    OUT_H.parent.mkdir(parents=True, exist_ok=True)
    OUT_H.write_text("// Generated by tools/assets.py - do not edit.\n#pragma once\n#include <stdint.h>\n\n"
                     + "\n".join(decls) + "\n", newline="\n")
    OUT_C.write_text("// Generated by tools/assets.py - do not edit.\n"
                     "// Tile faces and the pointing hand, drawn in tools/art/.\n"
                     "#include \"Assets.h\"\n\n" + "\n\n".join(defs) + "\n", newline="\n")
    print(f"assets: {total} bytes of data")


if __name__ == "__main__":
    main()
