#include "StatusQuery.h"

#include "Protocol.h"

namespace tnet {

StatusQuery::StatusQuery(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, [this]() { finish(false, {}, 0, 0, false); });
    connect(&m_socket, &QTcpSocket::connected, this, [this]() {
        Message q;
        q.type = Message::Query;
        m_socket.write(encodeMessage(q));
    });
    connect(&m_socket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        finish(false, {}, 0, 0, false);
    });
    connect(&m_socket, &QTcpSocket::readyRead, this, [this]() {
        m_buffer += m_socket.readAll();
        int idx;
        while ((idx = m_buffer.indexOf('\n')) >= 0) {
            if (idx >= kMaxNativeFrameSize) {
                finish(false, {}, 0, 0, false);
                return;
            }
            const QByteArray line = m_buffer.left(idx);
            m_buffer.remove(0, idx + 1);
            Message msg;
            if (!decodeMessage(line, msg) || msg.type != Message::Status)
                continue;
            finish(true, msg.text.isEmpty() ? QStringLiteral("Tetrivibes") : msg.text, msg.slot,
                   msg.target, msg.value != 0);
            return;
        }
        if (m_buffer.size() >= kMaxNativeFrameSize)
            finish(false, {}, 0, 0, false);
    });
}

void StatusQuery::start(const QString &host, quint16 port)
{
    m_done = false;
    m_host = host;
    m_port = port;
    m_buffer.clear();
    m_socket.abort();
    m_timer.start(1500);
    m_socket.connectToHost(host, port);
}

void StatusQuery::finish(bool ok, const QString &name, int players, int maxPlayers, bool playing)
{
    if (m_done)
        return;
    m_done = true;
    m_timer.stop();
    m_socket.abort();
    emit finished(m_host, m_port, ok, name, players, maxPlayers, playing);
}

} // namespace tnet
