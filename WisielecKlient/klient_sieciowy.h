#pragma once

#include <QObject>
#include <QWebSocket>
#include <QJsonObject>
#include <QAbstractSocket>

class KlientSieciowy : public QObject
{
    Q_OBJECT

public:
    explicit KlientSieciowy(QObject *parent = nullptr);

    void polacz(const QUrl &url);
    void rozlacz();
    void wyslij(const QJsonObject &obj);

    bool czyPolaczony() const;

signals:
    void polaczono();
    void rozlaczono();
    void blad(const QString &tekst);
    void odebrano(const QJsonObject &obj);

private slots:
    void onPolaczono();
    void onRozlaczono();
    void onTekst(const QString &msg);
    void onBlad(QAbstractSocket::SocketError err);

private:
    QWebSocket socket;
};
