#pragma GCC optimize("Os")
#include <CHGfx.h>
#include "Tile.h"
#include "../RamFunc.h"     // the pile is up to 144 of these a frame: from SRAM

namespace tile {

RAMFUNC(tile) void draw(const uint8_t *cell, int x, int y, const uint8_t *lut, uint8_t side) {
    // Two source pixels (four bits) -> one framebuffer byte.
    uint8_t tab[16];
    bool bare = !cell;               // only the side
    if (!bare)
        for (uint8_t n = 0; n < 16; n++) tab[n] = (uint8_t)(lut[n & 3] | (lut[n >> 2] << 4));
    uint8_t ss = (uint8_t)(side * 0x11);
    int bx = x >> 1;
    for (int r = 0; r < H + SIDE; r++, cell += 2) {
        int yy = y + r;
        if (r >= H && side == NO_SIDE) break;
        if ((unsigned)yy >= GFX_H) continue;
        uint8_t row[5];
        int k0 = 0, k1 = 5;
        if (r < H && bare) {
            if (r < SIDE) continue;
            row[4] = ss;
            k0 = 4;
        } else if (r < H) {
            uint8_t a = cell[0], b = cell[1];
            row[0] = tab[a & 15]; row[1] = tab[a >> 4];
            row[2] = tab[b & 15]; row[3] = tab[b >> 4];
            row[4] = ss;
            if (r < SIDE || side == NO_SIDE) k1 = 4;
        } else {
            row[1] = row[2] = row[3] = row[4] = ss;
            k0 = 1;
        }
        if (bx + k0 < 0) k0 = -bx;
        if (bx + k1 > GFX_FB_STRIDE) k1 = GFX_FB_STRIDE - bx;
        uint8_t *p = gfx_fb + yy * GFX_FB_STRIDE + bx;
        for (int k = k0; k < k1; k++) p[k] = row[k];
    }
}

}  // namespace tile
