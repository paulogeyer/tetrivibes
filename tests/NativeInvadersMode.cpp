#include "game/Types.h"
#include "session/NetSession.h"

#include <QCoreApplication>
#include <QTimer>
#include <cassert>

namespace {

void waitForEvents(QCoreApplication &app, int ms)
{
    QTimer::singleShot(ms, &app, &QCoreApplication::quit);
    app.exec();
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    auto *session = new tnet::NetSession(true, {}, 0, QStringLiteral("Defender"), {},
                                         tnet::kMaxPlayers, 0);
    session->begin();
    waitForEvents(app, 50);

    assert(session->canStart());
    assert(session->activateSecretMode());
    assert(session->secretMode());
    session->startGame();
    waitForEvents(app, 80);

    assert(session->playing());
    assert(session->secretMode());
    assert(session->localEngine()->started());
    assert(session->localEngine()->level() == 1);
    assert(session->localEngine()->lines() == 3);
    assert(session->localEngine()->field().filledCount() >= 24);

    session->moveLeft();
    session->hardDrop();
    waitForEvents(app, 80);
    assert(session->localEngine()->field().filledCount() > 0);

    delete session;
    QCoreApplication::processEvents();
    return 0;
}
