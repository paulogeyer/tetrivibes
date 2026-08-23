#include "Engine.h"

#include <algorithm>

namespace tnet {

Engine::Engine(QObject *parent)
    : QObject(parent)
{
}

void Engine::reset(uint32_t seed)
{
    m_rng.seed(seed);
    m_field.clear();
    m_inventory.clear();
    m_alive = true;
    m_started = true;
    m_hasPiece = false;
    m_lines = 0;
    m_level = m_startLevel;
    m_paused = false;
    m_score = 0;
    m_gravityAcc = 0;
    m_lockAcc = 0;
    m_spawnDelay = 200;
    m_next = randomPiece();
    emit inventoryChanged();
    emit updated();
}

Piece Engine::randomPiece()
{
    std::uniform_int_distribution<int> kind(0, static_cast<int>(PieceKind::Count) - 1);
    std::uniform_int_distribution<int> color(1, 5);
    return Piece::spawn(kindFromIndex(kind(m_rng)), static_cast<Cell>(color(m_rng)));
}

void Engine::spawn()
{
    m_current = m_next;
    m_next = randomPiece();
    m_current.x = 4;
    m_current.y = 0;
    m_hasPiece = true;
    m_gravityAcc = 0;
    m_lockAcc = 0;
    if (!m_field.fits(m_current)) {
        m_alive = false;
        m_hasPiece = false;
        emit died();
    }
    emit updated();
}

int Engine::gravityMs() const
{
    const int table[] = {800, 720, 640, 560, 480, 400, 330, 270, 220, 180, 150, 120, 100, 80, 60};
    const int idx = std::min(m_level - 1, 14);
    return table[idx];
}

void Engine::tick(int ms)
{
    if (!m_alive || !m_started || m_paused)
        return;

    if (!m_hasPiece) {
        m_spawnDelay -= ms;
        if (m_spawnDelay <= 0)
            spawn();
        return;
    }

    Piece test = m_current;
    test.y += 1;
    const bool grounded = !m_field.fits(test);

    if (grounded) {
        m_lockAcc += ms;
        if (m_lockAcc >= 350)
            lockCurrent();
        return;
    }

    m_lockAcc = 0;
    m_gravityAcc += ms;
    while (m_hasPiece && m_gravityAcc >= gravityMs()) {
        m_gravityAcc -= gravityMs();
        if (!tryMove(0, 1, 0)) {
            lockCurrent();
            break;
        }
    }
}

bool Engine::tryMove(int dx, int dy, int drot)
{
    if (!m_hasPiece || !m_alive)
        return false;
    Piece next = m_current;
    next.x += dx;
    next.y += dy;
    next.rotation = (next.rotation + drot) & 3;
    if (!m_field.fits(next))
        return false;
    m_current = next;
    if (dy != 0)
        m_lockAcc = 0;
    emit updated();
    return true;
}

bool Engine::moveLeft()
{
    return tryMove(-1, 0, 0);
}

bool Engine::moveRight()
{
    return tryMove(1, 0, 0);
}

bool Engine::rotate(int dir)
{
    // Simple wall kicks: rotate in place, then try one cell left/right/up.
    if (tryMove(0, 0, dir))
        return true;
    if (tryMove(-1, 0, dir))
        return true;
    if (tryMove(1, 0, dir))
        return true;
    return tryMove(0, -1, dir);
}

bool Engine::softDrop()
{
    if (tryMove(0, 1, 0)) {
        m_score += 1;
        return true;
    }
    lockCurrent();
    return false;
}

void Engine::hardDrop()
{
    if (!m_hasPiece || !m_alive)
        return;
    int dropped = 0;
    while (tryMove(0, 1, 0))
        ++dropped;
    m_score += dropped * 2;
    lockCurrent();
}

void Engine::lockCurrent()
{
    if (!m_hasPiece)
        return;
    m_field.lock(m_current);
    m_hasPiece = false;
    const auto cleared = m_field.clearLines();
    if (cleared.lines > 0) {
        m_lines += cleared.lines;
        m_level = 1 + m_lines / 10;
        static const int kScore[] = {0, 100, 300, 500, 800};
        m_score += kScore[std::min(cleared.lines, 4)] * m_level;
        pushSpecials(cleared.collected);
        // Classic frequency: 1/3/5/7 specials planted for 1–4 lines.
        const int plant = cleared.lines * 2 - 1;
        m_field.plantSpecials(plant, m_rng);
    }
    m_spawnDelay = m_pieceDelay;
    emit updated();
}

void Engine::applySpecial(Special special)
{
    if (!m_alive)
        return;
    switch (special) {
    case Special::AddLine:
        m_field.addLine(m_rng);
        break;
    case Special::ClearLine:
        m_field.clearBottomLine();
        break;
    case Special::ClearSpecial:
        m_field.clearSpecials(m_rng);
        break;
    case Special::RandomClear:
        m_field.randomClear(m_rng);
        break;
    case Special::Bomb:
        m_field.bomb(m_rng);
        break;
    case Special::Quake:
        m_field.quake(m_rng);
        break;
    case Special::Gravity:
        m_field.gravity();
        break;
    case Special::SwitchField:
        break;
    case Special::Nuke:
        m_field.nuke();
        break;
    case Special::LeftGravity:
        m_field.leftGravity();
        break;
    case Special::PieceChange:
        changeCurrentPiece();
        break;
    case Special::Zebra:
        m_field.zebra();
        break;
    }
    if (m_hasPiece && !m_field.fits(m_current)) {
        if (!tryMove(0, -1, 0) && !tryMove(0, -2, 0)) {
            m_hasPiece = false;
            m_spawnDelay = 200;
        }
    }
    emit updated();
}

void Engine::changeCurrentPiece()
{
    if (!m_hasPiece || !m_alive)
        return;
    for (int i = 0; i < 8; ++i) {
        Piece p = randomPiece();
        p.x = m_current.x;
        p.y = m_current.y;
        p.rotation = m_current.rotation;
        if (m_field.fits(p)) {
            m_current = p;
            emit updated();
            return;
        }
    }
}

void Engine::setField(const Field &field)
{
    m_field = field;
    if (m_hasPiece && !m_field.fits(m_current)) {
        m_hasPiece = false;
        m_spawnDelay = 200;
    }
    emit updated();
}

void Engine::setRemoteState(const Field &field, bool alive, int level, int score, int lines,
                            const QVector<Special> &inventory, bool hasPiece, const Piece &current,
                            const Piece &next)
{
    m_field = field;
    m_alive = alive;
    m_started = true;
    m_hasPiece = hasPiece;
    m_current = current;
    m_next = next;
    m_level = std::max(1, level);
    m_score = std::max(0, score);
    m_lines = std::max(0, lines);
    m_inventory = inventory;
    emit inventoryChanged();
    emit updated();
}

Special Engine::takeSpecial()
{
    if (m_inventory.isEmpty())
        return Special::AddLine;
    const Special s = m_inventory.front();
    m_inventory.pop_front();
    emit inventoryChanged();
    return s;
}

void Engine::pushSpecials(const QVector<Special> &specials)
{
    for (Special s : specials) {
        if (m_inventory.size() >= kMaxInventory)
            break;
        m_inventory.push_back(s);
    }
    if (!specials.isEmpty())
        emit inventoryChanged();
}

Field Engine::snapshot(bool includePiece) const
{
    Field f = m_field;
    if (includePiece && m_hasPiece)
        f.lock(m_current);
    return f;
}

} // namespace tnet
