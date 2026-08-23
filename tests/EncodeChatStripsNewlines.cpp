// Encoded chat text cannot contain CR/LF, so injected commands stay one frame.
#include "net/Protocol.h"

#include <cassert>

int main()
{
    tnet::Message message;
    message.type = tnet::Message::Chat;
    message.slot = 0;
    message.text = QStringLiteral("hello\nWIN 1\r");
    assert(tnet::encodeMessage(message) == QByteArray("CHAT 0 helloWIN 1\n"));
    return 0;
}
