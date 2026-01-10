#include "klient_sieciowy.h"

#include <QJsonDocument>
#include <QJsonParseError>

KlientSieciowy::KlientSieciowy(QObject *parent)
    : QObject(parent)
{
    connect(&socket, &QWebSocket::connected, this, &KlientSieciowy::onPolaczono);
    connect(&socket, &QWebSocket::disconnected, this, &KlientSieciowy::onRozlaczono);
    connect(&socket, &QWebSocket::textMessageReceived, this, &KlientSieciowy::onTekst);
    connect(&socket, &QWebSocket::errorOccurred, this, &KlientSieciowy::onBlad);
}

void KlientSieciowy::polacz(const QUrl &url)
{
    socket.open(url);
}

void KlientSieciowy::rozlacz()
{
    socket.close();
}

bool KlientSieciowy::czyPolaczony() const
{
    return socket.state() == QAbstractSocket::ConnectedState;
}

void KlientSieciowy::wyslij(const QJsonObject &obj)
{
    if(!czyPolaczony()) return;

    QString msg = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    socket.sendTextMessage(msg);
}

void KlientSieciowy::onPolaczono()
{
    emit polaczono();
}

void KlientSieciowy::onRozlaczono()
{
    emit rozlaczono();
}

void KlientSieciowy::onTekst(const QString &msg)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &err);

    if(err.error != QJsonParseError::NoError || !doc.isObject())
    {
        emit blad("Niepoprawny JSON od serwera.");
        return;
    }

    emit odebrano(doc.object());
}

void KlientSieciowy::onBlad(QAbstractSocket::SocketError)
{
    emit blad(socket.errorString());
}
