#include "ClassicProtocol.h"

#include "game/Field.h"

#include <QAbstractSocket>
#include <QRandomGenerator>

namespace tnet {
namespace {

QByteArray hexByte(int value)
{
    return QByteArray::number(value & 0xff, 16).rightJustified(2, '0').toUpper();
}

} // namespace

// Official 1.13 login: "tetrisstart|tetrifaster <nick> 1.13", XOR-encoded
// with the decimal digits of 54*A+41*B+29*C+17*D from the *server* IPv4.
QByteArray encodeClassicLogin(const QString &nick, JoinProtocol proto, const QHostAddress &serverIp)
{
    const QString clean = nick.trimmed().left(16);
    const QString hello = proto == JoinProtocol::TetriFast ? QStringLiteral("tetrifaster ")
                                                           : QStringLiteral("tetrisstart ");
    const QString plain = hello + clean + QStringLiteral(" 1.13");

    QHostAddress v4 = serverIp;
    if (v4.protocol() == QAbstractSocket::IPv6Protocol)
        v4 = QHostAddress(v4.toIPv4Address());
    const quint32 ip = v4.toIPv4Address();
    const int a = (ip >> 24) & 0xff;
    const int b = (ip >> 16) & 0xff;
    const int c = (ip >> 8) & 0xff;
    const int d = ip & 0xff;
    const int ipHash = 54 * a + 41 * b + 29 * c + 17 * d;
    const QByteArray raw = plain.toLatin1();
    const QByteArray key = QByteArray::number(ipHash);
    int cur = static_cast<int>(QRandomGenerator::global()->bounded(256));
    QByteArray out = hexByte(cur);
    for (int i = 0; i < raw.size(); ++i) {
        cur = ((cur + static_cast<unsigned char>(raw[i])) % 255)
            ^ static_cast<unsigned char>(key[i % key.size()]);
        out += hexByte(cur);
    }
    return out;
}

QByteArray frameClassic(const QString &line)
{
    QByteArray out = line.toLatin1();
    out.append(char(0xff));
    return out;
}

bool parseClassicLine(const QString &line, ClassicMessage &msg)
{
    msg = ClassicMessage{};
    msg.raw = line;
    if (line.isEmpty()) {
        msg.cmd = QStringLiteral("heartbeat");
        return true;
    }
    const QStringList parts = line.split(QLatin1Char(' '));
    msg.cmd = parts[0];
    if (parts.size() > 1)
        msg.args = parts.mid(1);
    return true;
}

int classicPlayerToSlot(int playernum)
{
    return playernum - 1;
}

int slotToClassicPlayer(int slot)
{
    return slot + 1;
}

QString encodeClassicField(const Field &field)
{
    return field.encode();
}

// Full update: 264 cell chars. Partial: type ('!'..) + x/y encoded as 0x33+coord.
void applyClassicField(Field &field, const QString &data)
{
    if (data.size() == kFieldWidth * kFieldHeight) {
        if (Field::isValidEncoding(data))
            field = Field::decode(data);
        return;
    }
    int i = 0;
    while (i + 2 < data.size()) {
        const char type = data[i].toLatin1();
        const int x = static_cast<unsigned char>(data[i + 1].toLatin1()) - 0x33;
        const int y = static_cast<unsigned char>(data[i + 2].toLatin1()) - 0x33;
        i += 3;
        if (type < '!' || type > '/')
            continue;
        const int idx = type - '!';
        Cell cell = Cell::Empty;
        if (idx >= 1 && idx <= 5)
            cell = static_cast<Cell>(idx);
        else if (idx >= 6)
            cell = specialToCell(static_cast<Special>(idx - 6));
        if (x >= 0 && x < kFieldWidth && y >= 0 && y < kFieldHeight)
            field.set(x, y, cell);
    }
}

} // namespace tnet
