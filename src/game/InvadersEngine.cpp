#include "InvadersEngine.h"

#include <algorithm>
#include <cmath>

namespace tnet {
namespace {

constexpr int kShipRow = kFieldHeight - 2;

template<typename ActorT>
bool samePosition(const ActorT &a, const ActorT &b)
{
    return a.x == b.x && a.y == b.y;
}

} // namespace

void InvadersEngine::reset(uint32_t seed)
{
    m_rng.seed(seed);
    m_alive = true;
    m_shipX = kFieldWidth / 2;
    m_direction = 1;
    m_wave = 1;
    m_score = 0;
    m_lives = 3;
    m_invaderAcc = 0;
    m_projectileAcc = 0;
    m_enemyFireAcc = 0;
    m_fireCooldown = 0;
    m_slowTimer = 0;
    m_rapidFireTimer = 0;
    m_shots.clear();
    m_enemyShots.clear();
    m_inventory.clear();
    spawnWave();
    render();
}

void InvadersEngine::spawnWave()
{
    m_invaders.clear();
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 5; ++column)
            m_invaders.push_back({1 + column * 2, 2 + row * 2});
    }
    m_direction = 1;
    m_invaderAcc = 0;
}

bool InvadersEngine::moveLeft()
{
    if (!m_alive || m_shipX <= 1)
        return false;
    --m_shipX;
    render();
    return true;
}

bool InvadersEngine::moveRight()
{
    if (!m_alive || m_shipX >= kFieldWidth - 2)
        return false;
    ++m_shipX;
    render();
    return true;
}

bool InvadersEngine::fire()
{
    const size_t shotLimit = m_rapidFireTimer > 0 ? 6 : 3;
    if (!m_alive || m_fireCooldown > 0 || m_shots.size() >= shotLimit)
        return false;
    m_shots.push_back({m_shipX, kShipRow - 1});
    m_fireCooldown = m_rapidFireTimer > 0 ? 70 : 180;
    render();
    return true;
}

void InvadersEngine::autoplay()
{
    if (!m_alive || m_invaders.empty())
        return;
    const auto closest = std::min_element(
        m_invaders.begin(), m_invaders.end(), [this](const Actor &a, const Actor &b) {
            return std::abs(a.x - m_shipX) < std::abs(b.x - m_shipX);
        });
    if (closest->x < m_shipX)
        moveLeft();
    else if (closest->x > m_shipX)
        moveRight();
    else
        fire();
}

void InvadersEngine::stepInvaders()
{
    if (m_invaders.empty())
        return;
    bool atEdge = false;
    for (const Actor &invader : m_invaders) {
        const int nextX = invader.x + m_direction;
        if (nextX < 0 || nextX >= kFieldWidth) {
            atEdge = true;
            break;
        }
    }
    if (atEdge) {
        m_direction = -m_direction;
        for (Actor &invader : m_invaders) {
            ++invader.y;
            if (invader.y >= kShipRow)
                m_lives = 0;
        }
    } else {
        for (Actor &invader : m_invaders)
            invader.x += m_direction;
    }
    if (m_lives <= 0)
        m_alive = false;
}

void InvadersEngine::hitPlayer()
{
    if (--m_lives <= 0) {
        m_lives = 0;
        m_alive = false;
    }
    m_enemyShots.clear();
    m_shipX = kFieldWidth / 2;
}

void InvadersEngine::stepProjectiles()
{
    for (Actor &shot : m_shots)
        --shot.y;
    for (Actor &shot : m_enemyShots)
        ++shot.y;

    for (auto shot = m_shots.begin(); shot != m_shots.end();) {
        const auto invader = std::find_if(m_invaders.begin(), m_invaders.end(),
                                          [&](const Actor &a) { return samePosition(a, *shot); });
        if (invader != m_invaders.end()) {
            m_invaders.erase(invader);
            shot = m_shots.erase(shot);
            m_score += 10 * m_wave;
            maybeCollectPowerup();
        } else if (shot->y < 0) {
            shot = m_shots.erase(shot);
        } else {
            ++shot;
        }
    }

    const bool playerHit = std::any_of(m_enemyShots.begin(), m_enemyShots.end(), [this](const Actor &s) {
        return s.y >= kShipRow - 1 && std::abs(s.x - m_shipX) <= (s.y >= kShipRow ? 1 : 0);
    });
    m_enemyShots.erase(
        std::remove_if(m_enemyShots.begin(), m_enemyShots.end(),
                       [](const Actor &s) { return s.y >= kFieldHeight; }),
        m_enemyShots.end());
    if (playerHit)
        hitPlayer();

    if (m_alive && m_invaders.empty()) {
        ++m_wave;
        m_shots.clear();
        m_enemyShots.clear();
        spawnWave();
    }
}

void InvadersEngine::enemyFire()
{
    if (m_invaders.empty() || m_enemyShots.size() >= 4)
        return;
    std::uniform_int_distribution<size_t> pick(0, m_invaders.size() - 1);
    const Actor &source = m_invaders[pick(m_rng)];
    m_enemyShots.push_back({source.x, source.y + 1});
}

void InvadersEngine::pushPowerup(Special special)
{
    if (m_inventory.size() < kMaxInventory)
        m_inventory.push_back(special);
}

Special InvadersEngine::takePowerup()
{
    if (m_inventory.isEmpty())
        return Special::AddLine;
    const Special special = m_inventory.front();
    m_inventory.pop_front();
    return special;
}

void InvadersEngine::maybeCollectPowerup()
{
    std::uniform_int_distribution<int> drop(0, 99);
    if (drop(m_rng) >= 28 || m_inventory.size() >= kMaxInventory)
        return;
    std::uniform_int_distribution<int> type(0, static_cast<int>(Special::Zebra));
    pushPowerup(static_cast<Special>(type(m_rng)));
}

void InvadersEngine::applyPowerup(Special special)
{
    if (!m_alive)
        return;
    switch (special) {
    case Special::AddLine:
        for (Actor &invader : m_invaders)
            ++invader.y;
        for (int column = 0; column < 5; ++column)
            m_invaders.push_back({1 + column * 2, 1});
        break;
    case Special::ClearLine:
        m_lives = std::min(5, m_lives + 1);
        break;
    case Special::ClearSpecial:
        m_enemyShots.clear();
        break;
    case Special::RandomClear:
        m_shots.clear();
        break;
    case Special::Bomb:
        m_shots.clear();
        for (int dx = -1; dx <= 1; ++dx)
            m_enemyShots.push_back({std::clamp(m_shipX + dx, 0, kFieldWidth - 1),
                                    kShipRow - 5});
        break;
    case Special::Quake:
        for (Actor &invader : m_invaders)
            invader.y += 2;
        break;
    case Special::Gravity:
        m_slowTimer = 8000;
        break;
    case Special::SwitchField:
        break; // The server swaps both players' battlefields atomically.
    case Special::Nuke:
        m_invaders.clear();
        break;
    case Special::LeftGravity:
        m_shipX = 1;
        break;
    case Special::PieceChange:
        m_rapidFireTimer = 8000;
        m_fireCooldown = 0;
        break;
    case Special::Zebra:
        for (int x = 0; x < kFieldWidth; x += 2)
            m_enemyShots.push_back({x, kShipRow - 7});
        break;
    }
    for (const Actor &invader : m_invaders) {
        if (invader.y >= kShipRow) {
            m_lives = 0;
            m_alive = false;
            break;
        }
    }
    render();
}

void InvadersEngine::swapBattlefield(InvadersEngine &other)
{
    using std::swap;
    swap(m_invaders, other.m_invaders);
    swap(m_shots, other.m_shots);
    swap(m_enemyShots, other.m_enemyShots);
    swap(m_direction, other.m_direction);
    swap(m_invaderAcc, other.m_invaderAcc);
    swap(m_projectileAcc, other.m_projectileAcc);
    swap(m_enemyFireAcc, other.m_enemyFireAcc);
    render();
    other.render();
}

void InvadersEngine::tick(int ms)
{
    if (!m_alive)
        return;
    m_fireCooldown = std::max(0, m_fireCooldown - ms);
    m_slowTimer = std::max(0, m_slowTimer - ms);
    m_rapidFireTimer = std::max(0, m_rapidFireTimer - ms);
    m_invaderAcc += ms;
    m_projectileAcc += ms;
    m_enemyFireAcc += ms;

    const int invaderDelay = std::max(120, 520 - m_wave * 25) + (m_slowTimer > 0 ? 280 : 0);
    while (m_invaderAcc >= invaderDelay) {
        m_invaderAcc -= invaderDelay;
        stepInvaders();
    }
    while (m_projectileAcc >= 80) {
        m_projectileAcc -= 80;
        stepProjectiles();
    }
    const int enemyFireDelay = std::max(280, 850 - m_wave * 30);
    if (m_enemyFireAcc >= enemyFireDelay) {
        m_enemyFireAcc = 0;
        enemyFire();
    }
    render();
}

void InvadersEngine::render()
{
    m_field.clear();
    for (const Actor &invader : m_invaders)
        m_field.set(invader.x, invader.y, Cell::C3);
    for (const Actor &shot : m_shots)
        m_field.set(shot.x, shot.y, Cell::C2);
    for (const Actor &shot : m_enemyShots)
        m_field.set(shot.x, shot.y, Cell::C5);
    if (m_alive) {
        m_field.set(m_shipX, kShipRow - 1, Cell::C1);
        m_field.set(m_shipX - 1, kShipRow, Cell::C1);
        m_field.set(m_shipX, kShipRow, Cell::C1);
        m_field.set(m_shipX + 1, kShipRow, Cell::C1);
    }
}

} // namespace tnet
