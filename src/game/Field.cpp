#include "Field.h"

#include <algorithm>
#include <utility>

namespace tnet {
namespace {

constexpr int kSpecialWeight[] = {
    18, // AddLine
    16, // ClearLine
    12, // ClearSpecial
    12, // RandomClear
    3,  // Bomb
    6,  // Quake
    5,  // Gravity
    2,  // SwitchField
    1   // Nuke
};

Special pickSpecial(std::mt19937 &rng)
{
    int total = 0;
    for (int w : kSpecialWeight)
        total += w;
    std::uniform_int_distribution<int> dist(0, total - 1);
    int roll = dist(rng);
    for (int i = 0; i < 9; ++i) {
        roll -= kSpecialWeight[i];
        if (roll < 0)
            return static_cast<Special>(i);
    }
    return Special::AddLine;
}

} // namespace

Field::Field()
{
    clear();
}

void Field::clear()
{
    for (auto &row : m_cells)
        row.fill(Cell::Empty);
}

Cell Field::at(int x, int y) const
{
    if (!inBounds(x, y))
        return Cell::Empty;
    return m_cells[static_cast<size_t>(y)][static_cast<size_t>(x)];
}

void Field::set(int x, int y, Cell c)
{
    if (inBounds(x, y))
        m_cells[static_cast<size_t>(y)][static_cast<size_t>(x)] = c;
}

bool Field::inBounds(int x, int y) const
{
    return x >= 0 && x < kFieldWidth && y >= 0 && y < kFieldHeight;
}

bool Field::fits(const Piece &piece) const
{
    for (int py = 0; py < kPieceSize; ++py) {
        for (int px = 0; px < kPieceSize; ++px) {
            if (!piece.occupied(px, py))
                continue;
            const int x = piece.x + px;
            const int y = piece.y + py;
            if (x < 0 || x >= kFieldWidth || y >= kFieldHeight)
                return false;
            if (y >= 0 && isFilled(at(x, y)))
                return false;
        }
    }
    return true;
}

void Field::lock(const Piece &piece)
{
    for (int py = 0; py < kPieceSize; ++py) {
        for (int px = 0; px < kPieceSize; ++px) {
            if (!piece.occupied(px, py))
                continue;
            const int x = piece.x + px;
            const int y = piece.y + py;
            if (inBounds(x, y))
                set(x, y, piece.color);
        }
    }
}

Field::ClearResult Field::clearLines()
{
    ClearResult result;
    int write = kFieldHeight - 1;
    for (int y = kFieldHeight - 1; y >= 0; --y) {
        bool full = true;
        for (int x = 0; x < kFieldWidth; ++x) {
            if (!isFilled(at(x, y))) {
                full = false;
                break;
            }
        }
        if (full) {
            ++result.lines;
            for (int x = 0; x < kFieldWidth; ++x) {
                const Cell c = at(x, y);
                if (isSpecial(c))
                    result.collected.push_back(cellToSpecial(c));
            }
            continue;
        }
        if (write != y) {
            for (int x = 0; x < kFieldWidth; ++x)
                set(x, write, at(x, y));
        }
        --write;
    }
    for (int y = write; y >= 0; --y) {
        for (int x = 0; x < kFieldWidth; ++x)
            set(x, y, Cell::Empty);
    }
    return result;
}

int Field::filledCount() const
{
    int n = 0;
    for (int y = 0; y < kFieldHeight; ++y)
        for (int x = 0; x < kFieldWidth; ++x)
            if (isFilled(at(x, y)))
                ++n;
    return n;
}

int Field::holeCount() const
{
    int holes = 0;
    for (int x = 0; x < kFieldWidth; ++x) {
        bool block = false;
        for (int y = 0; y < kFieldHeight; ++y) {
            if (isFilled(at(x, y)))
                block = true;
            else if (block)
                ++holes;
        }
    }
    return holes;
}

int Field::stackHeight() const
{
    for (int y = 0; y < kFieldHeight; ++y)
        for (int x = 0; x < kFieldWidth; ++x)
            if (isFilled(at(x, y)))
                return kFieldHeight - y;
    return 0;
}

bool Field::spawnBlocked(const Piece &piece) const
{
    return !fits(piece);
}

void Field::shiftUp()
{
    for (int y = 0; y < kFieldHeight - 1; ++y)
        for (int x = 0; x < kFieldWidth; ++x)
            set(x, y, at(x, y + 1));
}

void Field::shiftDown()
{
    for (int y = kFieldHeight - 1; y > 0; --y)
        for (int x = 0; x < kFieldWidth; ++x)
            set(x, y, at(x, y - 1));
    for (int x = 0; x < kFieldWidth; ++x)
        set(x, 0, Cell::Empty);
}

void Field::addLine(std::mt19937 &rng)
{
    shiftUp();
    std::uniform_int_distribution<int> hole(0, kFieldWidth - 1);
    std::uniform_int_distribution<int> color(1, 5);
    const int gap = hole(rng);
    for (int x = 0; x < kFieldWidth; ++x) {
        if (x == gap)
            set(x, kFieldHeight - 1, Cell::Empty);
        else
            set(x, kFieldHeight - 1, static_cast<Cell>(color(rng)));
    }
}

void Field::clearBottomLine()
{
    for (int x = 0; x < kFieldWidth; ++x)
        set(x, kFieldHeight - 1, Cell::Empty);
    shiftDown();
}

void Field::clearSpecials()
{
    std::uniform_int_distribution<int> color(1, 5);
    std::mt19937 rng{std::random_device{}()};
    for (int y = 0; y < kFieldHeight; ++y) {
        for (int x = 0; x < kFieldWidth; ++x) {
            if (isSpecial(at(x, y)))
                set(x, y, static_cast<Cell>(color(rng)));
        }
    }
}

void Field::randomClear(std::mt19937 &rng)
{
    QVector<std::pair<int, int>> blocks;
    for (int y = 0; y < kFieldHeight; ++y)
        for (int x = 0; x < kFieldWidth; ++x)
            if (isFilled(at(x, y)))
                blocks.push_back({x, y});
    if (blocks.isEmpty())
        return;
    std::shuffle(blocks.begin(), blocks.end(), rng);
    const int n = std::min(10, static_cast<int>(blocks.size()));
    for (int i = 0; i < n; ++i)
        set(blocks[i].first, blocks[i].second, Cell::Empty);
}

void Field::bomb(std::mt19937 &rng)
{
    QVector<std::pair<int, int>> bombs;
    for (int y = 0; y < kFieldHeight; ++y)
        for (int x = 0; x < kFieldWidth; ++x)
            if (at(x, y) == Cell::Bomb)
                bombs.push_back({x, y});

    QVector<Cell> scatter;
    QVector<std::pair<int, int>> clearAt;
    for (const auto &b : bombs) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int x = b.first + dx;
                const int y = b.second + dy;
                if (!inBounds(x, y) || !isFilled(at(x, y)))
                    continue;
                scatter.push_back(at(x, y));
                clearAt.push_back({x, y});
            }
        }
    }
    for (const auto &p : clearAt)
        set(p.first, p.second, Cell::Empty);

    QVector<std::pair<int, int>> empty;
    for (int y = 0; y < kFieldHeight; ++y)
        for (int x = 0; x < kFieldWidth; ++x)
            if (!isFilled(at(x, y)))
                empty.push_back({x, y});
    std::shuffle(empty.begin(), empty.end(), rng);
    const int n = std::min(scatter.size(), empty.size());
    for (int i = 0; i < n; ++i)
        set(empty[i].first, empty[i].second, scatter[i]);
}

void Field::quake(std::mt19937 &rng)
{
    std::uniform_int_distribution<int> dist(-3, 3);
    for (int y = 0; y < kFieldHeight; ++y) {
        int shift = dist(rng);
        if (shift == 0)
            continue;
        std::array<Cell, kFieldWidth> row{};
        for (int x = 0; x < kFieldWidth; ++x) {
            const int nx = x + shift;
            if (nx >= 0 && nx < kFieldWidth)
                row[static_cast<size_t>(nx)] = at(x, y);
        }
        for (int x = 0; x < kFieldWidth; ++x)
            set(x, y, row[static_cast<size_t>(x)]);
    }
}

void Field::gravity()
{
    for (int x = 0; x < kFieldWidth; ++x) {
        int write = kFieldHeight - 1;
        for (int y = kFieldHeight - 1; y >= 0; --y) {
            if (isFilled(at(x, y))) {
                const Cell c = at(x, y);
                set(x, y, Cell::Empty);
                set(x, write, c);
                --write;
            }
        }
    }
    clearLines();
}

void Field::nuke()
{
    clear();
}

void Field::leftGravity()
{
    for (int y = 0; y < kFieldHeight; ++y) {
        int write = 0;
        for (int x = 0; x < kFieldWidth; ++x) {
            if (isFilled(at(x, y))) {
                const Cell c = at(x, y);
                set(x, y, Cell::Empty);
                set(write, y, c);
                ++write;
            }
        }
    }
}

void Field::zebra()
{
    for (int y = 0; y < kFieldHeight; ++y)
        for (int x = 1; x < kFieldWidth; x += 2)
            set(x, y, Cell::Empty);
}

void Field::plantSpecials(int count, std::mt19937 &rng)
{
    QVector<std::pair<int, int>> blocks;
    for (int y = 0; y < kFieldHeight; ++y)
        for (int x = 0; x < kFieldWidth; ++x)
            if (isFilled(at(x, y)) && !isSpecial(at(x, y)))
                blocks.push_back({x, y});
    if (blocks.isEmpty() || count <= 0)
        return;
    std::shuffle(blocks.begin(), blocks.end(), rng);
    const int n = std::min(count, static_cast<int>(blocks.size()));
    for (int i = 0; i < n; ++i)
        set(blocks[i].first, blocks[i].second, specialToCell(pickSpecial(rng)));
}

QString Field::encode() const
{
    QString out;
    out.reserve(kFieldWidth * kFieldHeight);
    for (int y = 0; y < kFieldHeight; ++y)
        for (int x = 0; x < kFieldWidth; ++x)
            out.append(QChar(cellToChar(at(x, y))));
    return out;
}

Field Field::decode(const QString &data)
{
    Field f;
    const int n = std::min(static_cast<int>(data.size()), kFieldWidth * kFieldHeight);
    for (int i = 0; i < n; ++i) {
        const int y = i / kFieldWidth;
        const int x = i % kFieldWidth;
        f.set(x, y, charToCell(data[i].toLatin1()));
    }
    return f;
}

} // namespace tnet
