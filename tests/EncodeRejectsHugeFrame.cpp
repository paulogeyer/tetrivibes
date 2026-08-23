// Oversized outbound native frames are dropped instead of being sent.
#include "net/Protocol.h"

#include <cassert>

int main()
{
    tnet::Message message;
    message.type = tnet::Message::Chat;
    message.slot = 0;
    message.text = QString(tnet::kMaxNativeFrameSize, QLatin1Char('x'));
    assert(tnet::encodeMessage(message).isEmpty());
    return 0;
}
