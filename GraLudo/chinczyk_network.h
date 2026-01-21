#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QVector>
#include <QJsonObject>

class ChinczykSerwer : public QObject
{
    Q_OBJECT
public:
    explicit ChinczykSerwer(QObject* parent = nullptr);

    bool start(quint16 port, int docelowaLiczbaGraczy, const QString& nazwaHosta);
    void stop();

    void ustawDocelowaLiczbeGraczy(int n); 
    int docelowaLiczbaGraczy() const { return m_docelowaLiczba; }

    int liczbaKlientow() const { return m_socketNaSlot.size(); }
    int liczbaPolaczonychGraczy() const; 
    bool czyPelny() const;

    void wyslijDoWszystkich(const QJsonObject& msg);
    void wyslijDoKlienta(int slot, const QJsonObject& msg);

signals:
    void log(const QString& s);
    void lobbyZmienione(const QJsonObject& lobby);
    void wiadomoscOdebrana(int slot, const QJsonObject& msg);
    void klientDolaczyl(int slot);
    void klientOdszedl(int slot);

private slots:
    void onNowePolaczenie();

private:
    void podlaczSocket(QTcpSocket* s);
    void onReadyRead(QTcpSocket* s);
    void onDisconnected(QTcpSocket* s);

    void obsluzLinie(QTcpSocket* s, const QByteArray& linia);

    int przydzielSlot() const;
    int kolorDlaSlot(int slot) const;
    QJsonObject zbudujLobbyJson() const;
    void emitLobby();

private:
    QTcpServer m_serwer;

    int m_docelowaLiczba = 4;
    QString m_nazwaHosta = "Host";
    QHash<QTcpSocket*, int> m_socketNaSlot;
    QHash<int, QTcpSocket*> m_slotNaSocket;
    QHash<QTcpSocket*, QByteArray> m_bufor;
    QHash<int, QString> m_slotNaNazwe;
};

class ChinczykKlient : public QObject
{
    Q_OBJECT
public:
    explicit ChinczykKlient(QObject* parent = nullptr);

    void polacz(const QString& ip, quint16 port, const QString& nazwaGracza);
    void rozlacz();
    void wyslij(const QJsonObject& msg);

signals:
    void log(const QString& s);
    void polaczono();
    void rozlaczono();
    void wiadomoscOdebrana(const QJsonObject& msg);

private slots:
    void onConnected();
    void onReadyRead();
    void onDisconnected();

private:
    QTcpSocket m_socket;
    QByteArray m_bufor;
    QString m_nazwa;
};

