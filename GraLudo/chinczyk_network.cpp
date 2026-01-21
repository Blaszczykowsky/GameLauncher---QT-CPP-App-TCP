#include "chinczyk_network.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>

static QByteArray jsonNaLinie(const QJsonObject& o)
{
    return QJsonDocument(o).toJson(QJsonDocument::Compact) + "\n";
}

static bool liniaNaJson(const QByteArray& linia, QJsonObject& out)
{
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(linia, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;
    out = doc.object();
    return true;
}

ChinczykSerwer::ChinczykSerwer(QObject* parent) : QObject(parent)
{
    connect(&m_serwer, &QTcpServer::newConnection, this, &ChinczykSerwer::onNowePolaczenie);
}

bool ChinczykSerwer::start(quint16 port, int docelowaLiczbaGraczy, const QString& nazwaHosta)
{
    stop();

    if (docelowaLiczbaGraczy < 2) docelowaLiczbaGraczy = 2;
    if (docelowaLiczbaGraczy > 4) docelowaLiczbaGraczy = 4;

    m_docelowaLiczba = docelowaLiczbaGraczy;
    m_nazwaHosta = nazwaHosta.isEmpty() ? "Host" : nazwaHosta;
    m_slotNaNazwe.clear();
    m_slotNaNazwe[0] = m_nazwaHosta;

    if (!m_serwer.listen(QHostAddress::Any, port))
    {
        emit log("Serwer: nie moge otworzyc portu.");
        return false;
    }

    emit log("Serwer: nasluch na porcie " + QString::number(port));
    emitLobby();
    return true;
}

void ChinczykSerwer::stop()
{
    for (auto* s : m_socketNaSlot.keys())
    {
        s->disconnect(this);
        s->disconnectFromHost();
        s->deleteLater();
    }

    m_socketNaSlot.clear();
    m_slotNaSocket.clear();
    m_bufor.clear();
    m_slotNaNazwe.clear();

    if (m_serwer.isListening())
        m_serwer.close();
}

void ChinczykSerwer::ustawDocelowaLiczbeGraczy(int n)
{
    if (n < 2) n = 2;
    if (n > 4) n = 4;
    if (liczbaKlientow() > 0)
        return;

    m_docelowaLiczba = n;
    emitLobby();
}

int ChinczykSerwer::liczbaPolaczonychGraczy() const
{
    return 1 + m_socketNaSlot.size();
}

bool ChinczykSerwer::czyPelny() const
{
    return liczbaPolaczonychGraczy() >= m_docelowaLiczba;
}

void ChinczykSerwer::onNowePolaczenie()
{
    while (m_serwer.hasPendingConnections())
    {
        auto* s = m_serwer.nextPendingConnection();
        podlaczSocket(s);
        emit log("Serwer: nowe polaczenie.");
    }
}

void ChinczykSerwer::podlaczSocket(QTcpSocket* s)
{
    connect(s, &QTcpSocket::readyRead, this, [this, s](){ onReadyRead(s); });
    connect(s, &QTcpSocket::disconnected, this, [this, s](){ onDisconnected(s); });
    m_bufor[s] = QByteArray();
}

void ChinczykSerwer::onReadyRead(QTcpSocket* s)
{
    m_bufor[s].append(s->readAll());

    while (true)
    {
        int idx = m_bufor[s].indexOf('\n');
        if (idx < 0) break;

        QByteArray linia = m_bufor[s].left(idx);
        m_bufor[s].remove(0, idx + 1);

        if (!linia.isEmpty())
            obsluzLinie(s, linia);
    }
}

void ChinczykSerwer::onDisconnected(QTcpSocket* s)
{
    int slot = -1;
    if (m_socketNaSlot.contains(s))
        slot = m_socketNaSlot.value(s);

    m_bufor.remove(s);

    if (slot != -1)
    {
        m_socketNaSlot.remove(s);
        m_slotNaSocket.remove(slot);
        m_slotNaNazwe.remove(slot);

        emit log("Serwer: klient rozlaczony (slot " + QString::number(slot) + ").");
        emit klientOdszedl(slot);
        emitLobby();
    }

    s->deleteLater();
}

int ChinczykSerwer::przydzielSlot() const
{
    for (int slot = 1; slot <= m_docelowaLiczba - 1; ++slot)
        if (!m_slotNaSocket.contains(slot))
            return slot;
    return -1;
}

int ChinczykSerwer::kolorDlaSlot(int slot) const
{
    if (m_docelowaLiczba == 2)
    {
        if (slot == 0) return 0;
        return 2; 
    }
    if (m_docelowaLiczba == 3)
    {
        if (slot == 0) return 0;
        if (slot == 1) return 1;
        return 2;
    }
    if (slot == 0) return 0;
    if (slot == 1) return 1;
    if (slot == 2) return 2;
    return 3;
}

QJsonObject ChinczykSerwer::zbudujLobbyJson() const
{
    QJsonObject lobby;
    lobby["t"] = "CH_LOBBY";
    lobby["totalPlayers"] = m_docelowaLiczba;

    QJsonArray players;
    for (int slot = 0; slot <= m_docelowaLiczba - 1; ++slot)
    {
        QJsonObject p;
        p["slot"] = slot;
        p["kolor"] = kolorDlaSlot(slot);
        p["name"] = m_slotNaNazwe.value(slot, "");
        p["connected"] = (slot == 0) ? true : m_slotNaSocket.contains(slot);
        players.append(p);
    }

    lobby["players"] = players;
    return lobby;
}

void ChinczykSerwer::emitLobby()
{
    QJsonObject lobby = zbudujLobbyJson();
    emit lobbyZmienione(lobby);
    wyslijDoWszystkich(lobby);
}

void ChinczykSerwer::wyslijDoWszystkich(const QJsonObject& msg)
{
    QByteArray data = jsonNaLinie(msg);
    for (auto* s : m_socketNaSlot.keys())
        s->write(data);
}

void ChinczykSerwer::wyslijDoKlienta(int slot, const QJsonObject& msg)
{
    if (!m_slotNaSocket.contains(slot))
        return;
    m_slotNaSocket.value(slot)->write(jsonNaLinie(msg));
}

void ChinczykSerwer::obsluzLinie(QTcpSocket* s, const QByteArray& linia)
{
    QJsonObject msg;
    if (!liniaNaJson(linia, msg))
    {
        emit log("Serwer: blad JSON od klienta.");
        return;
    }

    QString t = msg.value("t").toString();

    if (!m_socketNaSlot.contains(s))
    {
        if (t != "CH_HELLO")
        {
            QJsonObject rej;
            rej["t"] = "CH_REJECT";
            rej["reason"] = "Send CH_HELLO first.";
            s->write(jsonNaLinie(rej));
            s->disconnectFromHost();
            return;
        }

        if (czyPelny())
        {
            QJsonObject rej;
            rej["t"] = "CH_REJECT";
            rej["reason"] = "Lobby full.";
            s->write(jsonNaLinie(rej));
            s->disconnectFromHost();
            return;
        }

        int slot = przydzielSlot();
        if (slot == -1)
        {
            QJsonObject rej;
            rej["t"] = "CH_REJECT";
            rej["reason"] = "No free slot.";
            s->write(jsonNaLinie(rej));
            s->disconnectFromHost();
            return;
        }

        QString name = msg.value("name").toString();
        if (name.isEmpty()) name = "Gracz";

        m_socketNaSlot[s] = slot;
        m_slotNaSocket[slot] = s;
        m_slotNaNazwe[slot] = name;

        QJsonObject welcome;
        welcome["t"] = "CH_WELCOME";
        welcome["slot"] = slot;
        welcome["kolor"] = kolorDlaSlot(slot);
        welcome["totalPlayers"] = m_docelowaLiczba;
        welcome["players"] = zbudujLobbyJson().value("players").toArray();
        s->write(jsonNaLinie(welcome));

        emit log("Serwer: klient '" + name + "' -> slot " + QString::number(slot));
        emit klientDolaczyl(slot);
        emitLobby();
        return;
    }

    int slot = m_socketNaSlot.value(s);
    emit wiadomoscOdebrana(slot, msg);
}

ChinczykKlient::ChinczykKlient(QObject* parent) : QObject(parent)
{
    connect(&m_socket, &QTcpSocket::connected, this, &ChinczykKlient::onConnected);
    connect(&m_socket, &QTcpSocket::readyRead, this, &ChinczykKlient::onReadyRead);
    connect(&m_socket, &QTcpSocket::disconnected, this, &ChinczykKlient::onDisconnected);
}

void ChinczykKlient::polacz(const QString& ip, quint16 port, const QString& nazwaGracza)
{
    m_nazwa = nazwaGracza.isEmpty() ? "Gracz" : nazwaGracza;
    m_bufor.clear();

    emit log("Klient: lacze z " + ip + ":" + QString::number(port));
    m_socket.connectToHost(ip, port);
}

void ChinczykKlient::rozlacz()
{
    m_socket.disconnectFromHost();
}

void ChinczykKlient::wyslij(const QJsonObject& msg)
{
    m_socket.write(jsonNaLinie(msg));
}

void ChinczykKlient::onConnected()
{
    emit polaczono();
    emit log("Klient: polaczono.");

    QJsonObject hello;
    hello["t"] = "CH_HELLO";
    hello["name"] = m_nazwa;
    wyslij(hello);
}

void ChinczykKlient::onReadyRead()
{
    m_bufor.append(m_socket.readAll());

    while (true)
    {
        int idx = m_bufor.indexOf('\n');
        if (idx < 0) break;

        QByteArray linia = m_bufor.left(idx);
        m_bufor.remove(0, idx + 1);

        if (linia.isEmpty()) continue;

        QJsonObject msg;
        if (!liniaNaJson(linia, msg))
        {
            emit log("Klient: blad JSON.");
            continue;
        }

        emit wiadomoscOdebrana(msg);
    }
}

void ChinczykKlient::onDisconnected()
{
    emit rozlaczono();
    emit log("Klient: rozlaczono.");
}

