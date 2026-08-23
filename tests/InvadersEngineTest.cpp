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

    for (int i = 0; i < 20; ++i)
        game.moveLeft();
    assert(!game.moveLeft());
    assert(game.fire());
    assert(!game.fire());
    game.tick(200);
    assert(game.fire());
    assert(game.field().filledCount() > 0);
    return 0;
}
