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
    m_shots.clear();
    m_enemyShots.clear();
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
    if (!m_alive || m_fireCooldown > 0 || m_shots.size() >= 3)
        return false;
    m_shots.push_back({m_shipX, kShipRow - 1});
    m_fireCooldown = 180;
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

void InvadersEngine::tick(int ms)
{
    if (!m_alive)
        return;
    m_fireCooldown = std::max(0, m_fireCooldown - ms);
    m_invaderAcc += ms;
    m_projectileAcc += ms;
    m_enemyFireAcc += ms;

    const int invaderDelay = std::max(120, 520 - m_wave * 25);
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
