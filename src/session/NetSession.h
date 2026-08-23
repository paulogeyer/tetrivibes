#pragma once

#include "GameSession.h"
#include "net/Client.h"
#include "net/Server.h"

#include <array>

namespace tnet {

// Native multiplayer: optional embedded Server plus a Client to localhost or a remote host.
class NetSession : public GameSession {
    Q_OBJECT
public:
    NetSession(bool host, const QString &hostName, quint16 port, const QString &nick,
               const QString &serverName = QString(), int maxPlayers = kMaxPlayers,
               int botCount = 0, QObject *parent = nullptr);
    ~NetSession() override;

    Engine *localEngine() override;
    int localSlot() const override;
    Field opponentField(int slot) const override;
    QString playerName(int slot) const override;
    bool slotOccupied(int slot) const override;
    bool slotAlive(int slot) const override;
    bool canStart() const override;
    bool isHost() const override { return m_host; }
    bool playing() const override { return m_playing; }
    QString statusText() const override;

    void startGame() override;
    void useSpecial(int targetSlot) override;
    void sendChat(const QString &text) override;
    void tick(int ms) override;
    void moveLeft() override;
    void moveRight() override;
    void rotate(int direction) override;
    void softDrop() override;
    void hardDrop() override;

    bool serverOk() const;
    void begin();

private:
    void onGameStarted(int seed);
    void onSpecial(int from, int target, Special special);
    void onState(int slot, bool alive, int level, int score, int lines, const QString &inventory,
                 const QString &field, const QString &piece);

    bool m_host = false;
    QString m_nick;
    QString m_serverName;
    QString m_pendingHost;
    quint16 m_pendingPort = 31457;
    int m_maxPlayers = kMaxPlayers;
    int m_botCount = 0;
    Server *m_server = nullptr;
    Client m_client;
    Engine m_engine;
    std::array<Field, kMaxPlayers> m_fields{};
    std::array<bool, kMaxPlayers> m_alive{};
    bool m_playing = false;
};

} // namespace tnet
