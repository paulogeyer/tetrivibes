// A native host with practice bots can start, receive state, and accept input.
#include "game/Types.h"
#include "session/NetSession.h"

#include <QCoreApplication>
#include <QTimer>
#include <cassert>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    auto *session = new tnet::NetSession(true, {}, 0, QStringLiteral("TestPlayer"), {},
                                         tnet::kMaxPlayers, 2);
    session->begin();
    QTimer::singleShot(50, &app, &QCoreApplication::quit);
    app.exec();
    assert(session->canStart());
    int occupied = 0;
    for (int i = 0; i < tnet::kMaxPlayers; ++i) {
        if (session->slotOccupied(i))
            ++occupied;
    }
    assert(occupied >= 3);
    session->startGame();
    QTimer::singleShot(100, &app, &QCoreApplication::quit);
    app.exec();
    assert(session->playing());
    assert(session->localEngine()->started());
    session->moveLeft();
    session->hardDrop();
    QTimer::singleShot(50, &app, &QCoreApplication::quit);
    app.exec();
    delete session;
    QCoreApplication::processEvents();
    return 0;
}
