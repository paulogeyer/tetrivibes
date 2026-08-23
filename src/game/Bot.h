#pragma once

#include "Engine.h"

#include <QObject>

namespace tnet {

// Greedy placement: try rotations/columns, score holes and height, then hard-drop.
class Bot : public QObject {
    Q_OBJECT
public:
    explicit Bot(Engine *engine, QObject *parent = nullptr);

    void thinkAndAct();
    int chooseTarget(const QVector<int> &heights, int selfSlot) const;

private:
    struct Plan {
        int x = 4;
        int rot = 0;
        int score = -1000000;
    };

    Plan bestPlan() const;
    int evaluate(const Field &field, int lines) const;

    Engine *m_engine;
};

} // namespace tnet
