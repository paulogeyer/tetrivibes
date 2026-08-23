#pragma once

#include "Field.h"

#include <random>
#include <vector>

namespace tnet {

// Server-side simulation for the hidden native mode. Its rendered 12x22 board is sent through
// the regular STATE field payload, so the wire protocol still looks like an ordinary match.
class InvadersEngine {
public:
    void reset(uint32_t seed);
    void tick(int ms);

    bool moveLeft();
    bool moveRight();
    bool fire();
    void autoplay();

    const Field &field() const { return m_field; }
    bool alive() const { return m_alive; }
    int wave() const { return m_wave; }
    int score() const { return m_score; }
    int lives() const { return m_lives; }

private:
    struct Actor {
        int x = 0;
        int y = 0;
    };

    void spawnWave();
    void stepInvaders();
    void stepProjectiles();
    void enemyFire();
    void hitPlayer();
    void render();

    Field m_field;
    std::vector<Actor> m_invaders;
    std::vector<Actor> m_shots;
    std::vector<Actor> m_enemyShots;
    std::mt19937 m_rng;
    bool m_alive = false;
    int m_shipX = kFieldWidth / 2;
    int m_direction = 1;
    int m_wave = 1;
    int m_score = 0;
    int m_lives = 3;
    int m_invaderAcc = 0;
    int m_projectileAcc = 0;
    int m_enemyFireAcc = 0;
    int m_fireCooldown = 0;
};

} // namespace tnet
