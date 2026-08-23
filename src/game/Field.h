#pragma once

#include "Piece.h"
#include "Types.h"

#include <QString>
#include <QVector>
#include <random>

namespace tnet {

// 12x22 grid. encode()/decode() use the classic 264-char field string.
class Field {
public:
    Field();

    void clear();
    Cell at(int x, int y) const;
    void set(int x, int y, Cell c);

    bool fits(const Piece &piece) const;
    void lock(const Piece &piece);

    struct ClearResult {
        int lines = 0;
        QVector<Special> collected;
    };
    ClearResult clearLines();

    int filledCount() const;
    int holeCount() const;
    int stackHeight() const;

    void addLine(std::mt19937 &rng);
    void clearBottomLine();
    void clearSpecials(std::mt19937 &rng);
    void randomClear(std::mt19937 &rng);
    void bomb(std::mt19937 &rng);
    void quake(std::mt19937 &rng);
    void gravity();
    void nuke();
    void leftGravity();
    void zebra();
    void plantSpecials(int count, std::mt19937 &rng);

    QString encode() const;
    static Field decode(const QString &data);
    static bool isValidEncoding(const QString &data);

    const std::array<std::array<Cell, kFieldWidth>, kFieldHeight> &cells() const { return m_cells; }

private:
    bool inBounds(int x, int y) const;
    void shiftUp();
    void shiftDown();

    std::array<std::array<Cell, kFieldWidth>, kFieldHeight> m_cells{};
};

} // namespace tnet
