#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>

namespace tnet {

// One-shot TCP QUERY to a native host; used by the lobby player-count refresh.
class StatusQuery : public QObject {
    Q_OBJECT
public:
    explicit StatusQuery(QObject *parent = nullptr);

    void start(const QString &host, quint16 port);

signals:
    void finished(const QString &host, quint16 port, bool ok, const QString &name, int players,
                  int maxPlayers, bool playing);

private:
    void finish(bool ok, const QString &name, int players, int maxPlayers, bool playing);

    QTcpSocket m_socket;
    QTimer m_timer;
    QByteArray m_buffer;
    QString m_host;
    quint16 m_port = 0;
    bool m_done = false;
};

} // namespace tnet
