#include "Client.h"

namespace tnet {

Client::Client(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket, &QTcpSocket::connected, this, &Client::onConnected);
    connect(&m_socket, &QTcpSocket::disconnected, this, [this]() {
        m_slot = -1;
        m_used.fill(false);
        emit disconnected();
    });
    connect(&m_socket, &QTcpSocket::readyRead, this, &Client::onReadyRead);
    connect(&m_socket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit errorText(m_socket.errorString());
    });
}

Client::~Client()
{
    m_socket.disconnect(this);
    m_socket.abort();
}

void Client::connectTo(const QString &host, quint16 port, const QString &nick)
{
    m_nick = nick;
    m_slot = -1;
    m_used.fill(false);
    m_names.fill({});
    m_buffer.clear();
    m_socket.abort();
    m_socket.connectToHost(host, port);
}

void Client::disconnectFromHost()
{
    m_socket.disconnectFromHost();
}

bool Client::isConnected() const
{
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

void Client::onConnected()
{
    sendNick(m_nick);
    emit connected();
}

void Client::sendNick(const QString &nick)
{
    Message m;
    m.type = Message::Nick;
    m.text = nick;
    send(m);
}

void Client::sendChat(const QString &text)
{
    Message m;
    m.type = Message::Chat;
    m.text = text;
    send(m);
}

void Client::sendField(const QString &data)
{
    Message m;
    m.type = Message::Field;
    m.data = data;
    send(m);
}

void Client::sendSpecial(int target, Special special)
{
    Message m;
    m.type = Message::Special;
    m.target = target;
    m.text = QString(QChar(specialLetter(special)));
    send(m);
}

void Client::sendLose()
{
    Message m;
    m.type = Message::Lose;
    send(m);
}

void Client::send(const Message &msg)
{
    if (isConnected())
        m_socket.write(encodeMessage(msg));
}

void Client::onReadyRead()
{
    m_buffer += m_socket.readAll();
    int idx;
    while ((idx = m_buffer.indexOf('\n')) >= 0) {
        if (idx >= kMaxNativeFrameSize) {
            emit errorText(QStringLiteral("Received oversized server frame."));
            m_socket.disconnectFromHost();
            return;
        }
        const QByteArray line = m_buffer.left(idx);
        m_buffer.remove(0, idx + 1);
        Message msg;
        if (decodeMessage(line, msg))
            handle(msg);
    }
    if (m_buffer.size() >= kMaxNativeFrameSize) {
        emit errorText(QStringLiteral("Received oversized server frame."));
        m_socket.disconnectFromHost();
    }
}

void Client::handle(const Message &msg)
{
    switch (msg.type) {
    case Message::Welcome:
        m_slot = msg.slot;
        emit welcomed(m_slot);
        break;
    case Message::Player:
        if (msg.slot >= 0 && msg.slot < kMaxPlayers) {
            m_used[static_cast<size_t>(msg.slot)] = true;
            m_names[static_cast<size_t>(msg.slot)] = msg.text;
            emit playerUpdated(msg.slot, msg.text);
        }
        break;
    case Message::Left:
        if (msg.slot >= 0 && msg.slot < kMaxPlayers) {
            m_used[static_cast<size_t>(msg.slot)] = false;
            m_names[static_cast<size_t>(msg.slot)].clear();
            emit playerLeft(msg.slot);
        }
        break;
    case Message::Chat:
        emit chatReceived(msg.slot, msg.text);
        break;
    case Message::Start:
        emit gameStarted(msg.value);
        break;
    case Message::Field:
        emit fieldReceived(msg.slot, msg.data);
        break;
    case Message::Special: {
        const Cell cell = charToCell(msg.text.isEmpty() ? 'a' : msg.text[0].toLatin1());
        if (isSpecial(cell))
            emit specialReceived(msg.slot, msg.target, cellToSpecial(cell));
        break;
    }
    case Message::Lose:
        emit playerLost(msg.slot);
        break;
    case Message::Win:
        emit playerWon(msg.slot);
        break;
    default:
        break;
    }
}

QString Client::playerName(int slot) const
{
    if (slot < 0 || slot >= kMaxPlayers)
        return {};
    return m_names[static_cast<size_t>(slot)];
}

bool Client::slotUsed(int slot) const
{
    if (slot < 0 || slot >= kMaxPlayers)
        return false;
    return m_used[static_cast<size_t>(slot)];
}

} // namespace tnet
