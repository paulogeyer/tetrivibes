#include "game/Field.h"
#include "net/Protocol.h"
#include "session/ClassicSession.h"
#include "session/NetSession.h"

#include <QCoreApplication>
#include <QTimer>
#include <cassert>

int main(int argc, char *argv[])
{
    using namespace tnet;

    QCoreApplication app(argc, argv);

    Message message;
    assert(!decodeMessage("WELCOME nope", message));
    assert(!decodeMessage("SPECIAL 0 x a", message));
    assert(decodeMessage("SPECIAL 0 1 a", message));
    assert(message.slot == 0);
    assert(message.target == 1);

    message = Message{};
    message.type = Message::Chat;
    message.slot = 0;
    message.text = QStringLiteral("hello\nWIN 1\r");
    const QByteArray frame = encodeMessage(message);
    assert(frame == QByteArray("CHAT 0 helloWIN 1\n"));
    message.text = QString(kMaxNativeFrameSize, QLatin1Char('x'));
    assert(encodeMessage(message).isEmpty());

    const QString emptyField(kFieldWidth * kFieldHeight, QLatin1Char('0'));
    assert(Field::isValidEncoding(emptyField));
    assert(!Field::isValidEncoding(emptyField.left(emptyField.size() - 1)));
    assert(!Field::isValidEncoding(emptyField + QLatin1Char('0')));
    QString invalid = emptyField;
    invalid[0] = QLatin1Char('!');
    assert(!Field::isValidEncoding(invalid));

    auto *session = new NetSession(true, {}, 0, QStringLiteral("TestPlayer"));
    session->begin();
    QTimer::singleShot(50, &app, &QCoreApplication::quit);
    app.exec();
    delete session;
    QCoreApplication::processEvents();

    auto *classic = new ClassicSession(QStringLiteral("127.0.0.1"), 9,
                                       QStringLiteral("TestPlayer"), JoinProtocol::Tetrinet113);
    classic->begin();
    QTimer::singleShot(50, &app, &QCoreApplication::quit);
    app.exec();
    delete classic;
    QCoreApplication::processEvents();

    return 0;
}
