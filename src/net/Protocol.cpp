#include "Protocol.h"

#include <QList>

namespace tnet {
namespace {

bool parseInt(const QString &text, int &value)
{
    bool ok = false;
    const int parsed = text.toInt(&ok);
    if (!ok)
        return false;
    value = parsed;
    return true;
}

QString lineText(const QString &text)
{
    QString clean = text;
    clean.remove(QLatin1Char('\r'));
    clean.remove(QLatin1Char('\n'));
    return clean;
}

} // namespace

QByteArray encodeMessage(const Message &msg)
{
    QString line;
    switch (msg.type) {
    case Message::Nick:
        line = QStringLiteral("NICK %1").arg(lineText(msg.text));
        break;
    case Message::Welcome:
        line = QStringLiteral("WELCOME %1").arg(msg.slot);
        break;
    case Message::Player:
        line = QStringLiteral("PLAYER %1 %2").arg(msg.slot).arg(lineText(msg.text));
        break;
    case Message::Left:
        line = QStringLiteral("LEFT %1").arg(msg.slot);
        break;
    case Message::Chat:
        line = QStringLiteral("CHAT %1 %2").arg(msg.slot).arg(lineText(msg.text));
        break;
    case Message::Start:
        line = QStringLiteral("START %1").arg(msg.value);
        break;
    case Message::Field:
        line = QStringLiteral("FIELD %1 %2").arg(msg.slot).arg(lineText(msg.data));
        break;
    case Message::Special:
        line = QStringLiteral("SPECIAL %1 %2 %3")
                   .arg(msg.slot)
                   .arg(msg.target)
                   .arg(lineText(msg.text));
        break;
    case Message::Lose:
        line = QStringLiteral("LOSE %1").arg(msg.slot);
        break;
    case Message::Win:
        line = QStringLiteral("WIN %1").arg(msg.slot);
        break;
    case Message::Ping:
        line = QStringLiteral("PING");
        break;
    case Message::Pong:
        line = QStringLiteral("PONG");
        break;
    case Message::Query:
        line = QStringLiteral("QUERY");
        break;
    case Message::Status:
        line = QStringLiteral("STATUS %1 %2 %3 %4")
                   .arg(msg.slot)
                   .arg(msg.target)
                   .arg(msg.value)
                   .arg(lineText(msg.text));
        break;
    default:
        return {};
    }
    QByteArray encoded = line.toUtf8();
    if (encoded.size() >= kMaxNativeFrameSize)
        return {};
    encoded += '\n';
    return encoded;
}

bool decodeMessage(const QByteArray &raw, Message &msg)
{
    const QString line = QString::fromUtf8(raw).trimmed();
    if (line.isEmpty())
        return false;
    const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const QString cmd = parts[0];
    msg = Message{};

    auto restFrom = [&](int idx) {
        if (idx >= parts.size())
            return QString();
        return QStringList(parts.mid(idx)).join(QLatin1Char(' '));
    };

    if (cmd == QLatin1String("NICK") && parts.size() >= 2) {
        msg.type = Message::Nick;
        msg.text = restFrom(1).left(16);
        return true;
    }
    if (cmd == QLatin1String("WELCOME") && parts.size() >= 2) {
        if (!parseInt(parts[1], msg.slot))
            return false;
        msg.type = Message::Welcome;
        return true;
    }
    if (cmd == QLatin1String("PLAYER") && parts.size() >= 3) {
        if (!parseInt(parts[1], msg.slot))
            return false;
        msg.type = Message::Player;
        msg.text = restFrom(2);
        return true;
    }
    if (cmd == QLatin1String("LEFT") && parts.size() >= 2) {
        if (!parseInt(parts[1], msg.slot))
            return false;
        msg.type = Message::Left;
        return true;
    }
    if (cmd == QLatin1String("CHAT") && parts.size() >= 3) {
        if (!parseInt(parts[1], msg.slot))
            return false;
        msg.type = Message::Chat;
        msg.text = restFrom(2);
        return true;
    }
    if (cmd == QLatin1String("START") && parts.size() >= 2) {
        if (!parseInt(parts[1], msg.value))
            return false;
        msg.type = Message::Start;
        return true;
    }
    if (cmd == QLatin1String("FIELD") && parts.size() >= 3) {
        if (!parseInt(parts[1], msg.slot))
            return false;
        msg.type = Message::Field;
        msg.data = parts[2];
        return true;
    }
    if (cmd == QLatin1String("SPECIAL") && parts.size() >= 4) {
        if (!parseInt(parts[1], msg.slot) || !parseInt(parts[2], msg.target))
            return false;
        msg.type = Message::Special;
        msg.text = parts[3];
        return true;
    }
    if (cmd == QLatin1String("LOSE") && parts.size() >= 2) {
        if (!parseInt(parts[1], msg.slot))
            return false;
        msg.type = Message::Lose;
        return true;
    }
    if (cmd == QLatin1String("WIN") && parts.size() >= 2) {
        if (!parseInt(parts[1], msg.slot))
            return false;
        msg.type = Message::Win;
        return true;
    }
    if (cmd == QLatin1String("PING")) {
        msg.type = Message::Ping;
        return true;
    }
    if (cmd == QLatin1String("PONG")) {
        msg.type = Message::Pong;
        return true;
    }
    if (cmd == QLatin1String("QUERY")) {
        msg.type = Message::Query;
        return true;
    }
    if (cmd == QLatin1String("STATUS") && parts.size() >= 4) {
        if (!parseInt(parts[1], msg.slot) || !parseInt(parts[2], msg.target)
            || !parseInt(parts[3], msg.value))
            return false;
        msg.type = Message::Status;
        msg.text = restFrom(4);
        return true;
    }
    msg.type = Message::Unknown;
    return false;
}

} // namespace tnet
