#include "Server.h"

#include <QHostAddress>
#include <QRandomGenerator>
#include <algorithm>

namespace tnet {

constexpr quint16 kAnnouncePort = 31458;

Server::Server(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &Server::onNewConnection);
    connect(&m_announceTimer, &QTimer::timeout, this, &Server::announce);
    m_announceTimer.setInterval(2000);
}

Server::~Server()
{
    stop();
}

bool Server::listen(quint16 port)
{
    stop();
    if (!m_server.listen(QHostAddress::Any, port)) {
        emit logLine(QStringLiteral("Could not bind port %1").arg(port));
        return false;
    }
    emit logLine(QStringLiteral("Server listening on port %1").arg(m_server.serverPort()));
    if (m_announce.state() != QAbstractSocket::BoundState)
        m_announce.bind(QHostAddress(QHostAddress::AnyIPv4), 0);
    m_announceTimer.start();
    announce();
    return true;
}

void Server::stop()
{
    m_announceTimer.stop();
    for (auto &p : m_pending) {
        if (p.socket) {
            p.socket->disconnect(this);
            p.socket->deleteLater();
        }
    }
    m_pending.clear();
    for (auto &c : m_clients) {
        if (c.socket) {
            c.socket->disconnect(this);
            c.socket->deleteLater();
        }
        c = ClientSlot{};
    }
    m_server.close();
    m_playing = false;
}

void Server::setServerName(const QString &name)
{
    const QString trimmed = name.trimmed();
    m_name = trimmed.isEmpty() ? QStringLiteral("Tetrinet") : trimmed.left(32);
}

void Server::setMaxPlayers(int maxPlayers)
{
    m_maxPlayers = std::clamp(maxPlayers, 1, kMaxPlayers);
}

bool Server::isListening() const
{
    return m_server.isListening();
}

quint16 Server::port() const
{
    return m_server.serverPort();
}

int Server::playerCount() const
{
    int n = 0;
    for (const auto &c : m_clients)
        if (c.used)
            ++n;
    return n;
}

void Server::startGame()
{
    if (m_playing || playerCount() < 1)
        return;
    m_playing = true;
    const int seed = static_cast<int>(QRandomGenerator::global()->generate() & 0x7fffffff);
    for (auto &c : m_clients) {
        c.alive = c.used;
        c.field.clear();
    }
    Message start;
    start.type = Message::Start;
    start.value = seed;
    broadcast(start);
    emit logLine(QStringLiteral("Game started"));
}

void Server::onNewConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket *sock = m_server.nextPendingConnection();
        connect(sock, &QTcpSocket::readyRead, this, &Server::onPendingRead);
        connect(sock, &QTcpSocket::disconnected, this, &Server::onPendingDisconnected);
        m_pending.push_back(Pending{sock, {}});
    }
}

void Server::sendStatus(QTcpSocket *socket)
{
    Message st;
    st.type = Message::Status;
    st.slot = playerCount();
    st.target = m_maxPlayers;
    st.value = m_playing ? 1 : 0;
    st.text = m_name;
    sendRaw(socket, st);
}

void Server::sendRaw(QTcpSocket *socket, const Message &msg)
{
    if (socket && socket->state() == QAbstractSocket::ConnectedState)
        socket->write(encodeMessage(msg));
}

int Server::findPending(QTcpSocket *socket) const
{
    for (int i = 0; i < m_pending.size(); ++i)
        if (m_pending[i].socket == socket)
            return i;
    return -1;
}

void Server::onPendingRead()
{
    auto *sock = qobject_cast<QTcpSocket *>(sender());
    const int idx = findPending(sock);
    if (idx < 0)
        return;
    auto &p = m_pending[idx];
    p.buffer += sock->readAll();
    int nl;
    while ((nl = p.buffer.indexOf('\n')) >= 0) {
        const QByteArray line = p.buffer.left(nl);
        p.buffer.remove(0, nl + 1);
        Message msg;
        if (!decodeMessage(line, msg))
            continue;
        if (msg.type == Message::Query) {
            sendStatus(sock);
            sock->disconnectFromHost();
            return;
        }
        if (msg.type == Message::Nick) {
            const QString nick = msg.text;
            const QByteArray leftover = p.buffer;
            sock->disconnect(this);
            m_pending.removeAt(idx);
            if (!promotePending(sock, nick)) {
                sock->disconnectFromHost();
                sock->deleteLater();
                return;
            }
            if (!leftover.isEmpty()) {
                const int slot = findSlot(sock);
                if (slot >= 0) {
                    m_clients[static_cast<size_t>(slot)].buffer = leftover;
                    processClient(sock);
                }
            }
            return;
        }
    }
}

void Server::onPendingDisconnected()
{
    auto *sock = qobject_cast<QTcpSocket *>(sender());
    const int idx = findPending(sock);
    if (idx >= 0)
        m_pending.removeAt(idx);
    sock->deleteLater();
}

bool Server::promotePending(QTcpSocket *sock, const QString &nick)
{
    if (!sock || m_playing || playerCount() >= m_maxPlayers)
        return false;
    const int slot = allocateSlot();
    if (slot < 0)
        return false;

    auto &c = m_clients[static_cast<size_t>(slot)];
    c.socket = sock;
    c.used = true;
    c.name = nick.isEmpty() ? QStringLiteral("Player%1").arg(slot + 1) : nick;
    c.alive = false;
    connect(sock, &QTcpSocket::readyRead, this, &Server::onReadyRead);
    connect(sock, &QTcpSocket::disconnected, this, &Server::onDisconnected);

    Message welcome;
    welcome.type = Message::Welcome;
    welcome.slot = slot;
    sendTo(slot, welcome);
    sendPlayerList(slot);

    Message joined;
    joined.type = Message::Player;
    joined.slot = slot;
    joined.text = c.name;
    broadcast(joined, slot);
    emit playerListChanged();
    emit logLine(QStringLiteral("%1 joined slot %2").arg(c.name).arg(slot + 1));
    return true;
}

void Server::announce()
{
    const QByteArray payload =
        QStringLiteral("TNET %1 %2 %3 %4 %5\n")
            .arg(m_server.serverPort())
            .arg(playerCount())
            .arg(m_maxPlayers)
            .arg(m_playing ? 1 : 0)
            .arg(m_name)
            .toUtf8();
    m_announce.writeDatagram(payload, QHostAddress::Broadcast, kAnnouncePort);
}

int Server::allocateSlot() const
{
    for (int i = 0; i < m_maxPlayers; ++i)
        if (!m_clients[static_cast<size_t>(i)].used)
            return i;
    return -1;
}

int Server::findSlot(QTcpSocket *socket) const
{
    for (int i = 0; i < kMaxPlayers; ++i)
        if (m_clients[static_cast<size_t>(i)].socket == socket)
            return i;
    return -1;
}

void Server::onReadyRead()
{
    processClient(qobject_cast<QTcpSocket *>(sender()));
}

void Server::processClient(QTcpSocket *sock)
{
    const int slot = findSlot(sock);
    if (slot < 0)
        return;
    auto &c = m_clients[static_cast<size_t>(slot)];
    c.buffer += sock->readAll();
    int idx;
    while ((idx = c.buffer.indexOf('\n')) >= 0) {
        const QByteArray line = c.buffer.left(idx);
        c.buffer.remove(0, idx + 1);
        Message msg;
        if (decodeMessage(line, msg))
            handle(slot, msg);
    }
}

void Server::onDisconnected()
{
    auto *sock = qobject_cast<QTcpSocket *>(sender());
    const int slot = findSlot(sock);
    if (slot < 0)
        return;
    m_clients[static_cast<size_t>(slot)] = ClientSlot{};
    Message left;
    left.type = Message::Left;
    left.slot = slot;
    broadcast(left);
    emit playerListChanged();
    emit logLine(QStringLiteral("Slot %1 left").arg(slot + 1));
    if (m_playing)
        checkWin();
    sock->deleteLater();
}

void Server::handle(int slot, const Message &msg)
{
    auto &c = m_clients[static_cast<size_t>(slot)];
    switch (msg.type) {
    case Message::Nick:
        c.name = msg.text.isEmpty() ? QStringLiteral("Player%1").arg(slot + 1) : msg.text;
        {
            Message p;
            p.type = Message::Player;
            p.slot = slot;
            p.text = c.name;
            broadcast(p);
        }
        emit playerListChanged();
        break;
    case Message::Chat: {
        Message chat;
        chat.type = Message::Chat;
        chat.slot = slot;
        chat.text = msg.text;
        broadcast(chat);
        break;
    }
    case Message::Field:
        if (m_playing && c.alive) {
            c.field = msg.data;
            Message field = msg;
            field.slot = slot;
            broadcast(field, slot);
        }
        break;
    case Message::Special:
        if (m_playing && c.alive && msg.target >= 0 && msg.target < kMaxPlayers
            && m_clients[static_cast<size_t>(msg.target)].used) {
            Message spec = msg;
            spec.slot = slot;
            broadcast(spec);
        }
        break;
    case Message::Lose:
        if (m_playing && c.alive) {
            c.alive = false;
            Message lose;
            lose.type = Message::Lose;
            lose.slot = slot;
            broadcast(lose);
            checkWin();
        }
        break;
    case Message::Start:
        startGame();
        break;
    case Message::Ping: {
        Message pong;
        pong.type = Message::Pong;
        sendTo(slot, pong);
        break;
    }
    default:
        break;
    }
}

void Server::sendTo(int slot, const Message &msg)
{
    auto &c = m_clients[static_cast<size_t>(slot)];
    if (c.socket && c.socket->state() == QAbstractSocket::ConnectedState)
        c.socket->write(encodeMessage(msg));
}

void Server::broadcast(const Message &msg, int except)
{
    for (int i = 0; i < kMaxPlayers; ++i)
        if (i != except && m_clients[static_cast<size_t>(i)].used)
            sendTo(i, msg);
}

void Server::sendPlayerList(int slot)
{
    for (int i = 0; i < kMaxPlayers; ++i) {
        if (!m_clients[static_cast<size_t>(i)].used)
            continue;
        Message p;
        p.type = Message::Player;
        p.slot = i;
        p.text = m_clients[static_cast<size_t>(i)].name;
        sendTo(slot, p);
    }
}

void Server::checkWin()
{
    int alive = 0;
    int winner = -1;
    for (int i = 0; i < kMaxPlayers; ++i) {
        if (m_clients[static_cast<size_t>(i)].used && m_clients[static_cast<size_t>(i)].alive) {
            ++alive;
            winner = i;
        }
    }
    if (alive <= 1 && playerCount() > 0) {
        if (winner >= 0) {
            Message win;
            win.type = Message::Win;
            win.slot = winner;
            broadcast(win);
        }
        m_playing = false;
    }
}

} // namespace tnet
