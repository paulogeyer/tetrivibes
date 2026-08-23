#include "Piece.h"

#include <QStringList>

namespace tnet {
namespace {

bool parseInt(const QString &text, int &value)
{
    bool ok = false;
    const int parsed = text.toInt(&ok);
    if (!ok)
        return false;
    value = parsed;
    return true;
}

} // namespace
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

QString encodeLivePieces(bool hasPiece, const Piece &current, const Piece &next)
{
    const QString nextPart = QStringLiteral("%1,%2")
                                 .arg(static_cast<int>(next.kind))
                                 .arg(static_cast<int>(next.color));
    if (!hasPiece)
        return QStringLiteral("-:%1").arg(nextPart);
    return QStringLiteral("%1,%2,%3,%4,%5:%6")
        .arg(static_cast<int>(current.kind))
        .arg(current.rotation)
        .arg(current.x)
        .arg(current.y)
        .arg(static_cast<int>(current.color))
        .arg(nextPart);
}

bool decodeLivePieces(const QString &text, bool &hasPiece, Piece &current, Piece &next)
{
    const QStringList halves = text.split(QLatin1Char(':'));
    if (halves.size() != 2)
        return false;
    const QStringList nextParts = halves[1].split(QLatin1Char(','));
    if (nextParts.size() != 2)
        return false;
    int nextKind = 0;
    int nextColor = 0;
    if (!parseInt(nextParts[0], nextKind) || !parseInt(nextParts[1], nextColor))
        return false;
    if (nextKind < 0 || nextKind >= static_cast<int>(PieceKind::Count) || nextColor < 1
        || nextColor > 5)
        return false;
    next = Piece::spawn(kindFromIndex(nextKind), static_cast<Cell>(nextColor));
    if (halves[0] == QLatin1String("-")) {
        hasPiece = false;
        return true;
    }
    const QStringList p = halves[0].split(QLatin1Char(','));
    if (p.size() != 5)
        return false;
    int kind = 0;
    int rot = 0;
    int x = 0;
    int y = 0;
    int color = 0;
    if (!parseInt(p[0], kind) || !parseInt(p[1], rot) || !parseInt(p[2], x) || !parseInt(p[3], y)
        || !parseInt(p[4], color))
        return false;
    if (kind < 0 || kind >= static_cast<int>(PieceKind::Count) || color < 1 || color > 5)
        return false;
    current = Piece::spawn(kindFromIndex(kind), static_cast<Cell>(color));
    current.rotation = rot & 3;
    current.x = x;
    current.y = y;
    hasPiece = true;
    return true;
}

} // namespace tnet
