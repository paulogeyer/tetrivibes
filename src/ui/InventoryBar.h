#pragma once

#include "game/Types.h"

#include <QVector>
#include <QWidget>

namespace tnet {

// Queue of collected specials; the leftmost is fired with keys 1–6.
class InventoryBar : public QWidget {
    Q_OBJECT
public:
    explicit InventoryBar(QWidget *parent = nullptr);
    void setInventory(const QVector<Special> &inv);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<Special> m_inv;
};

} // namespace tnet
