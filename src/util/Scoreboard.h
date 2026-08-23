#pragma once

#include <QString>
#include <QVector>

namespace tnet {

struct ScoreEntry {
    QString name;
    int wins = 0;
};

QString scoreboardPath();
QVector<ScoreEntry> loadScoreboard(const QString &path = scoreboardPath());
void addWin(const QString &name, const QString &path = scoreboardPath());

} // namespace tnet
