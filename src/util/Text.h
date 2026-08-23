#pragma once

#include <QString>

namespace tnet {

// Partyline text is mixed with TetriNET color bytes (values < 32).
inline QString printable(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar c : text) {
        if (c.unicode() >= 32 || c == QLatin1Char('\t'))
            out += c;
    }
    return out.trimmed();
}

} // namespace tnet
