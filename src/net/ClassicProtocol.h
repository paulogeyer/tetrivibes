#pragma once

#include "game/Field.h"
#include "game/Types.h"

#include <QByteArray>
#include <QHostAddress>
#include <QString>
#include <QStringList>
#include <QVector>

namespace tnet {

// TetriNET 1.13 / TetriFast wire helpers.
// Lines are ASCII terminated with 0xFF. Player numbers on the wire are 1..6.

struct ClassicMessage {
    QString cmd;
    QStringList args;
    QString raw;
};

QByteArray encodeClassicLogin(const QString &nick, JoinProtocol proto, const QHostAddress &serverIp);
QByteArray frameClassic(const QString &line);
bool parseClassicLine(const QString &line, ClassicMessage &msg);
int classicPlayerToSlot(int playernum);
int slotToClassicPlayer(int slot);

QString encodeClassicField(const Field &field);
void applyClassicField(Field &field, const QString &data);

} // namespace tnet
