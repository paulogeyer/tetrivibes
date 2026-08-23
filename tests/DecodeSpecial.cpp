// Parses a valid native SPECIAL frame and its slot/target values.
#include "net/Protocol.h"

#include <cassert>

int main()
{
    tnet::Message message;
    assert(tnet::decodeMessage("SPECIAL 0 1 a", message));
    assert(message.type == tnet::Message::Special);
    assert(message.slot == 0);
    assert(message.target == 1);
    return 0;
}
