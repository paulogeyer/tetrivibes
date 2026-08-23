#include "MainWindow.h"

#include "GameView.h"
#include "LobbyWidget.h"
#include "session/ClassicSession.h"
#include "session/LocalSession.h"
#include "session/NetSession.h"

#include <QApplication>
#include <QMessageBox>
#include <QStackedWidget>
#include <QTimerEvent>

namespace tnet {

static const char *kStyle = R"(
QWidget { background: #0c0c12; color: #e6e6ee; font-family: "DejaVu Sans", sans-serif; }
QLabel#title { color: #ffcc33; font-size: 42px; font-weight: 800; letter-spacing: 8px; }
QLabel#subtitle { color: #8a8aa0; font-size: 14px; }
QLabel#help { color: #8a8aa0; font-size: 12px; }
QLabel#status { color: #ffcc33; font-weight: 700; font-size: 14px; }
QLineEdit, QSpinBox, QPlainTextEdit {
    background: #16161f; color: #eee; border: 1px solid #333348; padding: 6px;
    selection-background-color: #3a3a60;
}
QPushButton {
    background: #2a2a40; color: #eee; border: 1px solid #44445a; padding: 8px 16px;
    font-weight: 700;
}
QPushButton:hover { background: #3a3a58; }
QPushButton#primary { background: #8a1c1c; border-color: #c43; }
QPushButton#primary:hover { background: #a82828; }
QPushButton#secondary { background: #1c4a2a; border-color: #3a8; }
QPushButton#secondary:hover { background: #286838; }
QTableWidget { background: #12121a; alternate-background-color: #181822; gridline-color: #2a2a38; }
QHeaderView::section { background: #1a1a28; color: #ffcc33; border: 0; padding: 6px; font-weight: 700; }
QTableWidget::item:selected { background: #3a3a60; color: #fff; }
)";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Tetrinet"));
    resize(1100, 720);
    qApp->setStyleSheet(QString::fromUtf8(kStyle));

    m_stack = new QStackedWidget;
    m_lobby = new LobbyWidget;
    m_game = new GameView;
    m_stack->addWidget(m_lobby);
    m_stack->addWidget(m_game);
    setCentralWidget(m_stack);

    connect(m_lobby, &LobbyWidget::hostClicked, this, &MainWindow::hostGame);
    connect(m_lobby, &LobbyWidget::joinClicked, this, &MainWindow::joinGame);
    connect(m_lobby, &LobbyWidget::practiceClicked, this, &MainWindow::practiceGame);
    connect(m_game, &GameView::leaveRequested, this, &MainWindow::leaveGame);

    m_timer = startTimer(16);
}

MainWindow::~MainWindow()
{
    killTimer(m_timer);
    m_game->setSession(nullptr);
    delete m_session;
    m_session = nullptr;
}

void MainWindow::attachSession(GameSession *session)
{
    if (m_session) {
        m_session->deleteLater();
        m_session = nullptr;
    }
    m_session = session;
    m_game->setSession(m_session);
    connect(m_session, &GameSession::updated, m_game, &GameView::refresh);
    connect(m_session, &GameSession::statusChanged, this, [this]() {
        if (m_session)
            m_game->setStatus(m_session->statusText());
    });
    connect(m_session, &GameSession::chatReceived, m_game, &GameView::appendChat);
    connect(m_session, &GameSession::gameEnded, this, [this](const QString &r) {
        m_game->appendChat(QStringLiteral("* %1").arg(r));
        QMessageBox::information(this, QStringLiteral("Tetrinet"), r);
    });
    m_stack->setCurrentWidget(m_game);
    if (auto *net = qobject_cast<NetSession *>(m_session))
        net->begin();
    if (auto *classic = qobject_cast<ClassicSession *>(m_session))
        classic->begin();
    m_game->refresh();
    m_game->setFocus();
}

void MainWindow::hostGame()
{
    attachSession(new NetSession(true, m_lobby->host(), m_lobby->port(), m_lobby->nickname(),
                                m_lobby->serverName(), m_lobby->maxPlayers(), m_lobby->botCount(),
                                this));
}

void MainWindow::joinGame()
{
    JoinProtocol proto = m_lobby->protocol();
    const QString host = m_lobby->host();
    if (proto == JoinProtocol::Auto)
        proto = JoinProtocol::Tetrinet113;
    const quint16 port = m_lobby->port();
    const QString label = m_lobby->selectedServerName();
    if (proto == JoinProtocol::Native) {
        attachSession(new NetSession(false, host, port, m_lobby->nickname(), label,
                                    m_lobby->maxPlayers(), 0, this));
        return;
    }
    attachSession(new ClassicSession(host, port, m_lobby->nickname(), proto, label, this));
}

void MainWindow::practiceGame()
{
    auto *s = new LocalSession(m_lobby->nickname(), m_lobby->botCount(), this);
    attachSession(s);
    s->startGame();
}

void MainWindow::leaveGame()
{
    if (m_session) {
        m_session->deleteLater();
        m_session = nullptr;
    }
    m_game->setSession(nullptr);
    m_stack->setCurrentWidget(m_lobby);
    m_lobby->showMain();
}

void MainWindow::timerEvent(QTimerEvent *event)
{
    if (event->timerId() != m_timer)
        return;
    if (m_session)
        m_session->tick(16);
}

} // namespace tnet
