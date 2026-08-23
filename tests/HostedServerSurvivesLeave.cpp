// Leaving a hosted session keeps the server listening so the host can rejoin.
#include "net/Server.h"
#include "session/NetSession.h"

#include <QCoreApplication>
#include <QTimer>
#include <cassert>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    auto *server = new tnet::Server;
    server->setBotCount(1);
    assert(server->listen(0));
    const quint16 port = server->port();

    auto *session = new tnet::NetSession(true, QStringLiteral("127.0.0.1"), port,
                                         QStringLiteral("Host"));
    session->attachServer(server);
    session->begin();
    QTimer::singleShot(50, &app, &QCoreApplication::quit);
    app.exec();
    assert(session->canStart());
    delete session;
    QCoreApplication::processEvents();
    assert(server->isListening());
    assert(server->port() == port);

    auto *again = new tnet::NetSession(true, QStringLiteral("127.0.0.1"), port,
                                       QStringLiteral("Host"));
    again->attachServer(server);
    again->begin();
    QTimer::singleShot(50, &app, &QCoreApplication::quit);
    app.exec();
    assert(again->canStart());
    delete again;
    QCoreApplication::processEvents();
    assert(server->isListening());

    server->stop();
    delete server;
    return 0;
}
