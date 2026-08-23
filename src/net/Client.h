#pragma once

#include "Protocol.h"
#include "game/Types.h"

#include <QObject>
#include <QTcpSocket>
#include <array>

namespace tnet {

// Native-protocol client used when hosting or joining our own server.
class Client : public QObject {
    Q_OBJECT
public:
    explicit Client(QObject *parent = nullptr);
    ~Client() override;

    void connectTo(const QString &host, quint16 port, const QString &nick);
    void disconnectFromHost();
    bool isConnected() const;

    void sendChat(const QString &text);
    void sendField(const QString &data);
    void sendInput(const QString &action, int target = -1);
    void sendLose();
    void sendNick(const QString &nick);

    int localSlot() const { return m_slot; }
    QString playerName(int slot) const;
    bool slotUsed(int slot) const;

signals:
    void connected();
    void disconnected();
    void errorText(const QString &text);
    void chatReceived(int slot, const QString &text);
    void playerUpdated(int slot, const QString &name);
    void playerLeft(int slot);
    void gameStarted(int seed);
    void fieldReceived(int slot, const QString &data);
    void specialReceived(int from, int target, Special special);
    void playerLost(int slot);
    void playerWon(int slot);
    void stateReceived(int slot, bool alive, int level, int score, int lines,
                       const QString &inventory, const QString &field, const QString &piece);
    void welcomed(int slot);

private:
    void onConnected();
    void onReadyRead();
    void send(const Message &msg);
    void handle(const Message &msg);

    QTcpSocket m_socket;
    QByteArray m_buffer;
    QString m_nick;
    int m_slot = -1;
    std::array<QString, kMaxPlayers> m_names{};
    std::array<bool, kMaxPlayers> m_used{};
};

} // namespace tnet
