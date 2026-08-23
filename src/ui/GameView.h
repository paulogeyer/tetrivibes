#pragma once

#include "game/Types.h"
#include "session/GameSession.h"

#include <QWidget>
#include <QString>
#include <array>

class QDialog;
class QLabel;
class QPlainTextEdit;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace tnet {

class FieldWidget;
class InventoryBar;
class GameSession;

// Play UI: local field, opponents, inventory, partyline, channel list dialog.
class GameView : public QWidget {
    Q_OBJECT
public:
    explicit GameView(QWidget *parent = nullptr);

    void setSession(GameSession *session);
    void refresh();
    void appendChat(const QString &line);
    void setStatus(const QString &text);

signals:
    void leaveRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

private:
    void sendChat();
    void startGame();
    void showChannels();
    void fillChannelTable(const QVector<ChannelInfo> &channels);

    GameSession *m_session = nullptr;
    FieldWidget *m_local = nullptr;
    FieldWidget *m_next = nullptr;
    std::array<FieldWidget *, kMaxPlayers> m_opponents{};
    InventoryBar *m_inv = nullptr;
    QLabel *m_status = nullptr;
    QPlainTextEdit *m_chat = nullptr;
    QLineEdit *m_input = nullptr;
    QPushButton *m_start = nullptr;
    QPushButton *m_channels = nullptr;
    bool m_startArmed = false;
    QString m_secretKeys;
    QDialog *m_channelDlg = nullptr;
    QTableWidget *m_channelTable = nullptr;
};

} // namespace tnet
