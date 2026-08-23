#include "LobbyWidget.h"

#include "net/StatusQuery.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkDatagram>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QUrl>
#include <QXmlStreamReader>
#include <QPushButton>
#include <QSettings>
#include <QShowEvent>
#include <QSpinBox>
#include <QTextStream>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace tnet {
namespace {

constexpr quint16 kAnnouncePort = 31458;

QString serversFilePath()
{
    return QDir::homePath() + QStringLiteral("/.tetrivibes/servers.csv");
}

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

QString keyOf(const QString &host, quint16 port)
{
    return QStringLiteral("%1:%2").arg(host).arg(port);
}

} // namespace

LobbyWidget::LobbyWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *title = new QLabel(QStringLiteral("TETRIVIBES"));
    title->setObjectName(QStringLiteral("title"));
    title->setAlignment(Qt::AlignCenter);

    auto *sub = new QLabel(QStringLiteral("Multiplayer tetromino combat"));
    sub->setAlignment(Qt::AlignCenter);
    sub->setObjectName(QStringLiteral("subtitle"));

    m_nick = new QLineEdit(QStringLiteral("Player"));
    m_nick->setMaxLength(16);
    QSettings settings;
    const QString savedNick = settings.value(QStringLiteral("nickname")).toString();
    if (!savedNick.isEmpty())
        m_nick->setText(savedNick);
    connect(m_nick, &QLineEdit::editingFinished, this, [this]() {
        QSettings s;
        s.setValue(QStringLiteral("nickname"), m_nick->text().trimmed());
    });

    m_serverName = new QLineEdit(QStringLiteral("Tetrivibes"));
    m_serverName->setMaxLength(32);
    m_host = new QLineEdit(QStringLiteral("127.0.0.1"));
    m_host->hide();
    m_port = new QSpinBox;
    m_port->setRange(1, 65535);
    m_port->setValue(31457);
    m_maxPlayers = new QSpinBox;
    m_maxPlayers->setRange(2, 6);
    m_maxPlayers->setValue(6);
    m_bots = new QSpinBox;
    m_bots->setRange(0, 5);
    m_bots->setValue(2);
    m_protocol = new QComboBox;
    m_protocol->addItem(QStringLiteral("Auto"), static_cast<int>(JoinProtocol::Auto));
    m_protocol->addItem(QStringLiteral("TetriNET 1.13"), static_cast<int>(JoinProtocol::Tetrinet113));
    m_protocol->addItem(QStringLiteral("TetriFast"), static_cast<int>(JoinProtocol::TetriFast));
    m_protocol->addItem(QStringLiteral("Native"), static_cast<int>(JoinProtocol::Native));

    auto *nickRow = new QHBoxLayout;
    nickRow->addWidget(new QLabel(QStringLiteral("Nickname")));
    nickRow->addWidget(m_nick, 1);

    m_table = new QTableWidget(0, 5);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("Server"), QStringLiteral("Address"), QStringLiteral("Version"),
         QStringLiteral("Players"), QStringLiteral("Status")});
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setTextElideMode(Qt::ElideRight);
    m_table->setWordWrap(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setMinimumHeight(180);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &LobbyWidget::applySelection);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int, int) { tryJoin(); });

    auto *refreshBtn = new QPushButton(QStringLiteral("Refresh"));
    auto *addBtn = new QPushButton(QStringLiteral("Add Server"));
    auto *removeBtn = new QPushButton(QStringLiteral("Remove"));
    connect(refreshBtn, &QPushButton::clicked, this, &LobbyWidget::refreshServers);
    connect(addBtn, &QPushButton::clicked, this, &LobbyWidget::addServer);
    connect(removeBtn, &QPushButton::clicked, this, &LobbyWidget::removeSelected);

    auto *listBtns = new QHBoxLayout;
    listBtns->addWidget(refreshBtn);
    listBtns->addWidget(addBtn);
    listBtns->addWidget(removeBtn);
    listBtns->addStretch();

    auto *joinBtn = new QPushButton(QStringLiteral("Join Server"));
    auto *hostMenuBtn = new QPushButton(QStringLiteral("Host Game…"));
    joinBtn->setObjectName(QStringLiteral("primary"));
    connect(joinBtn, &QPushButton::clicked, this, &LobbyWidget::tryJoin);
    connect(hostMenuBtn, &QPushButton::clicked, this, &LobbyWidget::showHost);

    auto *mainBtns = new QHBoxLayout;
    mainBtns->addWidget(joinBtn);
    mainBtns->addWidget(hostMenuBtn);

    auto *mainPage = new QWidget;
    auto *mainBox = new QVBoxLayout(mainPage);
    mainBox->addWidget(title);
    mainBox->addWidget(sub);
    mainBox->addSpacing(8);
    mainBox->addLayout(nickRow);
    mainBox->addWidget(m_table, 1);
    mainBox->addLayout(listBtns);
    mainBox->addLayout(mainBtns);
    mainBox->setContentsMargins(28, 16, 28, 16);

    auto *hostTitle = new QLabel(QStringLiteral("HOST GAME"));
    hostTitle->setObjectName(QStringLiteral("title"));
    hostTitle->setAlignment(Qt::AlignCenter);
    m_hostStatus = new QLabel(QStringLiteral("Start a local server or practice vs bots"));
    m_hostStatus->setAlignment(Qt::AlignCenter);
    m_hostStatus->setObjectName(QStringLiteral("subtitle"));

    auto *hostForm = new QFormLayout;
    hostForm->addRow(QStringLiteral("Server name"), m_serverName);
    hostForm->addRow(QStringLiteral("Port"), m_port);
    hostForm->addRow(QStringLiteral("Players"), m_maxPlayers);
    hostForm->addRow(QStringLiteral("Protocol"), m_protocol);
    hostForm->addRow(QStringLiteral("Practice bots"), m_bots);

    m_hostStart = new QPushButton(QStringLiteral("Start Server"));
    auto *pracBtn = new QPushButton(QStringLiteral("Practice vs Bots"));
    auto *backBtn = new QPushButton(QStringLiteral("Back"));
    m_shutdown = new QPushButton(QStringLiteral("Shutdown Server"));
    m_hostStart->setObjectName(QStringLiteral("primary"));
    pracBtn->setObjectName(QStringLiteral("secondary"));
    m_shutdown->setObjectName(QStringLiteral("primary"));
    m_shutdown->hide();
    connect(m_hostStart, &QPushButton::clicked, this, &LobbyWidget::hostClicked);
    connect(pracBtn, &QPushButton::clicked, this, &LobbyWidget::practiceClicked);
    connect(backBtn, &QPushButton::clicked, this, &LobbyWidget::showMain);
    connect(m_shutdown, &QPushButton::clicked, this, &LobbyWidget::shutdownClicked);

    auto *hostBtns = new QHBoxLayout;
    hostBtns->addWidget(backBtn);
    hostBtns->addStretch();
    hostBtns->addWidget(pracBtn);
    hostBtns->addWidget(m_shutdown);
    hostBtns->addWidget(m_hostStart);

    auto *hostPage = new QWidget;
    auto *hostBox = new QVBoxLayout(hostPage);
    hostBox->addStretch();
    hostBox->addWidget(hostTitle);
    hostBox->addWidget(m_hostStatus);
    hostBox->addSpacing(16);
    hostBox->addLayout(hostForm);
    hostBox->addSpacing(16);
    hostBox->addLayout(hostBtns);
    hostBox->addStretch();
    hostBox->setContentsMargins(40, 20, 40, 20);

    m_pages = new QStackedWidget;
    m_pages->addWidget(mainPage);
    m_pages->addWidget(hostPage);

    auto *outer = new QHBoxLayout(this);
    outer->addStretch();
    auto *inner = new QWidget;
    inner->setMinimumWidth(680);
    inner->setMaximumWidth(820);
    auto *innerBox = new QVBoxLayout(inner);
    innerBox->setContentsMargins(0, 0, 0, 0);
    innerBox->addWidget(m_pages);
    outer->addWidget(inner);
    outer->addStretch();

    buildDefaults();
    loadCustom();
    rebuildTable();

    m_lan.bind(QHostAddress(QHostAddress::AnyIPv4), kAnnouncePort,
               QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    connect(&m_lan, &QUdpSocket::readyRead, this, &LobbyWidget::onLanDatagram);

    m_refresh = new QTimer(this);
    m_refresh->setInterval(4000);
    connect(m_refresh, &QTimer::timeout, this, [this]() {
        if (isVisible())
            refreshServers();
    });
    m_refresh->start();
}

void LobbyWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_pages && m_pages->currentIndex() == 0)
        refreshServers();
}

void LobbyWidget::showMain()
{
    m_pages->setCurrentIndex(0);
    refreshServers();
}

void LobbyWidget::showHost()
{
    m_pages->setCurrentIndex(1);
}

void LobbyWidget::setHostedServer(bool running, quint16 port, const QString &name)
{
    m_shutdown->setVisible(running);
    m_hostStart->setText(running ? QStringLiteral("Return to Game") : QStringLiteral("Start Server"));
    m_serverName->setEnabled(!running);
    m_port->setEnabled(!running);
    m_maxPlayers->setEnabled(!running);
    m_bots->setEnabled(!running);
    if (running) {
        m_hostStatus->setText(QStringLiteral("Server running on port %1 — %2").arg(port).arg(name));
    } else {
        m_hostStatus->setText(QStringLiteral("Start a local server or practice vs bots"));
    }
}

void LobbyWidget::buildDefaults()
{
    const QStringList hosts = {
        QStringLiteral("server.blocktrix.org"),
        QStringLiteral("linuxiuvat.de"),
        QStringLiteral("tetrinet.cyteen.eu"),
        QStringLiteral("tetrinet.de"),
        QStringLiteral("tetrinet.fr"),
        QStringLiteral("tetrinet.headcrab.party"),
        QStringLiteral("tetrinet.laber.fasel.org"),
    };
    m_servers.clear();
    for (const QString &host : hosts) {
        Entry e;
        e.name = host;
        e.host = host;
        e.port = 31457;
        e.listing = true;
        m_servers.push_back(e);
    }
}

void LobbyWidget::loadCustom()
{
    const QString path = serversFilePath();
    QFile file(path);
    if (!file.exists()) {
        QSettings s;
        const QStringList rows = s.value(QStringLiteral("customServers")).toStringList();
        for (const QString &row : rows) {
            const QStringList p = row.split(QLatin1Char('|'));
            if (p.size() < 3)
                continue;
            Entry e;
            e.name = p[0];
            e.host = p[1];
            e.port = static_cast<quint16>(p[2].toUShort());
            if (p.size() >= 4)
                e.version = p[3];
            e.custom = true;
            if (findEntry(e.host, e.port) < 0)
                m_servers.push_back(e);
        }
        if (!rows.isEmpty())
            saveCustom();
        return;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
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
        if (p.size() < 3)
            continue;
        Entry e;
        e.name = p[0];
        e.host = p[1];
        e.port = static_cast<quint16>(p[2].toUShort());
        if (p.size() >= 4)
            e.version = p[3];
        e.custom = true;
        const int i = findEntry(e.host, e.port);
        if (i < 0)
            m_servers.push_back(e);
        else {
            m_servers[i].name = e.name;
            m_servers[i].version = e.version;
            m_servers[i].custom = true;
        }
    }
}

void LobbyWidget::saveCustom() const
{
    const QString path = serversFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return;
    QTextStream out(&file);
    out << QStringLiteral("name,host,port,version\n");
    for (const auto &e : m_servers) {
        if (!e.custom)
            continue;
        out << csvEscape(e.name) << QLatin1Char(',') << csvEscape(e.host) << QLatin1Char(',')
            << e.port << QLatin1Char(',') << csvEscape(e.version) << QLatin1Char('\n');
    }
}

int LobbyWidget::findEntry(const QString &host, quint16 port) const
{
    for (int i = 0; i < m_servers.size(); ++i) {
        if (m_servers[i].host.compare(host, Qt::CaseInsensitive) == 0 && m_servers[i].port == port)
            return i;
    }
    return -1;
}

void LobbyWidget::rebuildTable()
{
    QString selected = keyOf(m_host->text().trimmed(), static_cast<quint16>(m_port->value()));
    if (m_table->currentRow() >= 0) {
        if (auto *item = m_table->item(m_table->currentRow(), 1))
            selected = item->text();
    }

    m_table->setRowCount(m_servers.size());
    int selectRow = 0;
    for (int i = 0; i < m_servers.size(); ++i) {
        const auto &e = m_servers[i];
        const QString addr = keyOf(e.host, e.port);
        QString players = QStringLiteral("—");
        if (e.online && e.players >= 0)
            players = e.listing ? QString::number(e.players)
                                : QStringLiteral("%1 / %2").arg(e.players).arg(e.maxPlayers);
        QString status = QStringLiteral("Offline");
        if (isBlocktrix(e))
            status = QStringLiteral("Unsupported");
        else if (e.online)
            status = e.playing ? QStringLiteral("In game") : QStringLiteral("Online");
        else if (e.lan)
            status = QStringLiteral("LAN");
        auto *name = new QTableWidgetItem(e.name);
        auto *address = new QTableWidgetItem(addr);
        auto *ver = new QTableWidgetItem(e.version.isEmpty() ? QStringLiteral("—") : e.version);
        auto *pl = new QTableWidgetItem(players);
        auto *st = new QTableWidgetItem(status);
        pl->setTextAlignment(Qt::AlignCenter);
        st->setTextAlignment(Qt::AlignCenter);
        const auto flags = isBlocktrix(e)
            ? (Qt::ItemIsEnabled)
            : (Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        const QColor dim(120, 120, 140);
        for (auto *it : {name, address, ver, pl, st}) {
            if (isBlocktrix(e)) {
                it->setFlags(Qt::NoItemFlags);
                it->setForeground(dim);
            } else {
                it->setFlags(flags);
            }
        }
        m_table->setItem(i, 0, name);
        m_table->setItem(i, 1, address);
        m_table->setItem(i, 2, ver);
        m_table->setItem(i, 3, pl);
        m_table->setItem(i, 4, st);
        if (!isBlocktrix(e) && addr.compare(selected, Qt::CaseInsensitive) == 0)
            selectRow = i;
        if (isBlocktrix(e) && selectRow == i)
            selectRow = -1;
    }
    if (selectRow < 0) {
        for (int i = 0; i < m_servers.size(); ++i) {
            if (!isBlocktrix(m_servers[i])) {
                selectRow = i;
                break;
            }
        }
    }
    if (selectRow >= 0)
        m_table->selectRow(selectRow);
}

// Set after the listing feed reports a Blocktrix version string.
bool LobbyWidget::isBlocktrix(const Entry &e) const
{
    return e.version.contains(QLatin1String("blocktrix"), Qt::CaseInsensitive);
}

void LobbyWidget::tryJoin()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_servers.size() || isBlocktrix(m_servers[row]))
        return;
    applySelection();
    emit joinClicked();
}

void LobbyWidget::applySelection()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_servers.size())
        return;
    const auto &e = m_servers[row];
    if (isBlocktrix(e))
        return;
    m_host->setText(e.host);
    m_port->setValue(e.port);
    if (e.online && e.maxPlayers >= 2)
        m_maxPlayers->setValue(e.maxPlayers);
}

void LobbyWidget::refreshServers()
{
    fetchListing();
    qDeleteAll(m_queries);
    m_queries.clear();
    for (int i = 0; i < m_servers.size(); ++i) {
        if (m_servers[i].listing)
            continue;
        auto *q = new StatusQuery(this);
        m_queries.push_back(q);
        connect(q, &StatusQuery::finished, this, &LobbyWidget::onStatus);
        q->start(m_servers[i].host, m_servers[i].port);
    }
}

void LobbyWidget::fetchListing()
{
    if (m_listingReply) {
        disconnect(m_listingReply, nullptr, this, nullptr);
        m_listingReply->abort();
        m_listingReply->deleteLater();
        m_listingReply = nullptr;
    }
    const QUrl url(QStringLiteral("https://servers.tetrinet.fr/servers.xml"));
    m_listingReply = m_http.get(QNetworkRequest(url));
    connect(m_listingReply, &QNetworkReply::finished, this, &LobbyWidget::onListingReply);
}

void LobbyWidget::onListingReply()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;
    if (reply == m_listingReply)
        m_listingReply = nullptr;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError)
        return;

    QSet<QString> seen;
    QXmlStreamReader xml(reply);
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement() || xml.name() != QLatin1String("server"))
            continue;
        const auto attrs = xml.attributes();
        const QString host = attrs.value(QLatin1String("name")).toString();
        if (host.isEmpty())
            continue;
        seen.insert(host.toLower());
        const int players = attrs.value(QLatin1String("players")).toInt();
        const QString version = attrs.value(QLatin1String("version")).toString();
        int i = findEntry(host, 31457);
        if (i < 0)
            i = findEntry(host, 30777);
        if (i < 0) {
            Entry e;
            e.name = host;
            e.host = host;
            e.port = 31457;
            e.listing = true;
            m_servers.push_back(e);
            i = m_servers.size() - 1;
        }
        auto &e = m_servers[i];
        e.listing = true;
        e.online = true;
        e.players = players;
        e.playing = players > 0;
        if (!version.isEmpty())
            e.version = version;
    }
    for (auto &e : m_servers) {
        if (e.listing && !seen.contains(e.host.toLower())) {
            e.online = false;
            e.players = -1;
            e.playing = false;
        }
    }
    rebuildTable();
}

void LobbyWidget::onStatus(const QString &host, quint16 port, bool ok, const QString &name,
                           int players, int maxPlayers, bool playing)
{
    const int i = findEntry(host, port);
    if (i < 0)
        return;
    auto &e = m_servers[i];
    e.online = ok;
    if (ok) {
        if (!name.isEmpty())
            e.name = name;
        e.players = players;
        e.maxPlayers = maxPlayers > 0 ? maxPlayers : 6;
        e.playing = playing;
    } else {
        e.players = -1;
        e.playing = false;
    }
    rebuildTable();
}

void LobbyWidget::onLanDatagram()
{
    while (m_lan.hasPendingDatagrams()) {
        const QNetworkDatagram dg = m_lan.receiveDatagram();
        const QString line = QString::fromUtf8(dg.data()).trimmed();
        const QStringList p = line.split(QLatin1Char(' '));
        if (p.size() < 6 || p[0] != QLatin1String("TNET"))
            continue;
        const quint16 port = static_cast<quint16>(p[1].toUShort());
        const int players = p[2].toInt();
        const int maxPlayers = p[3].toInt();
        const bool playing = p[4].toInt() != 0;
        const QString name = QStringList(p.mid(5)).join(QLatin1Char(' '));
        upsertLan(dg.senderAddress().toString(), port, name, players, maxPlayers, playing);
    }
}

void LobbyWidget::upsertLan(const QString &host, quint16 port, const QString &name, int players,
                            int maxPlayers, bool playing)
{
    QString h = host;
    if (h.startsWith(QLatin1String("::ffff:")))
        h = h.mid(7);
    int i = findEntry(h, port);
    if (i < 0) {
        Entry e;
        e.host = h;
        e.port = port;
        e.lan = true;
        m_servers.push_back(e);
        i = m_servers.size() - 1;
    }
    auto &e = m_servers[i];
    e.name = name.isEmpty() ? QStringLiteral("LAN Server") : name;
    e.players = players;
    e.maxPlayers = maxPlayers > 0 ? maxPlayers : 6;
    e.playing = playing;
    e.online = true;
    e.lan = e.lan || (h != QLatin1String("127.0.0.1"));
    rebuildTable();
}

void LobbyWidget::addServer()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Add server"));
    auto *nameEdit = new QLineEdit;
    nameEdit->setMaxLength(32);
    nameEdit->setPlaceholderText(QStringLiteral("My server"));
    auto *hostEdit = new QLineEdit;
    hostEdit->setPlaceholderText(QStringLiteral("hostname or IP"));
    auto *portEdit = new QSpinBox;
    portEdit->setRange(1, 65535);
    portEdit->setValue(31457);
    auto *verEdit = new QLineEdit;
    verEdit->setPlaceholderText(QStringLiteral("TetriNET 1.13"));
    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Name"), nameEdit);
    form->addRow(QStringLiteral("Host"), hostEdit);
    form->addRow(QStringLiteral("Port"), portEdit);
    form->addRow(QStringLiteral("Version"), verEdit);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    auto *box = new QVBoxLayout(&dlg);
    box->addLayout(form);
    box->addWidget(buttons);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QString host = hostEdit->text().trimmed();
    const QString name = nameEdit->text().trimmed();
    const quint16 port = static_cast<quint16>(portEdit->value());
    if (host.isEmpty())
        return;
    const QString label = name.isEmpty() ? host : name;
    const QString version = verEdit->text().trimmed();
    const int existing = findEntry(host, port);
    if (existing >= 0) {
        m_servers[existing].name = label;
        m_servers[existing].version = version;
        m_servers[existing].custom = true;
    } else {
        Entry e;
        e.name = label;
        e.host = host;
        e.port = port;
        e.version = version;
        e.custom = true;
        m_servers.push_back(e);
    }
    saveCustom();
    rebuildTable();
    refreshServers();
}

void LobbyWidget::removeSelected()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_servers.size())
        return;
    if (!m_servers[row].custom && !m_servers[row].lan)
        return;
    m_servers.removeAt(row);
    saveCustom();
    rebuildTable();
}

QString LobbyWidget::nickname() const
{
    return m_nick->text().trimmed();
}

QString LobbyWidget::serverName() const
{
    return m_serverName->text().trimmed();
}

QString LobbyWidget::selectedServerName() const
{
    const int row = m_table->currentRow();
    if (row >= 0 && row < m_servers.size()) {
        const QString n = m_servers[row].name.trimmed();
        if (!n.isEmpty())
            return n;
    }
    return host();
}

QString LobbyWidget::host() const
{
    return m_host->text().trimmed();
}

quint16 LobbyWidget::port() const
{
    return static_cast<quint16>(m_port->value());
}

int LobbyWidget::botCount() const
{
    return m_bots->value();
}

int LobbyWidget::maxPlayers() const
{
    return m_maxPlayers->value();
}

JoinProtocol LobbyWidget::protocol() const
{
    return static_cast<JoinProtocol>(m_protocol->currentData().toInt());
}

} // namespace tnet
