// Hosted-game wins accumulate in a scoreboard CSV.
#include "util/Scoreboard.h"

#include <QDir>
#include <QFile>
#include <cassert>

int main()
{
    const QString path = QDir::tempPath() + QStringLiteral("/tetrivibes-scoreboard-test.csv");
    QFile::remove(path);
    tnet::addWin(QStringLiteral("Alice"), path);
    tnet::addWin(QStringLiteral("Bob"), path);
    tnet::addWin(QStringLiteral("alice"), path);
    const auto entries = tnet::loadScoreboard(path);
    assert(entries.size() == 2);
    assert(entries[0].name == QLatin1String("Alice"));
    assert(entries[0].wins == 2);
    assert(entries[1].name == QLatin1String("Bob"));
    assert(entries[1].wins == 1);
    QFile::remove(path);
    return 0;
}
