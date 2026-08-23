// Parses a native STATE snapshot, including score fields and piece token.
#include "game/Types.h"
#include "net/Protocol.h"

#include <QString>
#include <cassert>

int main()
{
    tnet::Message message;
    const QString emptyField(tnet::kFieldWidth * tnet::kFieldHeight, QLatin1Char('0'));
    assert(tnet::decodeMessage(QStringLiteral("STATE 0 1 2 300 4 a %1 -:0,1").arg(emptyField).toUtf8(),
                               message));
    assert(message.type == tnet::Message::State);
    assert(message.level == 2);
    assert(message.score == 300);
    assert(message.lines == 4);
    assert(message.piece == QLatin1String("-:0,1"));
    return 0;
}
