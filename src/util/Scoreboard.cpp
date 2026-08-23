#include "Scoreboard.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>
#include <algorithm>

namespace tnet {
namespace {

QString csvEscape(const QString &value)
{
    if (!value.contains(QLatin1Char(',')) && !value.contains(QLatin1Char('"'))
        && !value.contains(QLatin1Char('\n')))
        return value;
    QString q = value;
    q.replace(QLatin1String("\""), QLatin1String("\"\""));
    return QLatin1Char('"') + q + QLatin1Char('"');
}

QStringList csvSplit(const QString &line)
{
    QStringList out;
    QString cur;
    bool quoted = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (quoted) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < line.size() && line[i + 1] == QLatin1Char('"')) {
                    cur += QLatin1Char('"');
                    ++i;
                } else {
                    quoted = false;
                }
            } else {
                cur += c;
            }
        } else if (c == QLatin1Char('"')) {
            quoted = true;
        } else if (c == QLatin1Char(',')) {
            out << cur;
            cur.clear();
        } else {
            cur += c;
        }
    }
    out << cur;
    return out;
}

void saveScoreboard(const QString &path, const QVector<ScoreEntry> &entries)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return;
    QTextStream out(&file);
    out << QStringLiteral("name,wins\n");
    for (const auto &e : entries)
        out << csvEscape(e.name) << QLatin1Char(',') << e.wins << QLatin1Char('\n');
}

} // namespace

QString scoreboardPath()
{
    return QDir::homePath() + QStringLiteral("/.tetrivibes/scoreboard.csv");
}

QVector<ScoreEntry> loadScoreboard(const QString &path)
{
    QVector<ScoreEntry> entries;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return entries;
    QTextStream in(&file);
    bool header = true;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;
        const QStringList p = csvSplit(line);
        if (header && p.value(0).compare(QLatin1String("name"), Qt::CaseInsensitive) == 0) {
            header = false;
            continue;
        }
        header = false;
        if (p.size() < 2)
            continue;
        ScoreEntry e;
        e.name = p[0].trimmed();
        e.wins = p[1].toInt();
        if (e.name.isEmpty() || e.wins < 0)
            continue;
        entries.push_back(e);
    }
    std::sort(entries.begin(), entries.end(), [](const ScoreEntry &a, const ScoreEntry &b) {
        if (a.wins != b.wins)
            return a.wins > b.wins;
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    return entries;
}

void addWin(const QString &name, const QString &path)
{
    const QString nick = name.trimmed();
    if (nick.isEmpty())
        return;
    QVector<ScoreEntry> entries = loadScoreboard(path);
    bool found = false;
    for (auto &e : entries) {
        if (e.name.compare(nick, Qt::CaseInsensitive) != 0)
            continue;
        ++e.wins;
        found = true;
        break;
    }
    if (!found)
        entries.push_back(ScoreEntry{nick, 1});
    saveScoreboard(path, entries);
}

} // namespace tnet
