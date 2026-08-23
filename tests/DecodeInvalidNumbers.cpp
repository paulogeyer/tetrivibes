// Rejects native frames whose numeric fields are not valid integers.
#include "net/Protocol.h"

#include <cassert>

int main()
{
    tnet::Message message;
    assert(!tnet::decodeMessage("WELCOME nope", message));
    assert(!tnet::decodeMessage("SPECIAL 0 x a", message));
    return 0;
}
