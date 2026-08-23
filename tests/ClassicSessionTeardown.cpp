// Destroying a connected classic session must not crash.
#include "game/Types.h"
#include "session/ClassicSession.h"

#include <QCoreApplication>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    auto *classic = new tnet::ClassicSession(QStringLiteral("127.0.0.1"), 9,
                                             QStringLiteral("TestPlayer"),
                                             tnet::JoinProtocol::Tetrinet113);
    classic->begin();
    QTimer::singleShot(50, &app, &QCoreApplication::quit);
    app.exec();
    delete classic;
    QCoreApplication::processEvents();
    return 0;
}
