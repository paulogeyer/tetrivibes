#include "FieldWidget.h"

#include <QPainter>
#include <algorithm>

namespace tnet {
namespace {

QColor cellColor(Cell c)
{
    switch (c) {
    case Cell::Empty: return QColor(18, 18, 24);
    case Cell::C1: return QColor(50, 120, 220);
    case Cell::C2: return QColor(230, 200, 40);
    case Cell::C3: return QColor(50, 190, 80);
    case Cell::C4: return QColor(170, 80, 210);
    case Cell::C5: return QColor(220, 60, 60);
    case Cell::AddLine: return QColor(30, 80, 180);
    case Cell::ClearLine: return QColor(220, 190, 30);
    case Cell::ClearSpecial: return QColor(200, 50, 50);
    case Cell::RandomClear: return QColor(40, 170, 70);
    case Cell::Bomb: return QColor(230, 230, 230);
    case Cell::Quake: return QColor(160, 90, 30);
    case Cell::Gravity: return QColor(20, 140, 90);
    case Cell::SwitchField: return QColor(150, 60, 190);
    case Cell::Nuke: return QColor(10, 90, 50);
    case Cell::LeftGravity: return QColor(80, 140, 220);
    case Cell::PieceChange: return QColor(230, 140, 40);
    case Cell::Zebra: return QColor(230, 230, 230);
    }
    return Qt::black;
}

} // namespace

FieldWidget::FieldWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void FieldWidget::setField(const Field &field)
{
    m_field = field;
    update();
}

void FieldWidget::setPiece(const Piece *piece)
{
    m_hasPiece = piece != nullptr;
    if (piece)
        m_piece = *piece;
    update();
}

void FieldWidget::setTitle(const QString &title)
{
    m_title = title;
    update();
}

void FieldWidget::setAlive(bool alive)
{
    m_alive = alive;
    update();
}

void FieldWidget::setHighlight(bool on)
{
    m_highlight = on;
    update();
}

void FieldWidget::setCompact(bool compact)
{
    m_compact = compact;
    update();
}

QSize FieldWidget::sizeHint() const
{
    return m_compact ? QSize(132, 260) : QSize(252, 480);
}

QSize FieldWidget::minimumSizeHint() const
{
    return m_compact ? QSize(96, 190) : QSize(180, 340);
}

void FieldWidget::drawCell(QPainter &p, const QRect &r, Cell cell) const
{
    if (!isFilled(cell)) {
        p.fillRect(r, QColor(16, 16, 22));
        p.setPen(QColor(28, 28, 36));
        p.drawRect(r.adjusted(0, 0, -1, -1));
        return;
    }

    const QColor base = cellColor(cell);
    p.fillRect(r, base);
    p.fillRect(QRect(r.left(), r.top(), r.width(), 2), base.lighter(140));
    p.fillRect(QRect(r.left(), r.top(), 2, r.height()), base.lighter(130));
    p.fillRect(QRect(r.left(), r.bottom() - 1, r.width(), 2), base.darker(150));
    p.fillRect(QRect(r.right() - 1, r.top(), 2, r.height()), base.darker(150));

    if (isSpecial(cell)) {
        p.setPen(QColor(0, 0, 0, 180));
        QFont f = p.font();
        f.setBold(true);
        f.setPixelSize(std::max(8, r.height() - 4));
        p.setFont(f);
        p.drawText(r.adjusted(1, 1, 1, 1), Qt::AlignCenter, QString(QChar(cellToChar(cell))));
        p.setPen(Qt::white);
        p.drawText(r, Qt::AlignCenter, QString(QChar(cellToChar(cell))));
    }
}

void FieldWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect(), QColor(8, 8, 12));

    const int header = m_compact ? 18 : 22;
    const int pad = 4;
    const int innerW = width() - pad * 2;
    const int innerH = height() - header - pad;
    const int cw = innerW / kFieldWidth;
    const int ch = innerH / kFieldHeight;
    const int cell = std::max(4, std::min(cw, ch));
    const int gridW = cell * kFieldWidth;
    const int gridH = cell * kFieldHeight;
    const int ox = (width() - gridW) / 2;
    const int oy = header + std::max(0, (innerH - gridH) / 2);

    p.setPen(m_highlight ? QColor(255, 200, 60) : QColor(70, 70, 90));
    p.drawRect(QRect(ox - 1, oy - 1, gridW + 1, gridH + 1));

    Field draw = m_field;
    if (m_hasPiece)
        draw.lock(m_piece);

    for (int y = 0; y < kFieldHeight; ++y) {
        for (int x = 0; x < kFieldWidth; ++x) {
            const QRect r(ox + x * cell, oy + y * cell, cell, cell);
            drawCell(p, r, draw.at(x, y));
        }
    }

    if (m_hasPiece && m_alive) {
        Piece ghost = m_piece;
        while (true) {
            Piece n = ghost;
            n.y += 1;
            if (!m_field.fits(n))
                break;
            ghost = n;
        }
        if (ghost.y != m_piece.y) {
            p.setPen(QColor(255, 255, 255, 70));
            for (int py = 0; py < kPieceSize; ++py) {
                for (int px = 0; px < kPieceSize; ++px) {
                    if (!ghost.occupied(px, py))
                        continue;
                    const int x = ghost.x + px;
                    const int y = ghost.y + py;
                    if (x < 0 || y < 0)
                        continue;
                    p.drawRect(QRect(ox + x * cell + 1, oy + y * cell + 1, cell - 3, cell - 3));
                }
            }
        }
    }

    QFont f = font();
    f.setBold(true);
    f.setPixelSize(m_compact ? 10 : 12);
    p.setFont(f);
    p.setPen(m_alive ? QColor(220, 220, 230) : QColor(180, 70, 70));
    QString title = m_title;
    if (!m_alive && !title.isEmpty())
        title += QStringLiteral("  OUT");
    p.drawText(QRect(ox, 2, gridW, header - 2), Qt::AlignLeft | Qt::AlignVCenter, title);

    if (!m_alive) {
        p.fillRect(QRect(ox, oy, gridW, gridH), QColor(0, 0, 0, 120));
    }
}

} // namespace tnet
