#include "GameView.h"

#include "FieldWidget.h"
#include "InventoryBar.h"
#include <QAbstractItemView>
#include <QDialog>
#include <QFocusEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace tnet {

GameView::GameView(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);

    m_local = new FieldWidget;
    m_local->setFocusProxy(this);
    m_next = new FieldWidget;
    m_next->setCompact(true);
    m_next->setTitle(QStringLiteral("NEXT"));
    m_next->setFocusProxy(this);
    m_inv = new InventoryBar;
    m_inv->setFocusProxy(this);
    m_status = new QLabel(QStringLiteral("Ready"));
    m_status->setObjectName(QStringLiteral("status"));

    auto *oppGrid = new QGridLayout;
    oppGrid->setSpacing(6);
    for (int i = 0; i < kMaxPlayers; ++i) {
        m_opponents[static_cast<size_t>(i)] = new FieldWidget;
        m_opponents[static_cast<size_t>(i)]->setCompact(true);
        m_opponents[static_cast<size_t>(i)]->setFocusProxy(this);
        oppGrid->addWidget(m_opponents[static_cast<size_t>(i)], i / 3, i % 3);
    }

    m_chat = new QPlainTextEdit;
    m_chat->setReadOnly(true);
    m_chat->setMaximumHeight(140);
    m_chat->setPlaceholderText(QStringLiteral("Partyline"));
    m_input = new QLineEdit;
    m_input->setPlaceholderText(QStringLiteral("Chat — press Enter"));
    connect(m_input, &QLineEdit::returnPressed, this, &GameView::sendChat);

    m_start = new QPushButton(QStringLiteral("Start Game"));
    m_start->setAutoDefault(false);
    m_start->setDefault(false);
    m_channels = new QPushButton(QStringLiteral("Channels"));
    auto *leave = new QPushButton(QStringLiteral("Leave"));
    connect(m_start, &QPushButton::clicked, this, &GameView::startGame);
    connect(m_channels, &QPushButton::clicked, this, &GameView::showChannels);
    connect(leave, &QPushButton::clicked, this, &GameView::leaveRequested);

    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(m_start);
    btnRow->addWidget(m_channels);
    btnRow->addWidget(leave);
    btnRow->addStretch();

    auto *right = new QVBoxLayout;
    right->addWidget(m_status);
    right->addLayout(oppGrid, 1);
    right->addWidget(m_inv);
    right->addWidget(m_chat);
    right->addWidget(m_input);
    right->addLayout(btnRow);

    auto *leftCol = new QVBoxLayout;
    leftCol->addWidget(m_local, 1);
    leftCol->addWidget(m_next);

    auto *root = new QHBoxLayout(this);
    root->addLayout(leftCol, 2);
    root->addLayout(right, 3);
}

void GameView::setSession(GameSession *session)
{
    m_session = session;
    m_startArmed = false;
    m_secretKeys.clear();
    m_chat->clear();
    if (m_channelDlg)
        m_channelDlg->hide();
    refresh();
    setFocus();
    QTimer::singleShot(300, this, [this]() { m_startArmed = true; });
}

void GameView::appendChat(const QString &line)
{
    m_chat->appendPlainText(line);
}

void GameView::setStatus(const QString &text)
{
    m_status->setText(text);
}

void GameView::refresh()
{
    if (!m_session)
        return;

    Engine *eng = m_session->localEngine();
    const int me = m_session->localSlot();
    const bool invaders = m_session->secretMode();

    m_local->setTitle(invaders ? QStringLiteral("DEFENDER  %1").arg(m_session->playerName(me))
                               : QStringLiteral("%1  #%2")
                                     .arg(m_session->playerName(me))
                                     .arg(me + 1));
    m_local->setField(eng->field());
    if (eng->hasPiece()) {
        const Piece p = eng->current();
        m_local->setPiece(&p);
    } else {
        m_local->setPiece(nullptr);
    }
    m_local->setAlive(eng->alive() || !m_session->playing());
    m_local->setHighlight(true);

    Field nextField;
    if (eng->started()) {
        Piece n = eng->next();
        n.x = 4;
        n.y = 8;
        nextField.lock(n);
    }
    m_next->setField(nextField);
    m_next->setPiece(nullptr);
    m_next->setVisible(!invaders);

    m_inv->setInventory(eng->inventory());
    m_inv->setVisible(true);
    m_start->setVisible(m_session->canStart());
    m_channels->setVisible(m_session->hasChannels());
    m_status->setText(m_session->statusText());

    for (int i = 0; i < kMaxPlayers; ++i) {
        auto *w = m_opponents[static_cast<size_t>(i)];
        const bool used = m_session->slotOccupied(i);
        w->setVisible(true);
        w->setTitle(used ? (invaders ? QStringLiteral("DEFENDER %1").arg(i + 1)
                                    : QStringLiteral("%1  #%2")
                                          .arg(m_session->playerName(i))
                                          .arg(i + 1))
                         : QStringLiteral("Empty  #%1").arg(i + 1));
        w->setField(used ? m_session->opponentField(i) : Field{});
        w->setPiece(nullptr);
        w->setAlive(!used || !m_session->playing() || m_session->slotAlive(i) || i == me);
        w->setHighlight(i == me);
    }
}

void GameView::sendChat()
{
    if (!m_session)
        return;
    const QString t = m_input->text().trimmed();
    if (t.isEmpty())
        return;
    m_session->sendChat(t);
    m_input->clear();
    setFocus();
}

void GameView::showChannels()
{
    if (!m_session || !m_session->hasChannels())
        return;
    if (!m_channelDlg) {
        m_channelDlg = new QDialog(this);
        m_channelDlg->setWindowTitle(QStringLiteral("Channels"));
        m_channelDlg->resize(520, 320);
        m_channelTable = new QTableWidget(0, 4, m_channelDlg);
        m_channelTable->setHorizontalHeaderLabels(
            {QStringLiteral("Channel"), QStringLiteral("Players"), QStringLiteral("Status"),
             QStringLiteral("Description")});
        m_channelTable->horizontalHeader()->setStretchLastSection(true);
        m_channelTable->verticalHeader()->setVisible(false);
        m_channelTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_channelTable->setSelectionMode(QAbstractItemView::SingleSelection);
        m_channelTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_channelTable->setShowGrid(false);
        m_channelTable->setAlternatingRowColors(true);
        connect(m_channelTable, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
            if (!m_session || row < 0)
                return;
            auto *item = m_channelTable->item(row, 0);
            if (!item)
                return;
            m_session->sendChat(QStringLiteral("/join %1").arg(item->text()));
            m_channelDlg->hide();
            setFocus();
        });
        auto *hint = new QLabel(QStringLiteral("Double-click a channel to join"));
        hint->setObjectName(QStringLiteral("help"));
        auto *lay = new QVBoxLayout(m_channelDlg);
        lay->addWidget(m_channelTable, 1);
        lay->addWidget(hint);
    }
    m_channelTable->setRowCount(1);
    m_channelTable->setItem(0, 0, new QTableWidgetItem(QStringLiteral("Loading…")));
    m_channelTable->setItem(0, 1, new QTableWidgetItem(QString()));
    m_channelTable->setItem(0, 2, new QTableWidgetItem(QString()));
    m_channelTable->setItem(0, 3, new QTableWidgetItem(QString()));
    disconnect(m_session, &GameSession::channelsReceived, this, nullptr);
    connect(m_session, &GameSession::channelsReceived, this, &GameView::fillChannelTable);
    m_session->requestChannels();
    m_channelDlg->show();
    m_channelDlg->raise();
}

void GameView::fillChannelTable(const QVector<ChannelInfo> &channels)
{
    if (!m_channelTable)
        return;
    m_channelTable->setRowCount(channels.size());
    for (int i = 0; i < channels.size(); ++i) {
        m_channelTable->setItem(i, 0, new QTableWidgetItem(channels[i].name));
        m_channelTable->setItem(i, 1, new QTableWidgetItem(channels[i].players));
        m_channelTable->setItem(i, 2, new QTableWidgetItem(channels[i].status));
        m_channelTable->setItem(i, 3, new QTableWidgetItem(channels[i].description));
    }
    if (channels.isEmpty()) {
        m_channelTable->setRowCount(1);
        m_channelTable->setItem(0, 0, new QTableWidgetItem(QStringLiteral("No channels listed")));
    }
}

void GameView::startGame()
{
    if (!m_startArmed || !m_session)
        return;
    m_session->startGame();
    setFocus();
}

void GameView::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
}

void GameView::keyReleaseEvent(QKeyEvent *event)
{
    QWidget::keyReleaseEvent(event);
}

void GameView::keyPressEvent(QKeyEvent *event)
{
    if (!m_session || m_input->hasFocus()) {
        QWidget::keyPressEvent(event);
        return;
    }

    if (event->isAutoRepeat() && event->key() == Qt::Key_Space)
        return;

    if (!m_session->playing() && !event->isAutoRepeat() && !event->text().isEmpty()) {
        m_secretKeys += event->text().toUpper();
        if (m_secretKeys.size() > 8)
            m_secretKeys = m_secretKeys.right(8);
        if (m_secretKeys == QLatin1String("INVADERS") && m_session->activateSecretMode()) {
            m_secretKeys.clear();
            refresh();
            return;
        }
    }

    if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_6) {
        m_session->useSpecial(event->key() - Qt::Key_1);
        return;
    }

    switch (event->key()) {
    case Qt::Key_Left:
        m_session->moveLeft();
        break;
    case Qt::Key_Right:
        m_session->moveRight();
        break;
    case Qt::Key_Down:
        m_session->softDrop();
        break;
    case Qt::Key_Up:
    case Qt::Key_X:
        m_session->rotate(1);
        break;
    case Qt::Key_Z:
    case Qt::Key_Control:
        m_session->rotate(-1);
        break;
    case Qt::Key_Space:
        m_session->hardDrop();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        m_input->setFocus();
        break;
    default:
        QWidget::keyPressEvent(event);
        break;
    }
}

} // namespace tnet
