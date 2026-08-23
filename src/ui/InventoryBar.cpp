#include "InventoryBar.h"

#include <QPainter>

namespace tnet {
namespace {

QColor specialColor(Special s)
{
    switch (s) {
    case Special::AddLine: return QColor(40, 90, 200);
    case Special::ClearLine: return QColor(220, 190, 30);
    case Special::ClearSpecial: return QColor(200, 50, 50);
    case Special::RandomClear: return QColor(40, 170, 70);
    case Special::Bomb: return QColor(220, 220, 220);
    case Special::Quake: return QColor(170, 100, 40);
    case Special::Gravity: return QColor(20, 150, 100);
    case Special::SwitchField: return QColor(160, 70, 200);
    case Special::Nuke: return QColor(20, 100, 60);
    case Special::LeftGravity: return QColor(80, 140, 220);
    case Special::PieceChange: return QColor(230, 140, 40);
    case Special::Zebra: return QColor(240, 240, 240);
    }
    return Qt::gray;
}

} // namespace

InventoryBar::InventoryBar(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(36);
}

void InventoryBar::setInventory(const QVector<Special> &inv)
{
    m_inv = inv;
    update();
}

QSize InventoryBar::sizeHint() const
{
    return QSize(400, 40);
}

void InventoryBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(14, 14, 20));
    p.setPen(QColor(60, 60, 80));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    const int cell = 28;
    const int y = (height() - cell) / 2;
    p.setPen(QColor(180, 180, 190));
    p.drawText(QRect(8, 0, 70, height()), Qt::AlignVCenter, QStringLiteral("SPECIALS"));

    int x = 82;
    for (int i = 0; i < kMaxInventory; ++i) {
        const QRect r(x, y, cell, cell);
        if (i < m_inv.size()) {
            const Special s = m_inv[i];
            p.fillRect(r, specialColor(s));
            p.setPen(i == 0 ? QColor(255, 220, 80) : QColor(0, 0, 0, 160));
            p.drawRect(r.adjusted(0, 0, -1, -1));
            QFont f = p.font();
            f.setBold(true);
            f.setPixelSize(16);
            p.setFont(f);
            p.setPen(Qt::white);
            p.drawText(r, Qt::AlignCenter, QString(QChar(specialLetter(s))));
        } else {
            p.fillRect(r, QColor(22, 22, 30));
            p.setPen(QColor(40, 40, 50));
            p.drawRect(r.adjusted(0, 0, -1, -1));
        }
        x += cell + 3;
    }
}

} // namespace tnet
