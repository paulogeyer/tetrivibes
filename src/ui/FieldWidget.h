#pragma once

#include "game/Field.h"
#include "game/Piece.h"

#include <QWidget>

namespace tnet {

// Paints a 12x22 field. Compact mode is used for opponents and NEXT.
class FieldWidget : public QWidget {
    Q_OBJECT
public:
    explicit FieldWidget(QWidget *parent = nullptr);

    void setField(const Field &field);
    void setPiece(const Piece *piece);
    void setTitle(const QString &title);
    void setAlive(bool alive);
    void setHighlight(bool on);
    void setCompact(bool compact);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void drawCell(QPainter &p, const QRect &r, Cell cell) const;

    Field m_field;
    Piece m_piece;
    bool m_hasPiece = false;
    bool m_alive = true;
    bool m_highlight = false;
    bool m_compact = false;
    QString m_title;
};

} // namespace tnet
