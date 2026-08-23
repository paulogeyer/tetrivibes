#pragma once

#include <QMainWindow>

class QStackedWidget;

namespace tnet {

class LobbyWidget;
class GameView;
class GameSession;
class Server;

// Lobby <-> in-game stack. A 16ms timer drives session->tick().
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void timerEvent(QTimerEvent *event) override;

private:
    void hostGame();
    void joinGame();
    void practiceGame();
    void attachSession(GameSession *session);
    void leaveGame();
    void shutdownHostedServer();
    void syncHostedServerUi();

    QStackedWidget *m_stack = nullptr;
    LobbyWidget *m_lobby = nullptr;
    GameView *m_game = nullptr;
    GameSession *m_session = nullptr;
    Server *m_hostedServer = nullptr;
    int m_timer = 0;
};

} // namespace tnet
