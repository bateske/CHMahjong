// A mahjong tile: an 8x12 face (its top and left edge, then 7x11 of art)
// standing on a body that shows as two bands, right of and below it - the
// tile's thickness, then its backing - as real tiles have.
//
// Faces are stored at 2 bits a pixel: the face, the emboss (the art's
// shadow, a pixel down and right of it, worked out by tools/assets.py) and
// two inks; the edge is drawn round them. They are drawn through a Style,
// so the same art is a free tile (embossed), a blocked one (flat and dim),
// a flash or a shimmer.
//
// At 1x a tile is drawn at an even x: a framebuffer byte is two pixels, so
// a row is a few byte stores. Scaled (the close-up view), w is the tile's
// width, 8..16 px; 16 is the fast doubled case.
#pragma once
#include <stdint.h>

namespace tile {

constexpr int W = 8, H = 12;        // the face; a tile next to it starts W right or H down
constexpr uint8_t NONE = 0xFF;

struct Style {
    uint8_t face, shade, edge;      // shade: the emboss (= face for none)
    uint8_t side, back;             // the bands; side NONE: no body (a tile held up)
};

// Rows drawn: [y0, y1). Also sets CHGfx's clip to them.
void setClip(int y0, int y1);

// cell: 24 bytes (tools/assets.py pack_cell); inks: low nibble, high nibble.
// No cell (nullptr): the body only - all that shows of a tile with another
// squarely on it.
void draw(const uint8_t *cell, uint8_t inks, int x, int y, const Style &s, int w = W);

}  // namespace tile
