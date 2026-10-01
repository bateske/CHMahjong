#pragma GCC optimize("Os")
#include <CHGfx.h>
#include "Tile.h"
#include "../RamFunc.h"     // the pile is up to 144 of these a frame: from SRAM

namespace tile {

static int clipY0 = 0, clipY1 = GFX_H;

void setClip(int y0, int y1) {
    clipY0 = y0;
    clipY1 = y1;
    gfx_setClip(0, y0, GFX_W, y1 - y0);
}

// Byte k of a row (bx + k) gets v, in the nibbles m says.
static inline __attribute__((always_inline)) void put(uint8_t *p, int bx, int k, uint8_t v, uint8_t m) {
    if (!m || (unsigned)(bx + k) >= (unsigned)GFX_FB_STRIDE) return;
    p[k] = m == 0xFF ? v : (uint8_t)((p[k] & ~m) | (v & m));
}

// 1x at an even x. The body is two copies of the tile's outline, moved
// (2, 2) in the backing colour and (1, 1) in the side colour, under the face:
//
//   row  0   E E E E E E E E . .
//   row  1   E f f f f f f f S .
//   ...      E f f f f f f f S B
//   row 12   . S S S S S S S S B
//   row 13   . . B B B B B B B B
RAMFUNC(tile1) static void draw1(const uint8_t *cell, int x, int y, const uint8_t *lut, const Style &s) {
    uint8_t tab[16];
    if (cell)
        for (uint8_t n = 0; n < 16; n++) tab[n] = (uint8_t)(lut[n & 3] | (lut[n >> 2] << 4));
    bool body = s.side != NONE;
    uint8_t E = (uint8_t)(s.edge * 0x11), S = (uint8_t)(s.side * 0x11), B = (uint8_t)(s.back * 0x11);
    uint8_t SB = (uint8_t)((S & 0x0F) | (B & 0xF0));
    int bx = x >> 1;
    for (int r = 0; r < H + 2; r++) {
        int yy = y + r;
        if (yy < clipY0 || yy >= clipY1) continue;
        if (r >= H && !body) break;
        uint8_t *p = gfx_fb + yy * GFX_FB_STRIDE + bx;
        if (r < H) {
            if (cell) {
                if (!r) {
                    for (int k = 0; k < 4; k++) put(p, bx, k, E, 0xFF);
                } else {
                    uint8_t a = cell[2 * r], b = cell[2 * r + 1];
                    put(p, bx, 0, (uint8_t)((tab[a & 15] & 0xF0) | (E & 0x0F)), 0xFF);
                    put(p, bx, 1, tab[a >> 4], 0xFF);
                    put(p, bx, 2, tab[b & 15], 0xFF);
                    put(p, bx, 3, tab[b >> 4], 0xFF);
                }
            }
            if (body && r) put(p, bx, 4, SB, r == 1 ? 0x0F : 0xFF);
        } else if (r == H) {
            put(p, bx, 0, S, 0xF0);
            for (int k = 1; k < 4; k++) put(p, bx, k, S, 0xFF);
            put(p, bx, 4, SB, 0xFF);
        } else {
            for (int k = 1; k < 5; k++) put(p, bx, k, B, 0xFF);
        }
    }
}

// Doubled (w 16) at an even x: a source pixel is a framebuffer byte, a
// source row two framebuffer rows, and the body's bands are 2 px: bytes 8
// (side) and 9 (backing) right of the face, rows 24-27 below it.
RAMFUNC(tile2) static void draw2(const uint8_t *cell, int x, int y, const uint8_t *lut, const Style &s) {
    uint8_t pair[4];
    for (int k = 0; k < 4; k++) pair[k] = (uint8_t)(lut[k] * 0x11);
    uint8_t E = (uint8_t)(lut[4] * 0x11), S = (uint8_t)(s.side * 0x11), B = (uint8_t)(s.back * 0x11);
    bool body = s.side != NONE;
    int bx = x >> 1;
    uint8_t row[10];
    int loaded = -1;
    for (int r = 0; r < 2 * H + 4; r++) {
        int yy = y + r;
        if (r >= 2 * H && !body) break;
        if (yy < clipY0 || yy >= clipY1) continue;
        if (r < 2 * H && cell && (r >> 1) != loaded) {
            // The source row, once for its two screen rows (or for the one
            // the clip leaves).
            int sr = loaded = r >> 1;
            uint16_t bits = (uint16_t)(cell[2 * sr] | (cell[2 * sr + 1] << 8));
            for (int k = 0; k < 8; k++, bits >>= 2) row[k] = (sr && k) ? pair[bits & 3] : E;
        }
        uint8_t *p = gfx_fb + yy * GFX_FB_STRIDE + bx;
        int k0 = 0, k1 = 8;
        if (r < 2 * H) {
            if (!cell) k0 = 8;
            if (body && r >= 2) { row[8] = S; row[9] = B; k1 = r >= 4 ? 10 : 9; }
        } else if (r < 2 * H + 2) {
            for (int k = 1; k < 9; k++) row[k] = S;
            row[9] = B;
            k0 = 1; k1 = 10;
        } else {
            for (int k = 2; k < 10; k++) row[k] = B;
            k0 = 2; k1 = 10;
        }
        for (int k = k0; k < k1; k++) put(p, bx, k, row[k], 0xFF);
    }
}

// Any size, any x (the frames of a zoom): a pixel at a time, the body's
// bands included.
RAMFUNC(tilen) static void drawN(const uint8_t *cell, int x, int y, const uint8_t *lut, const Style &s, int w) {
    int h = w * 3 / 2, t = w >= 16 ? 2 : 1;
    bool body = s.side != NONE;
    int cols = body ? w + 2 * t : w, rows = body ? h + 2 * t : h;
    uint8_t sc[2 * W];
    for (int i = 0; i < w; i++) sc[i] = (uint8_t)(i * W / w);
    for (int j = 0; j < rows; j++) {
        int yy = y + j;
        if (yy < clipY0 || yy >= clipY1) continue;
        int sr = j < h ? j * W / w : 0;
        uint16_t bits = cell && j < h ? (uint16_t)(cell[2 * sr] | (cell[2 * sr + 1] << 8)) : 0;
        uint8_t *row = gfx_fb + yy * GFX_FB_STRIDE;
        for (int i = 0; i < cols; i++) {
            int xx = x + i;
            if ((unsigned)xx >= (unsigned)GFX_W) continue;
            uint8_t c;
            if (i < w && j < h) {
                if (!cell) continue;
                c = (!sr || !sc[i]) ? lut[4] : lut[(bits >> (2 * sc[i])) & 3];
            } else if (i >= t && i < w + t && j >= t && j < h + t) c = s.side;
            else if (i >= 2 * t && j >= 2 * t) c = s.back;
            else continue;
            uint8_t &q = row[xx >> 1];
            q = (xx & 1) ? (uint8_t)((q & 0x0F) | (c << 4)) : (uint8_t)((q & 0xF0) | c);
        }
    }
}

void draw(const uint8_t *cell, uint8_t inks, int x, int y, const Style &s, int w) {
    uint8_t lut[5] = {s.face, s.shade, (uint8_t)(inks & 15), (uint8_t)(inks >> 4), s.edge};
    if (!(x & 1) && w == W) draw1(cell, x, y, lut, s);
    else if (!(x & 1) && w == 2 * W) draw2(cell, x, y, lut, s);
    else drawN(cell, x, y, lut, s, w);
}

}  // namespace tile
