#pragma once

#include "ClassicProtocol.h"
#include "game/Field.h"
#include "game/Types.h"

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <array>

namespace tnet {

// TCP client for TetriNET 1.13 / TetriFast. First packet is the XOR login.
class ClassicClient : public QObject {
    Q_OBJECT
public:
    explicit ClassicClient(QObject *parent = nullptr);
    ~ClassicClient() override;

    void connectTo(const QString &host, quint16 port, const QString &nick, JoinProtocol proto);
    void disconnectFromHost();
    bool isConnected() const;

    void sendChat(const QString &text);
    void sendField(const Field &field);
    void sendSpecial(int targetSlot, Special special);
    void sendLose();
    void sendStart();
    void sendLevel(int level);
    void sendTeam(const QString &team);

    int localSlot() const { return m_slot; }
    QString playerName(int slot) const;
    bool slotUsed(int slot) const;
    JoinProtocol protocol() const { return m_proto; }

signals:
    void connected();
    void disconnected();
    void errorText(const QString &text);
    void welcomed(int slot);
    void playerUpdated(int slot, const QString &name);
    void playerLeft(int slot);
    void chatReceived(int slot, const QString &text);
    void gameStarted(int startHeight, int startLevel, bool tetrifast);
    void gameEnded();
    void fieldReceived(int slot, const Field &field);
    void specialReceived(int from, int target, Special special);
    void playerLost(int slot);
    void playerWon(int slot);
    void paused(bool on);
    void winlist(const QString &text);

private:
    void onConnected();
    void onReadyRead();
    void sendLogin();
    void sendLine(const QString &line);
    void handle(const ClassicMessage &msg);
    void handlePlayernum(const ClassicMessage &msg, bool tetrifast);
    void onLoginTimeout();

    QTcpSocket m_socket;
    QTimer m_loginTimer;
    QByteArray m_buffer;
    QString m_nick;
    JoinProtocol m_proto = JoinProtocol::Tetrinet113;
    int m_slot = -1;
    bool m_loginSent = false;
    std::array<QString, kMaxPlayers> m_names{};
    std::array<bool, kMaxPlayers> m_used{};
};

} // namespace tnet
