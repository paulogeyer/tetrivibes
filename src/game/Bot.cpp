#include "Bot.h"

#include <algorithm>
#include <limits>

namespace tnet {

Bot::Bot(Engine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
}

Bot::Plan Bot::bestPlan() const
{
    Plan best;
    if (!m_engine->hasPiece() || !m_engine->alive())
        return best;

    const Field base = m_engine->field();
    Piece piece = m_engine->current();

    for (int rot = 0; rot < 4; ++rot) {
        Piece rotated = piece;
        rotated.rotation = rot;
        rotated.x = 0;
        rotated.y = 0;
        while (rotated.x > -2 && base.fits(rotated))
            --rotated.x;
        ++rotated.x;

        for (int x = -2; x < kFieldWidth; ++x) {
            Piece test = rotated;
            test.x = x;
            test.y = 0;
            if (!base.fits(test))
                continue;
            while (true) {
                Piece down = test;
                down.y += 1;
                if (!base.fits(down))
                    break;
                test = down;
            }
            Field sim = base;
            sim.lock(test);
            const auto cleared = sim.clearLines();
            const int score = evaluate(sim, cleared.lines);
            if (score > best.score) {
                best.score = score;
                best.x = x;
                best.rot = rot;
            }
        }
    }
    return best;
}

int Bot::evaluate(const Field &field, int lines) const
{
    return lines * 800 - field.holeCount() * 180 - field.stackHeight() * 12
        - field.filledCount() * 2;
}

void Bot::thinkAndAct()
{
    if (!m_engine->alive() || !m_engine->hasPiece())
        return;

    const Plan plan = bestPlan();
    const int rotDelta = (plan.rot - m_engine->current().rotation) & 3;
    if (rotDelta != 0) {
        m_engine->rotate(rotDelta == 3 ? -1 : 1);
        return;
    }
    if (m_engine->current().x < plan.x) {
        m_engine->moveRight();
        return;
    }
    if (m_engine->current().x > plan.x) {
        m_engine->moveLeft();
        return;
    }
    m_engine->hardDrop();
}

int Bot::chooseTarget(const QVector<int> &heights, int selfSlot) const
{
    int best = -1;
    int bestH = -1;
    for (int i = 0; i < heights.size(); ++i) {
        if (i == selfSlot || heights[i] < 0)
            continue;
        if (heights[i] > bestH) {
            bestH = heights[i];
            best = i;
        }
    }
    if (best < 0) {
        for (int i = 0; i < heights.size(); ++i) {
            if (i != selfSlot && heights[i] >= 0)
                return i;
        }
        return selfSlot;
    }
    return best;
}

} // namespace tnet
