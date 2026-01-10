#pragma once

#include <QObject>
#include <QMap>
#include <QSet>
#include <QString>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonObject>

class SerwerWisielec : public QObject
{
    Q_OBJECT
public:
    explicit SerwerWisielec(quint16 port, QObject *parent = nullptr);

private slots:
    void onNowePolaczenie();
    void onTekstOdebrany(const QString &wiadomosc);
    void onRozlaczono();

private:
    struct StanPokoju
    {
        QString nazwa;
        QList<QWebSocket*> klienci;
        QWebSocket* ustawiacz = nullptr;
        QWebSocket* zgadujacy = nullptr;

        bool aktywnaGra = false;
        QString slowo;
        QSet<QChar> uzyte;
        int bledy = 0;
        int maxBledow = 8;
    };

    QWebSocketServer serwer;

    QMap<QString, StanPokoju> pokoje;
    QMap<QWebSocket*, QString> klientPokoj;
    QMap<QWebSocket*, QString> klientNazwa;

    void wyslij(QWebSocket *s, const QJsonObject &obj);
    void broadcast(const QString &pokoj, const QJsonObject &obj);

    void obsluzHello(QWebSocket *s, const QJsonObject &obj);
    void obsluzRoomCreate(QWebSocket *s, const QJsonObject &obj);
    void obsluzRoomJoin(QWebSocket *s, const QJsonObject &obj);
    void obsluzSetWord(QWebSocket *s, const QJsonObject &obj);
    void obsluzGuess(QWebSocket *s, const QJsonObject &obj);

    void resetGry(const QString &pokoj);
    void wyslijStan(const QString &pokoj);
    QString maska(const StanPokoju &s) const;
    bool wygrana(const StanPokoju &s) const;
};
