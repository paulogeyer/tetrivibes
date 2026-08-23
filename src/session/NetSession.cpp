#include "NetSession.h"

#include <algorithm>

namespace tnet {
namespace {

QVector<Special> decodeInventory(const QString &encoded)
{
    QVector<Special> inventory;
    if (encoded == QLatin1String("-"))
        return inventory;
    for (const QChar ch : encoded) {
        const Cell cell = charToCell(ch.toLatin1());
        if (!isSpecial(cell))
            return {};
        inventory.push_back(cellToSpecial(cell));
    }
    return inventory;
}

} // namespace

NetSession::NetSession(bool host, const QString &hostName, quint16 port, const QString &nick,
                       const QString &serverName, int maxPlayers, int botCount, QObject *parent)
    : GameSession(parent)
    , m_host(host)
    , m_nick(nick.isEmpty() ? QStringLiteral("Player") : nick)
    , m_serverName(serverName)
    , m_maxPlayers(maxPlayers)
    , m_botCount(botCount)
{
    m_pendingHost = hostName;
    m_pendingPort = port;
    connect(&m_engine, &Engine::updated, this, [this]() {
        emit updated();
    });
    connect(&m_engine, &Engine::inventoryChanged, this, &GameSession::updated);
    connect(&m_engine, &Engine::died, this, [this]() {
        emit updated();
    });

    connect(&m_client, &Client::welcomed, this, [this](int) {
        emit chatReceived(QStringLiteral("* Joined server"));
        emit statusChanged();
        emit updated();
    });
    connect(&m_client, &Client::disconnected, this, [this]() {
        emit chatReceived(QStringLiteral("* Disconnected"));
        emit statusChanged();
    });
    connect(&m_client, &Client::errorText, this, [this](const QString &e) {
        emit chatReceived(QStringLiteral("* Error: %1").arg(e));
    });
    connect(&m_client, &Client::chatReceived, this, [this](int slot, const QString &text) {
        emit chatReceived(QStringLiteral("%1: %2").arg(playerName(slot), text));
    });
    connect(&m_client, &Client::playerUpdated, this, [this](int, const QString &) {
        emit updated();
        emit statusChanged();
    });
    connect(&m_client, &Client::playerLeft, this, [this](int slot) {
        m_alive[static_cast<size_t>(slot)] = false;
        emit chatReceived(QStringLiteral("* %1 left").arg(QStringLiteral("P%1").arg(slot + 1)));
        emit updated();
    });
    connect(&m_client, &Client::gameStarted, this, &NetSession::onGameStarted);
    connect(&m_client, &Client::fieldReceived, this, [this](int slot, const QString &data) {
        if (slot >= 0 && slot < kMaxPlayers && Field::isValidEncoding(data))
            m_fields[static_cast<size_t>(slot)] = Field::decode(data);
        emit updated();
    });
    connect(&m_client, &Client::specialReceived, this, &NetSession::onSpecial);
    connect(&m_client, &Client::stateReceived, this, &NetSession::onState);
    connect(&m_client, &Client::playerLost, this, [this](int slot) {
        if (slot >= 0 && slot < kMaxPlayers)
            m_alive[static_cast<size_t>(slot)] = false;
        emit chatReceived(QStringLiteral("* %1 topped out").arg(playerName(slot)));
        emit updated();
    });
    connect(&m_client, &Client::playerWon, this, [this](int slot) {
        m_playing = false;
        emit gameEnded(QStringLiteral("%1 wins!").arg(playerName(slot)));
        emit statusChanged();
    });

}

void NetSession::attachServer(Server *server)
{
    m_server = server;
    m_ownsServer = false;
}

void NetSession::begin()
{
    if (m_host) {
        if (!m_server) {
            m_server = new Server(this);
            m_ownsServer = true;
            m_server->setServerName(m_serverName.isEmpty() ? m_nick : m_serverName);
            m_server->setMaxPlayers(m_maxPlayers);
            m_server->setBotCount(m_botCount);
            if (!m_server->listen(m_pendingPort))
                emit chatReceived(QStringLiteral("* Failed to start server"));
        }
        connect(m_server, &Server::logLine, this, &GameSession::chatReceived);
        if (m_server->isListening())
            emit chatReceived(QStringLiteral("* Hosting on port %1").arg(m_server->port()));
        m_client.connectTo(QStringLiteral("127.0.0.1"),
                           m_server->isListening() ? m_server->port() : m_pendingPort, m_nick);
    } else {
        emit chatReceived(QStringLiteral("* Connecting to %1")
                              .arg(m_serverName.isEmpty() ? m_pendingHost : m_serverName));
        m_client.connectTo(m_pendingHost, m_pendingPort, m_nick);
    }
}

NetSession::~NetSession()
{
    QObject::disconnect(&m_client, nullptr, this, nullptr);
    if (m_server)
        QObject::disconnect(m_server, nullptr, this, nullptr);
    if (m_ownsServer && m_server)
        m_server->stop();
    m_client.disconnectFromHost();
}

bool NetSession::serverOk() const
{
    return !m_host || (m_server && m_server->isListening());
}

Engine *NetSession::localEngine()
{
    return &m_engine;
}

int NetSession::localSlot() const
{
    return m_client.localSlot() < 0 ? 0 : m_client.localSlot();
}

Field NetSession::opponentField(int slot) const
{
    if (slot == localSlot())
        return m_engine.snapshot(true);
    if (slot < 0 || slot >= kMaxPlayers)
        return {};
    return m_fields[static_cast<size_t>(slot)];
}

QString NetSession::playerName(int slot) const
{
    const QString n = m_client.playerName(slot);
    if (!n.isEmpty())
        return n;
    if (slotOccupied(slot))
        return QStringLiteral("P%1").arg(slot + 1);
    return {};
}

bool NetSession::slotOccupied(int slot) const
{
    return m_client.slotUsed(slot);
}

bool NetSession::slotAlive(int slot) const
{
    return slotOccupied(slot) && m_alive[static_cast<size_t>(slot)];
}

bool NetSession::canStart() const
{
    return m_host && !m_playing && m_client.isConnected();
}

QString NetSession::statusText() const
{
    if (!m_client.isConnected())
        return QStringLiteral("Connecting to %1…")
            .arg(m_serverName.isEmpty() ? m_pendingHost : m_serverName);
    if (!m_playing)
        return m_host ? QStringLiteral("Host lobby — press Start when ready")
                      : QStringLiteral("Waiting for host to start");
    return QStringLiteral("L%1  %2 pts  %3 lines")
        .arg(m_engine.level())
        .arg(m_engine.score())
        .arg(m_engine.lines());
}

void NetSession::startGame()
{
    if (m_host && m_server)
        m_server->startGame();
}

void NetSession::onGameStarted(int seed)
{
    Q_UNUSED(seed);
    m_playing = true;
    m_alive.fill(false);
    for (int i = 0; i < kMaxPlayers; ++i) {
        m_fields[static_cast<size_t>(i)] = Field{};
        if (m_client.slotUsed(i))
            m_alive[static_cast<size_t>(i)] = true;
    }
    emit chatReceived(QStringLiteral("* Game started"));
    emit statusChanged();
    emit updated();
}

void NetSession::useSpecial(int targetSlot)
{
    if (!m_playing || !m_engine.alive() || m_engine.inventory().isEmpty())
        return;
    if (!slotOccupied(targetSlot))
        return;
    m_client.sendInput(QStringLiteral("special"), targetSlot);
}

void NetSession::onSpecial(int from, int target, Special special)
{
    emit chatReceived(QStringLiteral("* %1 used %2 on %3")
                          .arg(playerName(from), specialName(special), playerName(target)));

}

void NetSession::sendChat(const QString &text)
{
    m_client.sendChat(text);
}

void NetSession::onState(int slot, bool alive, int level, int score, int lines,
                         const QString &inventory, const QString &field, const QString &piece)
{
    if (slot < 0 || slot >= kMaxPlayers || !Field::isValidEncoding(field))
        return;
    bool hasPiece = false;
    Piece current;
    Piece next;
    if (!decodeLivePieces(piece, hasPiece, current, next))
        return;
    m_alive[static_cast<size_t>(slot)] = alive;
    Field decoded = Field::decode(field);
    if (slot == localSlot()) {
        m_engine.setRemoteState(decoded, alive, level, score, lines, decodeInventory(inventory),
                                hasPiece, current, next);
    } else {
        if (hasPiece)
            decoded.lock(current);
        m_fields[static_cast<size_t>(slot)] = decoded;
    }
    emit statusChanged();
    emit updated();
}

void NetSession::tick(int ms)
{
    Q_UNUSED(ms);
    emit statusChanged();
}

void NetSession::moveLeft()
{
    m_client.sendInput(QStringLiteral("left"));
}

void NetSession::moveRight()
{
    m_client.sendInput(QStringLiteral("right"));
}

void NetSession::rotate(int direction)
{
    m_client.sendInput(direction < 0 ? QStringLiteral("ccw") : QStringLiteral("cw"));
}

void NetSession::softDrop()
{
    m_client.sendInput(QStringLiteral("down"));
}

void NetSession::hardDrop()
{
    m_client.sendInput(QStringLiteral("drop"));
}

} // namespace tnet
