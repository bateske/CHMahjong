// A mahjong tile: an 8x12 cell (its left and top edge, then the 7x11 face)
// stored at 2 bits a pixel and drawn through four colours - face, edge and
// two inks - so the same art is a free tile, a shaded one, or a flash. The
// tile's side shows as a band 2 px wide, right of and below the cell.
//
// Cells are drawn at even x only: a framebuffer byte is two pixels, so a
// row of a cell is four byte stores (and one more for the side).
#pragma once
#include <stdint.h>

namespace tile {

constexpr int W = 8, H = 12;        // the cell; a tile next to it starts W right or H down
constexpr int SIDE = 2;
constexpr uint8_t NO_SIDE = 0xFF;

// cell: 24 bytes (tools/assets.py pack_cell). lut: face, edge, ink, ink.
// x is taken as even. Clipped to the screen.
void draw(const uint8_t *cell, int x, int y, const uint8_t *lut, uint8_t side);

}  // namespace tile
