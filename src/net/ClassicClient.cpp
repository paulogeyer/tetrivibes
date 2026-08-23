#include "ClassicClient.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QIODevice>

namespace tnet {

ClassicClient::ClassicClient(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket, &QTcpSocket::connected, this, &ClassicClient::onConnected);
    connect(&m_socket, &QTcpSocket::disconnected, this, [this]() {
        m_slot = -1;
        m_used.fill(false);
        emit disconnected();
    });
    connect(&m_socket, &QTcpSocket::readyRead, this, &ClassicClient::onReadyRead);
    connect(&m_socket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit errorText(m_socket.errorString());
    });
    m_loginTimer.setSingleShot(true);
    connect(&m_loginTimer, &QTimer::timeout, this, &ClassicClient::onLoginTimeout);
}

void ClassicClient::connectTo(const QString &host, quint16 port, const QString &nick,
                              JoinProtocol proto)
{
    m_nick = nick.isEmpty() ? QStringLiteral("Player") : nick.left(16);
    m_proto = proto == JoinProtocol::Auto ? JoinProtocol::Tetrinet113 : proto;
    m_slot = -1;
    m_loginSent = false;
    m_used.fill(false);
    m_names.fill({});
    m_buffer.clear();
    m_socket.abort();
    m_loginTimer.start(5000);
    m_socket.connectToHost(host, port, QIODevice::ReadWrite, QAbstractSocket::IPv4Protocol);
}

void ClassicClient::disconnectFromHost()
{
    m_loginTimer.stop();
    m_socket.disconnectFromHost();
}

void ClassicClient::onLoginTimeout()
{
    if (m_slot >= 0)
        return;
    emit errorText(QStringLiteral("Login timed out."));
    disconnectFromHost();
}

bool ClassicClient::isConnected() const
{
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

void ClassicClient::onConnected()
{
    emit connected();
    sendLogin();
}

void ClassicClient::sendLogin()
{
    if (m_loginSent || !isConnected())
        return;
    m_loginSent = true;
    QHostAddress serverIp = m_socket.peerAddress();
    if (serverIp.protocol() == QAbstractSocket::IPv6Protocol)
        serverIp = QHostAddress(serverIp.toIPv4Address());
    const QByteArray login = encodeClassicLogin(m_nick, m_proto, serverIp);
    m_socket.write(frameClassic(QString::fromLatin1(login)));
}

void ClassicClient::sendLine(const QString &line)
{
    if (isConnected())
        m_socket.write(frameClassic(line));
}

void ClassicClient::sendChat(const QString &text)
{
    if (m_slot < 0)
        return;
    sendLine(QStringLiteral("pline %1 %2").arg(slotToClassicPlayer(m_slot)).arg(text));
}

void ClassicClient::sendField(const Field &field)
{
    if (m_slot < 0)
        return;
    sendLine(QStringLiteral("f %1 %2")
                 .arg(slotToClassicPlayer(m_slot))
                 .arg(encodeClassicField(field)));
}

void ClassicClient::sendSpecial(int targetSlot, Special special)
{
    if (m_slot < 0)
        return;
    sendLine(QStringLiteral("sb %1 %2 %3")
                 .arg(slotToClassicPlayer(targetSlot))
                 .arg(QChar(specialLetter(special)))
                 .arg(slotToClassicPlayer(m_slot)));
}

void ClassicClient::sendLose()
{
    if (m_slot < 0)
        return;
    sendLine(QStringLiteral("playerlost %1").arg(slotToClassicPlayer(m_slot)));
}

void ClassicClient::sendStart()
{
    if (m_slot < 0)
        return;
    sendLine(QStringLiteral("startgame 1 %1").arg(slotToClassicPlayer(m_slot)));
}

void ClassicClient::sendLevel(int level)
{
    if (m_slot < 0)
        return;
    sendLine(QStringLiteral("lvl %1 %2").arg(slotToClassicPlayer(m_slot)).arg(level));
}

void ClassicClient::sendTeam(const QString &team)
{
    if (m_slot < 0)
        return;
    sendLine(QStringLiteral("team %1 %2").arg(slotToClassicPlayer(m_slot)).arg(team));
}

void ClassicClient::onReadyRead()
{
    m_buffer += m_socket.readAll();
    int idx;
    while ((idx = m_buffer.indexOf(char(0xff))) >= 0) {
        const QByteArray raw = m_buffer.left(idx);
        m_buffer.remove(0, idx + 1);
        ClassicMessage msg;
        parseClassicLine(QString::fromLatin1(raw), msg);
        handle(msg);
    }
}

void ClassicClient::handlePlayernum(const ClassicMessage &msg, bool tetrifast)
{
    if (msg.args.isEmpty())
        return;
    if (tetrifast)
        m_proto = JoinProtocol::TetriFast;
    m_slot = classicPlayerToSlot(msg.args[0].toInt());
    if (m_slot < 0 || m_slot >= kMaxPlayers)
        return;
    m_used[static_cast<size_t>(m_slot)] = true;
    m_names[static_cast<size_t>(m_slot)] = m_nick;
    sendTeam(QString());
    m_loginTimer.stop();
    emit welcomed(m_slot);
    emit playerUpdated(m_slot, m_nick);
}

void ClassicClient::handle(const ClassicMessage &msg)
{
    const QString &c = msg.cmd;
    if (c == QLatin1String("heartbeat")) {
        if (m_slot >= 0)
            sendLine(QString());
        return;
    }
    if (c == QLatin1String("playernum")) {
        handlePlayernum(msg, false);
        return;
    }
    if (c == QLatin1String(")#)(!@(*3")) {
        handlePlayernum(msg, true);
        return;
    }
    if (c == QLatin1String("playerjoin") && msg.args.size() >= 2) {
        const int slot = classicPlayerToSlot(msg.args[0].toInt());
        if (slot < 0 || slot >= kMaxPlayers)
            return;
        m_used[static_cast<size_t>(slot)] = true;
        m_names[static_cast<size_t>(slot)] = QStringList(msg.args.mid(1)).join(QLatin1Char(' '));
        emit playerUpdated(slot, m_names[static_cast<size_t>(slot)]);
        return;
    }
    if (c == QLatin1String("playerleave") && !msg.args.isEmpty()) {
        const int slot = classicPlayerToSlot(msg.args[0].toInt());
        if (slot < 0 || slot >= kMaxPlayers)
            return;
        m_used[static_cast<size_t>(slot)] = false;
        m_names[static_cast<size_t>(slot)].clear();
        emit playerLeft(slot);
        return;
    }
    if ((c == QLatin1String("pline") || c == QLatin1String("plineact")) && msg.args.size() >= 2) {
        const int slot = classicPlayerToSlot(msg.args[0].toInt());
        emit chatReceived(slot, QStringList(msg.args.mid(1)).join(QLatin1Char(' ')));
        return;
    }
    if (c == QLatin1String("gmsg") && !msg.args.isEmpty()) {
        emit chatReceived(-1, msg.args.join(QLatin1Char(' ')));
        return;
    }
    if (c == QLatin1String("newgame") || c == QLatin1String("*******")) {
        const bool fast = c == QLatin1String("*******");
        if (fast)
            m_proto = JoinProtocol::TetriFast;
        const int height = msg.args.value(0).toInt();
        const int level = msg.args.value(1).toInt();
        emit gameStarted(height, level > 0 ? level : 1, fast);
        return;
    }
    if (c == QLatin1String("endgame")) {
        emit gameEnded();
        return;
    }
    if (c == QLatin1String("ingame")) {
        emit chatReceived(-1, QStringLiteral("Game already in progress"));
        return;
    }
    if (c == QLatin1String("f") && msg.args.size() >= 2) {
        const int slot = classicPlayerToSlot(msg.args[0].toInt());
        Field field;
        applyClassicField(field, msg.args[1]);
        emit fieldReceived(slot, field);
        return;
    }
    if (c == QLatin1String("sb") && msg.args.size() >= 3) {
        const int target = classicPlayerToSlot(msg.args[0].toInt());
        const int from = classicPlayerToSlot(msg.args[2].toInt());
        const QString spec = msg.args[1];
        if (spec.startsWith(QLatin1String("cs")))
            return;
        const Cell cell = charToCell(spec.isEmpty() ? 'a' : spec[0].toLatin1());
        if (isSpecial(cell))
            emit specialReceived(from, target, cellToSpecial(cell));
        return;
    }
    if (c == QLatin1String("playerlost") && !msg.args.isEmpty()) {
        emit playerLost(classicPlayerToSlot(msg.args[0].toInt()));
        return;
    }
    if (c == QLatin1String("playerwon") && !msg.args.isEmpty()) {
        emit playerWon(classicPlayerToSlot(msg.args[0].toInt()));
        return;
    }
    if (c == QLatin1String("pause") && !msg.args.isEmpty()) {
        emit paused(msg.args[0] != QLatin1String("0"));
        return;
    }
    if (c == QLatin1String("winlist")) {
        emit winlist(msg.args.join(QLatin1Char(' ')));
        return;
    }
    if (c == QLatin1String("noconnecting")) {
        emit errorText(msg.args.isEmpty() ? QStringLiteral("Connection denied")
                                          : msg.args.join(QLatin1Char(' ')));
        return;
    }
    if (c == QLatin1String("lvl") && msg.args.size() >= 2 && msg.args[0] == QLatin1String("0")
        && msg.args[1] == QLatin1String("0"))
        return;
}

QString ClassicClient::playerName(int slot) const
{
    if (slot < 0 || slot >= kMaxPlayers)
        return {};
    return m_names[static_cast<size_t>(slot)];
}

bool ClassicClient::slotUsed(int slot) const
{
    if (slot < 0 || slot >= kMaxPlayers)
        return false;
    return m_used[static_cast<size_t>(slot)];
}

} // namespace tnet
