#pragma once

#include "GameSession.h"
#include "game/Bot.h"

#include <array>
#include <memory>

namespace tnet {

// Offline match: one human engine plus bot engines in the same process.
class LocalSession : public GameSession {
    Q_OBJECT
public:
    explicit LocalSession(const QString &playerName, int bots, QObject *parent = nullptr);

    Engine *localEngine() override;
    int localSlot() const override { return 0; }
    Field opponentField(int slot) const override;
    QString playerName(int slot) const override;
    bool slotOccupied(int slot) const override;
    bool slotAlive(int slot) const override;
    bool canStart() const override { return !m_playing; }
    bool isHost() const override { return true; }
    bool playing() const override { return m_playing; }
    QString statusText() const override;

    void startGame() override;
    void useSpecial(int targetSlot) override;
    void sendChat(const QString &text) override;
    void tick(int ms) override;

private:
    void applySpecialTo(int from, int target, Special special);
    void checkWin();
    QVector<int> heights() const;

    std::array<std::unique_ptr<Engine>, kMaxPlayers> m_engines;
    std::array<std::unique_ptr<Bot>, kMaxPlayers> m_bots;
    std::array<QString, kMaxPlayers> m_names;
    std::array<bool, kMaxPlayers> m_used{};
    std::array<bool, kMaxPlayers> m_alive{};
    bool m_playing = false;
    int m_botAcc = 0;
};

} // namespace tnet
