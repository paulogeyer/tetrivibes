#pragma once

#include "game/Types.h"

#include <QNetworkAccessManager>
#include <QUdpSocket>
#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QNetworkReply;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTableWidget;
class QTimer;

namespace tnet {

class StatusQuery;

// Main menu (server list) and host page. Custom hosts live in ~/.tetrivibes/servers.csv.
class LobbyWidget : public QWidget {
    Q_OBJECT
public:
    explicit LobbyWidget(QWidget *parent = nullptr);

    QString nickname() const;
    QString serverName() const;
    QString selectedServerName() const;
    QString host() const;
    quint16 port() const;
    int botCount() const;
    int maxPlayers() const;
    JoinProtocol protocol() const;
    void refreshServers();
    void showMain();
    void setHostedServer(bool running, quint16 port = 0, const QString &name = QString());

signals:
    void hostClicked();
    void joinClicked();
    void practiceClicked();
    void shutdownClicked();

protected:
    void showEvent(QShowEvent *event) override;

private:
    struct Entry {
        QString name;
        QString host;
        quint16 port = 31457;
        int players = -1;
        int maxPlayers = 6;
        bool online = false;
        bool playing = false;
        bool lan = false;
        bool custom = false;
        bool listing = false;
        QString version;
    };

    void buildDefaults();
    void loadCustom();
    void saveCustom() const;
    void rebuildTable();
    void applySelection();
    void showHost();
    void addServer();
    void removeSelected();
    int findEntry(const QString &host, quint16 port) const;
    bool isBlocktrix(const Entry &e) const;
    void tryJoin();
    void upsertLan(const QString &host, quint16 port, const QString &name, int players,
                   int maxPlayers, bool playing);
    void onLanDatagram();
    void onStatus(const QString &host, quint16 port, bool ok, const QString &name, int players,
                  int maxPlayers, bool playing);
    void fetchListing();
    void onListingReply();

    QLineEdit *m_nick = nullptr;
    QLineEdit *m_serverName = nullptr;
    QLineEdit *m_host = nullptr;
    QSpinBox *m_port = nullptr;
    QSpinBox *m_bots = nullptr;
    QSpinBox *m_maxPlayers = nullptr;
    QComboBox *m_protocol = nullptr;
    QLabel *m_hostStatus = nullptr;
    QPushButton *m_hostStart = nullptr;
    QPushButton *m_shutdown = nullptr;
    QStackedWidget *m_pages = nullptr;
    QTableWidget *m_table = nullptr;
    QVector<Entry> m_servers;
    QVector<StatusQuery *> m_queries;
    QUdpSocket m_lan;
    QTimer *m_refresh = nullptr;
    QNetworkAccessManager m_http;
    QNetworkReply *m_listingReply = nullptr;
};

} // namespace tnet
