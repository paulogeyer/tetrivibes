#include "Piece.h"

namespace tnet {
namespace {

// 7 pieces × 4 rotations × 4 rows, each row a 4-bit nibble (bit 0 = left).
constexpr uint16_t kShapes[7][4] = {
    // I
    {0x0F00, 0x2222, 0x00F0, 0x4444},
    // O
    {0x0660, 0x0660, 0x0660, 0x0660},
    // T
    {0x0E40, 0x4C40, 0x4E00, 0x4640},
    // S
    {0x06C0, 0x8C40, 0x06C0, 0x8C40},
    // Z
    {0x0C60, 0x4C80, 0x0C60, 0x4C80},
    // J
    {0x08E0, 0x6440, 0x0E20, 0x44C0},
    // L
    {0x02E0, 0x4460, 0x0E80, 0xC440},
};

} // namespace

bool Piece::occupied(int px, int py) const
{
    if (px < 0 || px >= kPieceSize || py < 0 || py >= kPieceSize)
        return false;
    const int kindIndex = static_cast<int>(kind);
    const uint16_t bits = kShapes[kindIndex][rotation & 3];
    const int bit = py * 4 + px;
    return (bits & (0x8000 >> bit)) != 0;
}

Piece Piece::spawn(PieceKind kind, Cell color)
{
    Piece p;
    p.kind = kind;
    p.rotation = 0;
    p.x = 4;
    p.y = 0;
    p.color = color;
    return p;
}

PieceKind kindFromIndex(int index)
{
    const int n = static_cast<int>(PieceKind::Count);
    int v = index % n;
    if (v < 0)
        v += n;
    return static_cast<PieceKind>(v);
}

} // namespace tnet
