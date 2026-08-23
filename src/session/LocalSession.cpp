#include "LocalSession.h"

#include <QRandomGenerator>
#include <algorithm>

namespace tnet {

LocalSession::LocalSession(const QString &playerName, int bots, QObject *parent)
    : GameSession(parent)
{
    m_used[0] = true;
    m_names[0] = playerName.isEmpty() ? QStringLiteral("You") : playerName;
    m_engines[0] = std::make_unique<Engine>();
    connect(m_engines[0].get(), &Engine::updated, this, &GameSession::updated);
    connect(m_engines[0].get(), &Engine::inventoryChanged, this, &GameSession::updated);
    connect(m_engines[0].get(), &Engine::died, this, [this]() {
        m_alive[0] = false;
        checkWin();
        emit updated();
    });

    const int nBots = std::clamp(bots, 0, kMaxPlayers - 1);
    for (int i = 1; i <= nBots; ++i) {
        m_used[static_cast<size_t>(i)] = true;
        m_names[static_cast<size_t>(i)] = QStringLiteral("Bot %1").arg(i);
        m_engines[static_cast<size_t>(i)] = std::make_unique<Engine>();
        m_bots[static_cast<size_t>(i)] = std::make_unique<Bot>(m_engines[static_cast<size_t>(i)].get());
        connect(m_engines[static_cast<size_t>(i)].get(), &Engine::updated, this, &GameSession::updated);
        connect(m_engines[static_cast<size_t>(i)].get(), &Engine::died, this, [this, i]() {
            m_alive[static_cast<size_t>(i)] = false;
            checkWin();
            emit updated();
        });
    }
}

Engine *LocalSession::localEngine()
{
    return m_engines[0].get();
}

Field LocalSession::opponentField(int slot) const
{
    if (!slotOccupied(slot) || !m_engines[static_cast<size_t>(slot)])
        return {};
    return m_engines[static_cast<size_t>(slot)]->snapshot(true);
}

QString LocalSession::playerName(int slot) const
{
    if (!slotOccupied(slot))
        return {};
    return m_names[static_cast<size_t>(slot)];
}

bool LocalSession::slotOccupied(int slot) const
{
    return slot >= 0 && slot < kMaxPlayers && m_used[static_cast<size_t>(slot)];
}

bool LocalSession::slotAlive(int slot) const
{
    return slotOccupied(slot) && m_alive[static_cast<size_t>(slot)];
}

QString LocalSession::statusText() const
{
    if (!m_playing)
        return QStringLiteral("Practice lobby — press Start");
    return QStringLiteral("Practice  •  L%1  %2 pts  %3 lines")
        .arg(m_engines[0]->level())
        .arg(m_engines[0]->score())
        .arg(m_engines[0]->lines());
}

void LocalSession::startGame()
{
    const uint32_t seed = QRandomGenerator::global()->generate();
    m_playing = true;
    for (int i = 0; i < kMaxPlayers; ++i) {
        if (!m_used[static_cast<size_t>(i)])
            continue;
        m_alive[static_cast<size_t>(i)] = true;
        m_engines[static_cast<size_t>(i)]->reset(seed + static_cast<uint32_t>(i) * 9973u);
    }
    emit chatReceived(QStringLiteral("* Game started"));
    emit statusChanged();
    emit updated();
}

void LocalSession::useSpecial(int targetSlot)
{
    Engine *eng = localEngine();
    if (!m_playing || !eng->alive() || eng->inventory().isEmpty())
        return;
    if (!slotOccupied(targetSlot) || !m_alive[static_cast<size_t>(targetSlot)])
        return;
    const Special s = eng->takeSpecial();
    applySpecialTo(0, targetSlot, s);
}

void LocalSession::applySpecialTo(int from, int target, Special special)
{
    const QString line = QStringLiteral("* %1 used %2 on %3")
                             .arg(m_names[static_cast<size_t>(from)],
                                  specialName(special),
                                  m_names[static_cast<size_t>(target)]);
    emit chatReceived(line);

    if (special == Special::SwitchField) {
        if (from == target)
            return;
        const Field a = m_engines[static_cast<size_t>(from)]->field();
        const Field b = m_engines[static_cast<size_t>(target)]->field();
        m_engines[static_cast<size_t>(from)]->setField(b);
        m_engines[static_cast<size_t>(target)]->setField(a);
        return;
    }
    m_engines[static_cast<size_t>(target)]->applySpecial(special);
}

void LocalSession::sendChat(const QString &text)
{
    emit chatReceived(QStringLiteral("%1: %2").arg(m_names[0], text));
}

QVector<int> LocalSession::heights() const
{
    QVector<int> h(kMaxPlayers, -1);
    for (int i = 0; i < kMaxPlayers; ++i) {
        if (m_used[static_cast<size_t>(i)] && m_alive[static_cast<size_t>(i)])
            h[i] = m_engines[static_cast<size_t>(i)]->field().stackHeight();
    }
    return h;
}

void LocalSession::tick(int ms)
{
    if (!m_playing)
        return;
    for (int i = 0; i < kMaxPlayers; ++i) {
        if (m_used[static_cast<size_t>(i)] && m_alive[static_cast<size_t>(i)])
            m_engines[static_cast<size_t>(i)]->tick(ms);
    }

    m_botAcc += ms;
    if (m_botAcc >= 90) {
        m_botAcc = 0;
        for (int i = 1; i < kMaxPlayers; ++i) {
            if (!m_bots[static_cast<size_t>(i)] || !m_alive[static_cast<size_t>(i)])
                continue;
            m_bots[static_cast<size_t>(i)]->thinkAndAct();
            Engine *botEng = m_engines[static_cast<size_t>(i)].get();
            if (!botEng->inventory().isEmpty() && QRandomGenerator::global()->bounded(100) < 18) {
                const int target = m_bots[static_cast<size_t>(i)]->chooseTarget(heights(), i);
                const Special s = botEng->takeSpecial();
                const bool helpful = (s == Special::ClearLine || s == Special::Gravity
                                      || s == Special::Nuke || s == Special::ClearSpecial);
                applySpecialTo(i, helpful ? i : target, s);
            }
        }
    }
    emit statusChanged();
}

void LocalSession::checkWin()
{
    if (!m_playing)
        return;
    int alive = 0;
    int winner = -1;
    for (int i = 0; i < kMaxPlayers; ++i) {
        if (m_used[static_cast<size_t>(i)] && m_alive[static_cast<size_t>(i)]) {
            ++alive;
            winner = i;
        }
    }
    if (alive <= 1) {
        m_playing = false;
        if (winner >= 0)
            emit gameEnded(QStringLiteral("%1 wins!").arg(m_names[static_cast<size_t>(winner)]));
        else
            emit gameEnded(QStringLiteral("Draw"));
        emit statusChanged();
    }
}

} // namespace tnet
