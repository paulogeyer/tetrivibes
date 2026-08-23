#include "ClassicSession.h"

#include "util/Text.h"

#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTimer>
#include <algorithm>

namespace tnet {

// Servers wrap names in list markup: "#Pure20]" or "(#Accueil)".
static QString sanitizeChannel(QString name)
{
    name = name.trimmed();
    while (!name.isEmpty()
           && (name.endsWith(QLatin1Char(']')) || name.endsWith(QLatin1Char(')'))
               || name.endsWith(QLatin1Char('[')) || name.endsWith(QLatin1Char('('))
               || name.endsWith(QLatin1Char(':')) || name.endsWith(QLatin1Char(','))
               || name.endsWith(QLatin1Char('.'))))
        name.chop(1);
    return name;
}

// Reject tokens parsed from help text ("Type /join #channelname") or English filler.
static bool isRealChannel(const QString &name)
{
    if (name.size() < 2 || !name.startsWith(QLatin1Char('#')))
        return false;
    const QString body = name.mid(1);
    static const QStringList kJunk = {QStringLiteral("existing"), QStringLiteral("channel"),
                                      QStringLiteral("channelname"), QStringLiteral("the")};
    for (const QString &j : kJunk) {
        if (body.compare(j, Qt::CaseInsensitive) == 0)
            return false;
    }
    return true;
}

ClassicSession::ClassicSession(const QString &host, quint16 port, const QString &nick,
                               JoinProtocol proto, const QString &serverLabel, QObject *parent)
    : GameSession(parent)
    , m_host(host)
    , m_label(serverLabel.isEmpty() ? host : serverLabel)
    , m_nick(nick.isEmpty() ? QStringLiteral("Player") : nick)
    , m_port(port)
    , m_proto(proto)
{
    connect(&m_engine, &Engine::updated, this, [this]() {
        emit updated();
        if (m_playing)
            broadcastField();
    });
    connect(&m_engine, &Engine::inventoryChanged, this, &GameSession::updated);
    connect(&m_engine, &Engine::died, this, [this]() {
        if (m_playing) {
            m_alive[static_cast<size_t>(std::max(0, m_client.localSlot()))] = false;
            m_client.sendLose();
        }
        emit updated();
    });

    connect(&m_client, &ClassicClient::welcomed, this, [this](int) {
        if (m_didHello)
            return;
        m_didHello = true;
        emit chatReceived(QStringLiteral("* Joined TetriNET server"));
        m_titleReady = false;
        emit statusChanged();
        emit updated();
        QTimer::singleShot(400, this, [this]() {
            if (!m_client.isConnected() || !m_pendingChannel.isEmpty())
                return;
            m_silentList = true;
            m_listing = true;
            m_listWait = 0;
            m_listed.clear();
            m_client.sendChat(QStringLiteral("/list"));
            m_client.sendChat(QStringLiteral("/who"));
        });
    });
    connect(&m_client, &ClassicClient::disconnected, this, [this]() {
        m_playing = false;
        emit chatReceived(QStringLiteral("* Disconnected"));
        emit statusChanged();
    });
    connect(&m_client, &ClassicClient::errorText, this, [this](const QString &e) {
        emit chatReceived(QStringLiteral("* Error: %1").arg(e));
    });
    connect(&m_client, &ClassicClient::chatReceived, this, [this](int slot, const QString &text) {
        const QString visible = printable(text);
        if (slot < 0) {
            if (m_listing)
                ingestListLine(text);
            if (!looksLikeListLine(visible)) {
                noteChannel(text);
                noteTopic(text);
            }
            noteWhoLine(visible);
            if (!m_pendingChannel.isEmpty()
                && visible.contains(m_pendingChannel, Qt::CaseInsensitive))
                finishJoin();
        }
        if (visible.contains(QLatin1String("Invalid /COMMAND"), Qt::CaseInsensitive))
            return;
        if (m_listing && slot < 0 && looksLikeListLine(visible))
            return;
        const QString who = slot >= 0 ? playerName(slot) : QStringLiteral("server");
        emit chatReceived(QStringLiteral("%1: %2").arg(who, visible));
    });
    connect(&m_client, &ClassicClient::playerUpdated, this, [this](int, const QString &) {
        emit updated();
        emit statusChanged();
    });
    connect(&m_client, &ClassicClient::playerLeft, this, [this](int slot) {
        m_alive[static_cast<size_t>(slot)] = false;
        emit chatReceived(QStringLiteral("* Player left"));
        emit updated();
    });
    connect(&m_client, &ClassicClient::gameStarted, this, &ClassicSession::onGameStarted);
    connect(&m_client, &ClassicClient::gameEnded, this, [this]() {
        m_playing = false;
        emit chatReceived(QStringLiteral("* Game ended"));
        emit statusChanged();
        emit updated();
    });
    connect(&m_client, &ClassicClient::fieldReceived, this, [this](int slot, const Field &field) {
        if (slot >= 0 && slot < kMaxPlayers)
            m_fields[static_cast<size_t>(slot)] = field;
        emit updated();
    });
    connect(&m_client, &ClassicClient::specialReceived, this, &ClassicSession::onSpecial);
    connect(&m_client, &ClassicClient::playerLost, this, [this](int slot) {
        if (slot >= 0 && slot < kMaxPlayers)
            m_alive[static_cast<size_t>(slot)] = false;
        emit chatReceived(QStringLiteral("* %1 topped out").arg(playerName(slot)));
        emit updated();
    });
    connect(&m_client, &ClassicClient::playerWon, this, [this](int slot) {
        m_playing = false;
        emit gameEnded(QStringLiteral("%1 wins!").arg(playerName(slot)));
        emit statusChanged();
    });
    connect(&m_client, &ClassicClient::paused, this, [this](bool on) {
        m_engine.setPaused(on);
        emit chatReceived(on ? QStringLiteral("* Paused") : QStringLiteral("* Resumed"));
    });
    connect(&m_client, &ClassicClient::winlist, this, [this](const QString &text) {
        emit chatReceived(QStringLiteral("* Winlist: %1").arg(text));
    });
}

ClassicSession::~ClassicSession()
{
    QObject::disconnect(&m_client, nullptr, this, nullptr);
    m_client.disconnectFromHost();
}

void ClassicSession::begin()
{
    emit chatReceived(QStringLiteral("* Connecting to %1").arg(m_label));
    m_client.connectTo(m_host, m_port, m_nick, m_proto);
}

Engine *ClassicSession::localEngine()
{
    return &m_engine;
}

int ClassicSession::localSlot() const
{
    return m_client.localSlot() < 0 ? 0 : m_client.localSlot();
}

Field ClassicSession::opponentField(int slot) const
{
    if (slot == localSlot())
        return m_engine.snapshot(true);
    if (slot < 0 || slot >= kMaxPlayers)
        return {};
    return m_fields[static_cast<size_t>(slot)];
}

QString ClassicSession::playerName(int slot) const
{
    const QString n = m_client.playerName(slot);
    if (!n.isEmpty())
        return n;
    if (slotOccupied(slot))
        return QStringLiteral("P%1").arg(slot + 1);
    return {};
}

bool ClassicSession::slotOccupied(int slot) const
{
    return m_client.slotUsed(slot);
}

bool ClassicSession::slotAlive(int slot) const
{
    return slotOccupied(slot) && m_alive[static_cast<size_t>(slot)];
}

bool ClassicSession::canStart() const
{
    return m_client.isConnected() && !m_playing;
}

QString ClassicSession::statusText() const
{
    if (!m_client.isConnected())
        return QStringLiteral("Connecting to %1…").arg(m_label);
    if (!m_pendingChannel.isEmpty())
        return QStringLiteral("Joining %1…").arg(m_pendingChannel);
    if (!m_titleReady)
        return QStringLiteral("Connecting to %1…").arg(m_label);
    if (!m_playing) {
        if (m_channel.isEmpty())
            return m_label;
        if (m_topic.isEmpty())
            return m_channel;
        return QStringLiteral("%1: %2").arg(m_channel, m_topic);
    }
    QString stats = QStringLiteral("L%1  %2 pts  %3 lines")
                        .arg(m_engine.level())
                        .arg(m_engine.score())
                        .arg(m_engine.lines());
    if (m_channel.isEmpty())
        return stats;
    return QStringLiteral("%1  —  %2").arg(m_channel, stats);
}

void ClassicSession::startGame()
{
    m_client.sendStart();
}

void ClassicSession::requestChannels()
{
    m_listing = true;
    m_listWait = 0;
    m_listed.clear();
    m_client.sendChat(QStringLiteral("/list"));
}

void ClassicSession::ingestListLine(const QString &text)
{
    const QString clean = printable(text);
    const int hash = clean.indexOf(QLatin1Char('#'));
    if (hash < 0)
        return;
    QString rest = clean.mid(hash);
    const QString rawName = rest.section(QRegularExpression(QStringLiteral("[\\s\\t]+")), 0, 0);
    QString name = sanitizeChannel(rawName);
    if (name.size() < 2)
        return;
    if (name.compare(QLatin1String("#channelname"), Qt::CaseInsensitive) == 0
        || clean.contains(QLatin1String("/join"), Qt::CaseInsensitive)
        || clean.contains(QLatin1String("lister"), Qt::CaseInsensitive))
        return;

    ChannelInfo info;
    info.name = name;
    const QRegularExpression occ(QStringLiteral("(\\d+)\\s*/\\s*(\\d+)"));
    const auto om = occ.match(clean);
    if (om.hasMatch())
        info.players = QStringLiteral("%1/%2").arg(om.captured(1), om.captured(2));
    if (clean.contains(QLatin1String("FULL"), Qt::CaseInsensitive))
        info.status = QStringLiteral("Full");
    else if (clean.contains(QLatin1String("INGAME"), Qt::CaseInsensitive))
        info.status = QStringLiteral("In game");
    else if (clean.contains(QLatin1String("OPEN"), Qt::CaseInsensitive))
        info.status = QStringLiteral("Open");
    QString desc = rest.mid(rawName.size());
    desc.replace(QRegularExpression(QStringLiteral("\\[[^\\]]*\\]")), QString());
    desc.replace(QRegularExpression(QStringLiteral("\\{\\w+\\}")), QString());
    desc.replace(QRegularExpression(QStringLiteral("\\(\\d+\\s*/\\s*\\d+\\)")), QString());
    desc.replace(QRegularExpression(QStringLiteral("\\(\\d+\\)")), QString());
    desc.replace(QRegularExpression(QStringLiteral("^\\d+\\s*/\\s*\\d+\\s*")), QString());
    desc = desc.trimmed();
    while (desc.startsWith(QLatin1Char(']')) || desc.startsWith(QLatin1Char(')'))
           || desc.startsWith(QLatin1Char('-')) || desc.startsWith(QLatin1Char(':')))
        desc = desc.mid(1).trimmed();
    if (desc.compare(m_nick, Qt::CaseInsensitive) == 0)
        desc.clear();
    info.description = desc;

    if (info.description.isEmpty() && info.players.isEmpty() && info.status.isEmpty())
        return;

    for (auto &e : m_listed) {
        if (e.name.compare(info.name, Qt::CaseInsensitive) != 0)
            continue;
        if (e.description.isEmpty())
            e.description = info.description;
        if (e.players.isEmpty())
            e.players = info.players;
        if (e.status.isEmpty())
            e.status = info.status;
        if (m_titleReady)
            applyListedDescription();
        return;
    }
    m_listed.push_back(info);
    if (m_titleReady)
        applyListedDescription();
}

void ClassicSession::finishList()
{
    if (!m_listing)
        return;
    const bool revealTitle = m_silentList || !m_titleReady;
    m_listing = false;
    m_listWait = 0;
    m_silentList = false;
    if (revealTitle) {
        if (m_channel.isEmpty() && !m_listed.isEmpty())
            applyChannel(m_listed.front().name);
        applyListedDescription();
        m_titleReady = true;
        emit statusChanged();
    }
    emit channelsReceived(m_listed);
}

void ClassicSession::onGameStarted(int startHeight, int startLevel, bool tetrifast)
{
    m_playing = true;
    m_alive.fill(false);
    for (int i = 0; i < kMaxPlayers; ++i) {
        m_fields[static_cast<size_t>(i)] = Field{};
        if (m_client.slotUsed(i))
            m_alive[static_cast<size_t>(i)] = true;
    }
    m_engine.setStartLevel(startLevel);
    m_engine.setPieceDelay(tetrifast ? 0 : 1000);
    m_engine.reset(QRandomGenerator::global()->generate());
    std::mt19937 rng{QRandomGenerator::global()->generate()};
    for (int i = 0; i < startHeight; ++i)
        m_engine.applySpecial(Special::AddLine);
    m_lastLevel = m_engine.level();
    m_lastSent.clear();
    emit chatReceived(QStringLiteral("* Game started"));
    emit statusChanged();
    emit updated();
}

void ClassicSession::useSpecial(int targetSlot)
{
    if (!m_playing || !m_engine.alive() || m_engine.inventory().isEmpty())
        return;
    if (!slotOccupied(targetSlot))
        return;
    const Special s = m_engine.takeSpecial();
    m_client.sendSpecial(targetSlot, s);
}

void ClassicSession::onSpecial(int from, int target, Special special)
{
    emit chatReceived(QStringLiteral("* %1 used %2 on %3")
                          .arg(playerName(from), specialName(special), playerName(target)));
    if (special == Special::SwitchField) {
        if (from == localSlot() && target == localSlot())
            return;
        if (target == localSlot() || from == localSlot()) {
            const int other = (from == localSlot()) ? target : from;
            Field mine = m_engine.field();
            Field theirs = m_fields[static_cast<size_t>(other)];
            m_engine.setField(theirs);
            m_fields[static_cast<size_t>(other)] = mine;
            broadcastField();
        }
        return;
    }
    if (target == localSlot() || target < 0)
        m_engine.applySpecial(special);
}

void ClassicSession::sendChat(const QString &text)
{
    const QString t = text.trimmed();
    if (t.startsWith(QLatin1String("/join "), Qt::CaseInsensitive)
        || t.startsWith(QLatin1String("/j "), Qt::CaseInsensitive)) {
        const int sp = t.indexOf(QLatin1Char(' '));
        QString ch = t.mid(sp + 1).trimmed().split(QLatin1Char(' ')).value(0);
        if (!ch.isEmpty()) {
            if (!ch.startsWith(QLatin1Char('#')))
                ch.prepend(QLatin1Char('#'));
            m_pendingChannel = sanitizeChannel(ch);
            m_pendingDesc = listedDescription(m_pendingChannel);
            emit statusChanged();
            QTimer::singleShot(700, this, [this]() { finishJoin(); });
        }
    }
    m_client.sendChat(text);
}

void ClassicSession::noteChannel(const QString &text)
{
    static const QRegularExpression re(QStringLiteral("#([^\\s.,!;)\\]]+)"));
    QRegularExpressionMatchIterator it = re.globalMatch(text);
    while (it.hasNext()) {
        const QString ch = sanitizeChannel(QLatin1Char('#') + it.next().captured(1));
        if (!isRealChannel(ch))
            continue;
        const QString lower = text.toLower();
        if (!lower.contains(QLatin1String("join"))
            && !lower.contains(QLatin1String("enter"))
            && !lower.contains(QLatin1String("now in"))
            && !lower.contains(QLatin1String("you are"))
            && !lower.contains(QLatin1String("welcome")))
            continue;
        if (!m_pendingChannel.isEmpty()
            && ch.compare(m_pendingChannel, Qt::CaseInsensitive) != 0)
            continue;
        applyChannel(!m_pendingChannel.isEmpty() ? m_pendingChannel : ch);
        return;
    }
}

void ClassicSession::noteWhoLine(const QString &text)
{
    if (m_nick.isEmpty() || !text.contains(m_nick, Qt::CaseInsensitive))
        return;
    static const QRegularExpression re(QStringLiteral("#([^\\s.,!;)\\]]+)"));
    const auto m = re.match(text);
    if (!m.hasMatch())
        return;
    const QString ch = sanitizeChannel(QLatin1Char('#') + m.captured(1));
    if (!isRealChannel(ch))
        return;
    if (!m_pendingChannel.isEmpty()
        && ch.compare(m_pendingChannel, Qt::CaseInsensitive) != 0)
        return;
    applyChannel(ch);
}

void ClassicSession::finishJoin()
{
    if (m_pendingChannel.isEmpty())
        return;
    applyChannel(m_pendingChannel);
}

bool ClassicSession::looksLikeListLine(const QString &text) const
{
    const QString lower = text.toLower();
    if (lower.contains(QLatin1String("lister")) || lower.contains(QLatin1String("/join")))
        return true;
    if (lower.contains(QLatin1String("open")) || lower.contains(QLatin1String("full"))
        || lower.contains(QLatin1String("ingame")))
        return true;
    if (text.contains(QLatin1Char('#'))
        && (text.contains(QLatin1Char('/')) || text.contains(QLatin1Char('['))))
        return true;
    if (text.contains(m_nick, Qt::CaseInsensitive) && text.contains(QLatin1Char('#')))
        return true;
    return false;
}

void ClassicSession::applyChannel(const QString &name)
{
    const QString ch = sanitizeChannel(name);
    if (!isRealChannel(ch))
        return;
    QString desc = listedDescription(ch);
    if (desc.isEmpty() && ch.compare(m_pendingChannel, Qt::CaseInsensitive) == 0)
        desc = m_pendingDesc;
    if (m_channel == ch && desc == m_topic && m_pendingChannel.isEmpty())
        return;
    m_channel = ch;
    if (!desc.isEmpty())
        m_topic = desc;
    m_pendingChannel.clear();
    m_pendingDesc.clear();
    emit statusChanged();
}

QString ClassicSession::listedDescription(const QString &name) const
{
    for (const auto &e : m_listed) {
        if (e.name.compare(name, Qt::CaseInsensitive) == 0 && !e.description.isEmpty())
            return e.description;
    }
    return {};
}

void ClassicSession::applyListedDescription()
{
    if (m_channel.isEmpty())
        return;
    const QString desc = listedDescription(m_channel);
    if (desc.isEmpty() || desc == m_topic)
        return;
    m_topic = desc;
    emit statusChanged();
}

void ClassicSession::noteTopic(const QString &text)
{
    const QString clean = printable(text);
    static const QRegularExpression re(
        QStringLiteral("(?:topic\\s+(?:for\\s+#\\S+\\s+)?is\\s*:?\\s+|topic to\\s+)(.+)"),
        QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(clean);
    if (!m.hasMatch())
        return;
    const QString topic = m.captured(1).trimmed();
    if (topic.isEmpty() || topic == m_topic)
        return;
    m_topic = topic;
    emit statusChanged();
}

void ClassicSession::broadcastField()
{
    // The same hidden-game trick can cross an unmodified TetriNET Classic server by treating
    // its regular `f` field packets as a tiny 12x22 framebuffer. Classic relays are normally
    // paced much slower (this client sends every 250 ms), so that experiment belongs behind a
    // separate opt-in transport rather than changing the native real-time MVP.
    const QString data = m_engine.snapshot(false).encode();
    if (data == m_lastSent)
        return;
    m_lastSent = data;
    m_client.sendField(m_engine.snapshot(false));
}

void ClassicSession::tick(int ms)
{
    if (m_listing) {
        m_listWait += ms;
        if (m_listWait >= 2500)
            finishList();
    }
    if (!m_playing || !m_engine.alive())
        return;
    m_engine.tick(ms);
    if (m_engine.level() != m_lastLevel) {
        m_lastLevel = m_engine.level();
        m_client.sendLevel(m_lastLevel);
    }
    m_fieldAcc += ms;
    if (m_fieldAcc >= 250) {
        m_fieldAcc = 0;
        broadcastField();
    }
    emit statusChanged();
}

} // namespace tnet
