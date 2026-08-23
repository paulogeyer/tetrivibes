#pragma once

#include "GameSession.h"
#include "net/ClassicClient.h"

#include <QTimer>
#include <array>

namespace tnet {

// Classic TetriNET 1.13 / TetriFast session.
// After login, a silent /list+/who fills the title: "#channel: description".
class ClassicSession : public GameSession {
    Q_OBJECT
public:
    ClassicSession(const QString &host, quint16 port, const QString &nick, JoinProtocol proto,
                   const QString &serverLabel = QString(), QObject *parent = nullptr);
    ~ClassicSession() override;

    Engine *localEngine() override;
    int localSlot() const override;
    Field opponentField(int slot) const override;
    QString playerName(int slot) const override;
    bool slotOccupied(int slot) const override;
    bool slotAlive(int slot) const override;
    bool canStart() const override;
    bool isHost() const override { return false; }
    bool playing() const override { return m_playing; }
    QString statusText() const override;

    void startGame() override;
    void requestChannels() override;
    bool hasChannels() const override { return true; }
    void useSpecial(int targetSlot) override;
    void sendChat(const QString &text) override;
    void tick(int ms) override;
    void begin();

private:
    void onGameStarted(int startHeight, int startLevel, bool tetrifast);
    void onSpecial(int from, int target, Special special);
    void broadcastField();
    void applyChannel(const QString &name);
    void finishJoin();
    void noteChannel(const QString &text);
    void noteTopic(const QString &text);
    QString listedDescription(const QString &name) const;
    void applyListedDescription();
    void ingestListLine(const QString &text);
    void finishList();
    void noteWhoLine(const QString &text);
    bool looksLikeListLine(const QString &text) const;

    ClassicClient m_client;
    Engine m_engine;
    QString m_host;
    QString m_label;
    QString m_nick;
    quint16 m_port = 31457;
    JoinProtocol m_proto = JoinProtocol::Tetrinet113;
    std::array<Field, kMaxPlayers> m_fields{};
    std::array<bool, kMaxPlayers> m_alive{};
    bool m_playing = false;
    int m_fieldAcc = 0;
    int m_lastLevel = 1;
    QString m_lastSent;
    QString m_channel;
    QString m_topic;
    QString m_pendingChannel; // /join in flight; title shows "Joining …"
    QString m_pendingDesc;
    bool m_listing = false;
    bool m_silentList = false; // hide /list output (used on connect)
    bool m_titleReady = false; // false until first /list completes
    bool m_didHello = false;   // playernum can fire again on channel switch
    int m_listWait = 0;
    QVector<ChannelInfo> m_listed;
};

} // namespace tnet
