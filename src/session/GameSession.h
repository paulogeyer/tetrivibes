#pragma once

#include "game/Engine.h"
#include "game/Types.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace tnet {

// One row from a server /list reply.
struct ChannelInfo {
    QString name;
    QString players;
    QString status;
    QString description;
};

// UI-facing match: practice bots, native LAN, or a classic TetriNET client.
class GameSession : public QObject {
    Q_OBJECT
public:
    explicit GameSession(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    virtual Engine *localEngine() = 0;
    virtual int localSlot() const = 0;
    virtual Field opponentField(int slot) const = 0;
    virtual QString playerName(int slot) const = 0;
    virtual bool slotOccupied(int slot) const = 0;
    virtual bool slotAlive(int slot) const = 0;
    virtual bool canStart() const { return false; }
    virtual bool isHost() const { return false; }
    virtual bool secretMode() const { return false; }
    virtual bool playing() const = 0;
    virtual QString statusText() const = 0;

    virtual void startGame() {}
    virtual bool activateSecretMode() { return false; }
    virtual void requestChannels() {}
    virtual bool hasChannels() const { return false; }
    virtual void useSpecial(int targetSlot) = 0;
    virtual void sendChat(const QString &text) = 0;
    virtual void tick(int ms) = 0;
    virtual void moveLeft() { localEngine()->moveLeft(); }
    virtual void moveRight() { localEngine()->moveRight(); }
    virtual void rotate(int direction) { localEngine()->rotate(direction); }
    virtual void softDrop() { localEngine()->softDrop(); }
    virtual void hardDrop() { localEngine()->hardDrop(); }

signals:
    void updated();
    void chatReceived(const QString &line);
    void statusChanged();
    void gameEnded(const QString &result);
    void channelsReceived(const QVector<ChannelInfo> &channels);
};

} // namespace tnet
