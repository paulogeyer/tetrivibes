#pragma once

#include "Field.h"
#include "Types.h"

#include <QObject>
#include <QVector>
#include <random>

namespace tnet {

// Local playfield simulation: gravity, lock delay, inventory, and specials.
class Engine : public QObject {
    Q_OBJECT
public:
    explicit Engine(QObject *parent = nullptr);

    void reset(uint32_t seed);
    void tick(int ms);

    bool moveLeft();
    bool moveRight();
    bool rotate(int dir);
    bool softDrop();
    void hardDrop();

    void applySpecial(Special special);
    void setField(const Field &field);
    void setRemoteState(const Field &field, bool alive, int level, int score, int lines,
                        const QVector<Special> &inventory, bool hasPiece, const Piece &current,
                        const Piece &next);
    void setPieceDelay(int ms) { m_pieceDelay = ms; }
    void setStartLevel(int level) { m_startLevel = level < 1 ? 1 : level; }
    void setPaused(bool paused) { m_paused = paused; }
    void changeCurrentPiece();
    Special takeSpecial();
    void pushSpecials(const QVector<Special> &specials);

    Field snapshot(bool includePiece) const;
    const Field &field() const { return m_field; }
    const Piece &current() const { return m_current; }
    const Piece &next() const { return m_next; }
    const QVector<Special> &inventory() const { return m_inventory; }

    bool alive() const { return m_alive; }
    bool started() const { return m_started; }
    int lines() const { return m_lines; }
    int level() const { return m_level; }
    int score() const { return m_score; }
    bool hasPiece() const { return m_hasPiece; }
    bool paused() const { return m_paused; }

signals:
    void updated();
    void died();
    void inventoryChanged();

private:
    Piece randomPiece();
    void spawn();
    void lockCurrent();
    bool tryMove(int dx, int dy, int drot);
    int gravityMs() const;

    Field m_field;
    Piece m_current;
    Piece m_next;
    QVector<Special> m_inventory;
    std::mt19937 m_rng;
    bool m_alive = false;
    bool m_started = false;
    bool m_hasPiece = false;
    int m_lines = 0;
    int m_level = 1;
    int m_score = 0;
    int m_gravityAcc = 0;
    int m_spawnDelay = 0;
    int m_lockAcc = 0;
    int m_pieceDelay = 700;
    int m_startLevel = 1;
    bool m_paused = false;
};

} // namespace tnet
