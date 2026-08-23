#pragma once

#include <QByteArray>
#include <QString>

namespace tnet {

constexpr int kMaxNativeFrameSize = 4096;

// Line-based protocol for the built-in native server (not classic TetriNET).
struct Message {
    enum Type {
        Nick,
        Welcome,
        Player,
        Left,
        Chat,
        Start,
        Field,
        Special,
        Lose,
        Win,
        Ping,
        Pong,
        Query,
        Status,
        Unknown
    } type = Unknown;

    int slot = -1;
    int target = -1;
    int value = 0;
    QString text;
    QString data;
};

QByteArray encodeMessage(const Message &msg);
bool decodeMessage(const QByteArray &line, Message &msg);

} // namespace tnet
