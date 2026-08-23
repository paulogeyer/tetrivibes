#pragma once

#include "Types.h"

namespace tnet {

// 4x4 tetromino. occupied(px,py) is local to the piece, not the field.
struct Piece {
    PieceKind kind = PieceKind::I;
    int rotation = 0;
    int x = 4;
    int y = 0;
    Cell color = Cell::C1;

    bool occupied(int px, int py) const;
    static Piece spawn(PieceKind kind, Cell color);
};

PieceKind kindFromIndex(int index);

} // namespace tnet
