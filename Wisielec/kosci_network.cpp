#include "kosci_network.h"
#include <QJsonDocument>

SiecManager::SiecManager(QObject* parent) : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &SiecManager::naNowePolaczenie);
    connect(&m_socketKlienta, &QTcpSocket::connected, this, &SiecManager::polaczono);
    connect(&m_socketKlienta, &QTcpSocket::readyRead, this, &SiecManager::naDane);
    connect(&m_socketKlienta, &QTcpSocket::errorOccurred, this, &SiecManager::naBlad);
}

bool SiecManager::startSerwer(quint16 port)
{
    m_jestemHostem = true;
    return m_server.listen(QHostAddress::Any, port);
}

void SiecManager::startKlient(QString ip, quint16 port)
{
    m_jestemHostem = false;
    m_socketKlienta.connectToHost(ip, port);
}

void SiecManager::wyslij(QTcpSocket* s, QJsonObject json)
{
    if(s && s->state() == QAbstractSocket::ConnectedState)
    {
        s->write(QJsonDocument(json).toJson(QJsonDocument::Compact) + "\n");
    }
}

void SiecManager::wyslijDoHosta(QJsonObject json)
{
    wyslij(&m_socketKlienta, json);
}

void SiecManager::wyslijDoKlienta(QJsonObject json)
{
    for(auto* k : m_klienciHosta) wyslij(k, json);
}

void SiecManager::naNowePolaczenie()
{
    while(m_server.hasPendingConnections())
    {
        auto* s = m_server.nextPendingConnection();
        m_klienciHosta.append(s);
        connect(s, &QTcpSocket::readyRead, this, &SiecManager::naDane);
        connect(s, &QTcpSocket::disconnected, [this, s](){ m_klienciHosta.removeOne(s); s->deleteLater(); });
        emit log("Klient dołączył!");
    }
}

void SiecManager::naDane()
{
    QTcpSocket* s = qobject_cast<QTcpSocket*>(sender());
    if(!s) return;
    
    while(s->canReadLine())
    {
        QByteArray line = s->readLine();
        QJsonDocument doc = QJsonDocument::fromJson(line);
        if(doc.isObject()) emit wiadomoscOdebrana(doc.object());
    }
}

void SiecManager::naBlad(QAbstractSocket::SocketError)
{
    emit log("Błąd sieci: " + m_socketKlienta.errorString());
}
