#include "game/InvadersEngine.h"

#include <cassert>

int main()
{
    tnet::InvadersEngine game;
    game.reset(1978);
    assert(game.alive());
    assert(game.wave() == 1);
    assert(game.lives() == 3);
    assert(game.score() == 0);
    assert(game.field().filledCount() >= 24);

    game.pushPowerup(tnet::Special::ClearLine);
    assert(game.inventory().size() == 1);
    assert(game.takePowerup() == tnet::Special::ClearLine);
    assert(game.inventory().isEmpty());
    game.applyPowerup(tnet::Special::ClearLine);
    assert(game.lives() == 4);

    for (int i = 0; i < tnet::kMaxInventory + 4; ++i)
        game.pushPowerup(tnet::Special::Bomb);
    assert(game.inventory().size() == tnet::kMaxInventory);

    for (int i = 0; i < 20; ++i)
        game.moveLeft();
    assert(!game.moveLeft());
    assert(game.fire());
    assert(!game.fire());
    game.tick(200);
    assert(game.fire());
    assert(game.field().filledCount() > 0);

    const int firstWave = game.wave();
    game.applyPowerup(tnet::Special::Nuke);
    game.tick(80);
    assert(game.wave() == firstWave + 1);
    (void)firstWave;
    return 0;
}
