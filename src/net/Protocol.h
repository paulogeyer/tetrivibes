#pragma once

#include <QByteArray>
#include <QString>

namespace tnet {

constexpr int kMaxNativeFrameSize = 4096;
// Negative values are outside the native Tetris seed range and can safely mark the hidden mode.
constexpr int kInvadersStartMarker = -0x49564e;

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
        Input,
        State,
        Unknown
    } type = Unknown;

    int slot = -1;
    int target = -1;
    int value = 0;
    int level = 1;
    int score = 0;
    int lines = 0;
    QString text;
    QString data;
    QString piece;
};

QByteArray encodeMessage(const Message &msg);
bool decodeMessage(const QByteArray &line, Message &msg);

} // namespace tnet
