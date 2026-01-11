#ifndef KOSCI_NETWORK_H
#define KOSCI_NETWORK_H

#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QList>

class SiecManager : public QObject
{
    Q_OBJECT

public:
    explicit SiecManager(QObject* parent = nullptr);

    bool startSerwer(quint16 port);
    void startKlient(QString ip, quint16 port);
    void wyslijDoHosta(QJsonObject json);
    void wyslijDoKlienta(QJsonObject json);

signals:
    void polaczono();
    void wiadomoscOdebrana(QJsonObject json);
    void log(QString msg);

private slots:
    void naNowePolaczenie();
    void naDane();
    void naBlad(QAbstractSocket::SocketError);

private:
    QTcpServer m_server;
    QTcpSocket m_socketKlienta;
    QList<QTcpSocket*> m_klienciHosta;
    bool m_jestemHostem = false;

    void wyslij(QTcpSocket* s, QJsonObject json);
};

#endif // KOSCI_NETWORK_H
