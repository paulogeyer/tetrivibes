#include "Server.h"

#include "game/Field.h"
#include "util/Scoreboard.h"

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
    connect(&m_gameTimer, &QTimer::timeout, this, &Server::tickGame);
    m_gameTimer.setInterval(16);
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
    addBots();
    if (m_announce.state() != QAbstractSocket::BoundState)
        m_announce.bind(QHostAddress(QHostAddress::AnyIPv4), 0);
    m_announceTimer.start();
    announce();
    return true;
}

void Server::stop()
{
    m_announceTimer.stop();
    m_gameTimer.stop();
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
    m_botAcc = 0;
}

void Server::setServerName(const QString &name)
{
    const QString trimmed = name.trimmed();
    m_name = trimmed.isEmpty() ? QStringLiteral("Tetrivibes") : trimmed.left(32);
}

void Server::setMaxPlayers(int maxPlayers)
{
    m_maxPlayers = std::clamp(maxPlayers, 1, kMaxPlayers);
}

void Server::setBotCount(int botCount)
{
    m_botCount = std::clamp(botCount, 0, kMaxPlayers - 1);
}

bool Server::setInvadersMode(bool enabled)
{
    if (m_playing)
        return false;
    m_invadersArmed = enabled;
    emit logLine(enabled ? QStringLiteral("Signal acquired: native field channel rerouted")
                         : QStringLiteral("Native field channel restored"));
    return true;
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

int Server::humanCount() const
{
    int n = 0;
    for (const auto &c : m_clients) {
        if (c.used && !c.bot)
            ++n;
    }
    return n;
}

void Server::returnToLobby()
{
    m_playing = false;
    m_gameTimer.stop();
    m_botAcc = 0;
    for (auto &c : m_clients) {
        c.alive = false;
        c.engine.reset();
        c.invaders.reset();
        c.botAi.reset();
        c.inputAction.clear();
        c.inputTarget = -1;
    }
}

void Server::startGame()
{
    if (m_playing || playerCount() < 1)
        return;
    m_playing = true;
    m_invadersMode = m_invadersArmed;
    const int seed = static_cast<int>(QRandomGenerator::global()->generate() & 0x7fffffff);
    for (int i = 0; i < kMaxPlayers; ++i) {
        auto &c = m_clients[static_cast<size_t>(i)];
        c.alive = c.used;
        c.botAi.reset();
        c.engine.reset();
        c.invaders.reset();
        c.inputAction.clear();
        c.inputTarget = -1;
        if (c.used) {
            const uint32_t playerSeed = static_cast<uint32_t>(seed)
                + static_cast<uint32_t>(i) * 7919u;
            if (m_invadersMode) {
                c.invaders = std::make_unique<InvadersEngine>();
                c.invaders->reset(playerSeed);
            } else {
                c.engine = std::make_unique<Engine>();
                c.engine->reset(playerSeed);
                if (c.bot)
                    c.botAi = std::make_unique<Bot>(c.engine.get());
            }
        }
    }
    Message start;
    start.type = Message::Start;
    start.value = m_invadersMode ? kInvadersStartMarker : seed;
    broadcast(start);
    for (int i = 0; i < kMaxPlayers; ++i)
        if (m_clients[static_cast<size_t>(i)].used)
            broadcastState(i);
    m_gameTimer.start();
    emit logLine(m_invadersMode ? QStringLiteral("Game started (signal 1978)")
                                : QStringLiteral("Game started"));
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
        if (nl >= kMaxNativeFrameSize) {
            emit logLine(QStringLiteral("Rejected oversized pending request"));
            sock->disconnectFromHost();
            return;
        }
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
    if (p.buffer.size() >= kMaxNativeFrameSize) {
        emit logLine(QStringLiteral("Rejected oversized pending request"));
        sock->disconnectFromHost();
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
    if (!sock || playerCount() >= m_maxPlayers)
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
    if (m_playing) {
        c.alive = true;
        if (m_invadersMode) {
            c.invaders = std::make_unique<InvadersEngine>();
            c.invaders->reset(QRandomGenerator::global()->generate());
        } else {
            c.engine = std::make_unique<Engine>();
            c.engine->reset(QRandomGenerator::global()->generate());
        }
        Message start;
        start.type = Message::Start;
        start.value = m_invadersMode ? kInvadersStartMarker : 0;
        sendTo(slot, start);
        for (int i = 0; i < kMaxPlayers; ++i) {
            if (m_clients[static_cast<size_t>(i)].used)
                broadcastState(i);
        }
    }
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
        if (idx >= kMaxNativeFrameSize) {
            emit logLine(QStringLiteral("Rejected oversized frame from slot %1").arg(slot + 1));
            sock->disconnectFromHost();
            return;
        }
        const QByteArray line = c.buffer.left(idx);
        c.buffer.remove(0, idx + 1);
        Message msg;
        if (decodeMessage(line, msg))
            handle(slot, msg);
    }
    if (c.buffer.size() >= kMaxNativeFrameSize) {
        emit logLine(QStringLiteral("Rejected oversized frame from slot %1").arg(slot + 1));
        sock->disconnectFromHost();
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
    if (humanCount() == 0)
        returnToLobby();
    else if (m_playing)
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
        break;
    case Message::Special:
        break;
    case Message::Lose:
        break;
    case Message::Start:
        // Only an embedded host may call startGame(); peer commands cannot start matches.
        break;
    case Message::Ping: {
        Message pong;
        pong.type = Message::Pong;
        sendTo(slot, pong);
        break;
    }
    case Message::Input:
        if (!m_playing || !c.alive || (!c.engine && !c.invaders))
            break;
        if (c.inputAction.isEmpty()) {
            c.inputAction = msg.text;
            c.inputTarget = msg.target;
        }
        break;
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
            addWin(m_clients[static_cast<size_t>(winner)].name);
        }
        m_playing = false;
        m_gameTimer.stop();
    }
}

void Server::addBots()
{
    const int want = std::clamp(m_botCount, 0, std::max(0, m_maxPlayers - 1));
    int have = 0;
    for (const auto &c : m_clients) {
        if (c.bot)
            ++have;
    }
    while (have < want) {
        const int slot = allocateSlot();
        if (slot < 0)
            break;
        auto &c = m_clients[static_cast<size_t>(slot)];
        c.used = true;
        c.bot = true;
        c.name = QStringLiteral("Bot %1").arg(++have);
        Message joined;
        joined.type = Message::Player;
        joined.slot = slot;
        joined.text = c.name;
        broadcast(joined);
        emit playerListChanged();
        emit logLine(QStringLiteral("%1 joined slot %2").arg(c.name).arg(slot + 1));
    }
}

QVector<int> Server::heights() const
{
    QVector<int> h(kMaxPlayers, -1);
    for (int i = 0; i < kMaxPlayers; ++i) {
        const auto &c = m_clients[static_cast<size_t>(i)];
        if (c.used && c.alive && c.engine)
            h[i] = c.engine->field().stackHeight();
    }
    return h;
}

void Server::tickBots()
{
    m_botAcc += m_gameTimer.interval();
    if (m_botAcc < 90)
        return;
    m_botAcc = 0;
    const QVector<int> stack = heights();
    for (int i = 0; i < kMaxPlayers; ++i) {
        auto &c = m_clients[static_cast<size_t>(i)];
        if (m_invadersMode) {
            if (c.bot && c.alive && c.invaders) {
                c.invaders->autoplay();
                if (!c.invaders->inventory().isEmpty()
                    && QRandomGenerator::global()->bounded(100) < 18) {
                    const Special special = c.invaders->inventory().front();
                    const bool helpful = special == Special::ClearLine
                        || special == Special::ClearSpecial || special == Special::Gravity
                        || special == Special::Nuke;
                    int target = i;
                    if (!helpful) {
                        for (int candidate = 0; candidate < kMaxPlayers; ++candidate) {
                            if (candidate != i
                                && m_clients[static_cast<size_t>(candidate)].alive) {
                                target = candidate;
                                break;
                            }
                        }
                    }
                    applySpecial(i, target);
                }
            }
            continue;
        }
        if (!c.bot || !c.alive || !c.botAi || !c.engine)
            continue;
        c.botAi->thinkAndAct();
        if (!c.engine->inventory().isEmpty() && QRandomGenerator::global()->bounded(100) < 18) {
            const int target = c.botAi->chooseTarget(stack, i);
            const Special s = c.engine->inventory().front();
            const bool helpful = (s == Special::ClearLine || s == Special::Gravity
                                  || s == Special::Nuke || s == Special::ClearSpecial);
            applySpecial(i, helpful ? i : target);
        }
    }
}

void Server::tickGame()
{
    if (!m_playing)
        return;
    tickBots();
    for (int i = 0; i < kMaxPlayers; ++i) {
        auto &c = m_clients[static_cast<size_t>(i)];
        if (!c.alive || (!c.engine && !c.invaders))
            continue;
        if (!c.inputAction.isEmpty()) {
            applyInput(i, c.inputAction, c.inputTarget);
            c.inputAction.clear();
            c.inputTarget = -1;
        }
        if (m_invadersMode)
            c.invaders->tick(m_gameTimer.interval());
        else
            c.engine->tick(m_gameTimer.interval());
        const bool stillAlive = m_invadersMode ? c.invaders->alive() : c.engine->alive();
        if (!stillAlive) {
            c.alive = false;
            Message lose;
            lose.type = Message::Lose;
            lose.slot = i;
            broadcast(lose);
            checkWin();
        }
        broadcastState(i);
    }
}

void Server::applyInput(int slot, const QString &action, int target)
{
    auto &c = m_clients[static_cast<size_t>(slot)];
    if (m_invadersMode) {
        if (!c.invaders)
            return;
        if (action == QLatin1String("left"))
            c.invaders->moveLeft();
        else if (action == QLatin1String("right"))
            c.invaders->moveRight();
        else if (action == QLatin1String("drop") || action == QLatin1String("down"))
            c.invaders->fire();
        else if (action == QLatin1String("special") && target >= 0 && target < kMaxPlayers
                 && m_clients[static_cast<size_t>(target)].alive)
            applySpecial(slot, target);
        return;
    }
    if (!c.engine)
        return;
    if (action == QLatin1String("left"))
        c.engine->moveLeft();
    else if (action == QLatin1String("right"))
        c.engine->moveRight();
    else if (action == QLatin1String("down"))
        c.engine->softDrop();
    else if (action == QLatin1String("drop"))
        c.engine->hardDrop();
    else if (action == QLatin1String("cw"))
        c.engine->rotate(1);
    else if (action == QLatin1String("ccw"))
        c.engine->rotate(-1);
    else if (action == QLatin1String("special") && target >= 0 && target < kMaxPlayers
             && m_clients[static_cast<size_t>(target)].alive)
        applySpecial(slot, target);
}

void Server::broadcastState(int slot)
{
    const auto &c = m_clients[static_cast<size_t>(slot)];
    if (!c.used || (!c.engine && !c.invaders))
        return;
    Message state;
    state.type = Message::State;
    state.slot = slot;
    state.value = c.alive ? 1 : 0;
    if (m_invadersMode) {
        state.level = c.invaders->wave();
        state.score = c.invaders->score();
        state.lines = c.invaders->lives();
        for (Special special : c.invaders->inventory())
            state.text += QChar(specialLetter(special));
        if (state.text.isEmpty())
            state.text = QStringLiteral("-");
        state.data = c.invaders->field().encode();
        state.piece = encodeLivePieces(false, Piece{}, Piece{});
        broadcast(state);
        return;
    }
    state.level = c.engine->level();
    state.score = c.engine->score();
    state.lines = c.engine->lines();
    for (Special special : c.engine->inventory())
        state.text += QChar(specialLetter(special));
    if (state.text.isEmpty())
        state.text = QStringLiteral("-");
    state.data = c.engine->snapshot(false).encode();
    state.piece = encodeLivePieces(c.engine->hasPiece(), c.engine->current(), c.engine->next());
    broadcast(state);
}

void Server::applySpecial(int from, int target)
{
    auto &source = m_clients[static_cast<size_t>(from)];
    auto &destination = m_clients[static_cast<size_t>(target)];
    if (m_invadersMode) {
        if (!source.invaders || !destination.invaders || source.invaders->inventory().isEmpty())
            return;
        const Special special = source.invaders->takePowerup();
        if (special == Special::SwitchField && from != target)
            source.invaders->swapBattlefield(*destination.invaders);
        else
            destination.invaders->applyPowerup(special);

        Message event;
        event.type = Message::Special;
        event.slot = from;
        event.target = target;
        event.text = QString(QChar(specialLetter(special)));
        broadcast(event);
        broadcastState(from);
        if (target != from)
            broadcastState(target);
        return;
    }
    if (!source.engine || !destination.engine || source.engine->inventory().isEmpty())
        return;
    const Special special = source.engine->takeSpecial();
    if (special == Special::SwitchField && from != target) {
        const Field sourceField = source.engine->field();
        source.engine->setField(destination.engine->field());
        destination.engine->setField(sourceField);
    } else {
        destination.engine->applySpecial(special);
    }
    Message event;
    event.type = Message::Special;
    event.slot = from;
    event.target = target;
    event.text = QString(QChar(specialLetter(special)));
    broadcast(event);
    broadcastState(from);
    if (target != from)
        broadcastState(target);
}

} // namespace tnet
