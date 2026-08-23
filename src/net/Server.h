#pragma once

#include "Protocol.h"
#include "game/Types.h"

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>
#include <QVector>
#include <array>

namespace tnet {

// Built-in native host: QUERY/STATUS for the browser, then NICK to take a seat.
class Server : public QObject {
    Q_OBJECT
public:
    explicit Server(QObject *parent = nullptr);
    ~Server() override;

    bool listen(quint16 port);
    void stop();
    bool isListening() const;
    quint16 port() const;
    void startGame();
    int playerCount() const;
    void setServerName(const QString &name);
    void setMaxPlayers(int maxPlayers);
    QString serverName() const { return m_name; }
    int maxPlayers() const { return m_maxPlayers; }

signals:
    void logLine(const QString &text);
    void playerListChanged();

private:
    struct ClientSlot {
        QTcpSocket *socket = nullptr;
        QByteArray buffer;
        QString name;
        bool used = false;
        bool alive = false;
        QString field;
    };

    struct Pending {
        QTcpSocket *socket = nullptr;
        QByteArray buffer;
    };

    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void onPendingRead();
    void onPendingDisconnected();
    void handle(int slot, const Message &msg);
    void sendTo(int slot, const Message &msg);
    void sendRaw(QTcpSocket *socket, const Message &msg);
    void broadcast(const Message &msg, int except = -1);
    void sendPlayerList(int slot);
    void sendStatus(QTcpSocket *socket);
    void announce();
    bool promotePending(QTcpSocket *socket, const QString &nick);
    void processClient(QTcpSocket *socket);
    void checkWin();
    int findSlot(QTcpSocket *socket) const;
    int findPending(QTcpSocket *socket) const;
    int allocateSlot() const;

    QTcpServer m_server;
    QUdpSocket m_announce;
    QTimer m_announceTimer;
    std::array<ClientSlot, kMaxPlayers> m_clients{};
    QVector<Pending> m_pending;
    QString m_name = QStringLiteral("Tetrinet");
    int m_maxPlayers = kMaxPlayers;
    bool m_playing = false;
};

} // namespace tnet
