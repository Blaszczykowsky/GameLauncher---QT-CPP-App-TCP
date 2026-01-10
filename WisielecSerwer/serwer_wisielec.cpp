#include "serwer_wisielec.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>
#include <QDebug>

SerwerWisielec::SerwerWisielec(quint16 port, QObject *parent)
    : QObject(parent),
    serwer(QStringLiteral("WisielecServer"), QWebSocketServer::NonSecureMode)
{
    if(!serwer.listen(QHostAddress::Any, port))
    {
        qCritical() << "Nie mozna uruchomic serwera na porcie:" << port;
        return;
    }

    qInfo() << "Serwer dziala na porcie:" << port;
    connect(&serwer, &QWebSocketServer::newConnection, this, &SerwerWisielec::onNowePolaczenie);
}

void SerwerWisielec::onNowePolaczenie()
{
    QWebSocket *socket = serwer.nextPendingConnection();
    qInfo() << "Nowy klient:" << socket;

    connect(socket, &QWebSocket::textMessageReceived, this, &SerwerWisielec::onTekstOdebrany);
    connect(socket, &QWebSocket::disconnected, this, &SerwerWisielec::onRozlaczono);

    QJsonObject obj;
    obj["typ"] = "info";
    obj["wiadomosc"] = "Polaczono. Wyslij hello: {\"typ\":\"hello\",\"nazwa\":\"...\"}.";
    wyslij(socket, obj);
}

void SerwerWisielec::onRozlaczono()
{
    QWebSocket *socket = qobject_cast<QWebSocket*>(sender());
    if(!socket) return;

    QString pokoj = klientPokoj.value(socket, "");
    if(!pokoj.isEmpty() && pokoje.contains(pokoj))
    {
        auto &stan = pokoje[pokoj];

        stan.klienci.removeAll(socket);
        if(stan.ustawiacz == socket) stan.ustawiacz = nullptr;
        if(stan.zgadujacy == socket) stan.zgadujacy = nullptr;

        if(stan.klienci.isEmpty())
        {
            pokoje.remove(pokoj);
            qInfo() << "Usunieto pusty pokoj:" << pokoj;
        }
        else
        {
            stan.ustawiacz = stan.klienci.value(0, nullptr);
            stan.zgadujacy = stan.klienci.value(1, nullptr);

            QJsonObject info;
            info["typ"] = "info";
            info["wiadomosc"] = "Ktos wyszedl. Role zostaly przeliczone.";
            broadcast(pokoj, info);

            resetGry(pokoj);
        }
    }

    klientPokoj.remove(socket);
    klientNazwa.remove(socket);

    socket->deleteLater();
    qInfo() << "Klient rozlaczony.";
}

void SerwerWisielec::onTekstOdebrany(const QString &wiadomosc)
{
    QWebSocket *socket = qobject_cast<QWebSocket*>(sender());
    if(!socket) return;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(wiadomosc.toUtf8(), &err);
    if(err.error != QJsonParseError::NoError || !doc.isObject())
    {
        QJsonObject obj;
        obj["typ"] = "error";
        obj["wiadomosc"] = "Niepoprawny JSON.";
        wyslij(socket, obj);
        return;
    }

    QJsonObject obj = doc.object();
    QString typ = obj.value("typ").toString();

    if(typ == "hello") obsluzHello(socket, obj);
    else if(typ == "room_create") obsluzRoomCreate(socket, obj);
    else if(typ == "room_join") obsluzRoomJoin(socket, obj);
    else if(typ == "set_word") obsluzSetWord(socket, obj);
    else if(typ == "guess") obsluzGuess(socket, obj);
    else
    {
        QJsonObject odp;
        odp["typ"] = "error";
        odp["wiadomosc"] = "Nieznany typ wiadomosci.";
        wyslij(socket, odp);
    }
}

void SerwerWisielec::wyslij(QWebSocket *s, const QJsonObject &obj)
{
    if(!s) return;
    s->sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void SerwerWisielec::broadcast(const QString &pokoj, const QJsonObject &obj)
{
    if(!pokoje.contains(pokoj)) return;

    auto &stan = pokoje[pokoj];
    QString msg = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));

    for(QWebSocket* s : stan.klienci)
        if(s) s->sendTextMessage(msg);
}

void SerwerWisielec::obsluzHello(QWebSocket *s, const QJsonObject &obj)
{
    QString nazwa = obj.value("nazwa").toString().trimmed();
    if(nazwa.isEmpty()) nazwa = "Gracz";

    klientNazwa[s] = nazwa;

    QJsonObject odp;
    odp["typ"] = "hello_ok";
    odp["wiadomosc"] = "Witaj, " + nazwa + ". Utworz pokoj (room_create) lub dolacz (room_join).";
    wyslij(s, odp);
}

void SerwerWisielec::obsluzRoomCreate(QWebSocket *s, const QJsonObject &obj)
{
    QString gra = obj.value("gra").toString().trimmed().toLower();
    QString nazwaPokoju = obj.value("room").toString().trimmed();

    if(gra != "wisielec")
    {
        QJsonObject odp;
        odp["typ"] = "error";
        odp["wiadomosc"] = "Serwer obsluguje tylko gra='wisielec'.";
        wyslij(s, odp);
        return;
    }

    if(nazwaPokoju.isEmpty())
    {
        QJsonObject odp;
        odp["typ"] = "error";
        odp["wiadomosc"] = "Brak nazwy pokoju.";
        wyslij(s, odp);
        return;
    }

    if(pokoje.contains(nazwaPokoju))
    {
        QJsonObject odp;
        odp["typ"] = "error";
        odp["wiadomosc"] = "Pokoj juz istnieje.";
        wyslij(s, odp);
        return;
    }

    StanPokoju stan;
    stan.nazwa = nazwaPokoju;
    stan.klienci.append(s);
    stan.ustawiacz = s;
    stan.zgadujacy = nullptr;
    stan.aktywnaGra = false;

    pokoje.insert(nazwaPokoju, stan);
    klientPokoj[s] = nazwaPokoju;

    QJsonObject odp;
    odp["typ"] = "room_ok";
    odp["room"] = nazwaPokoju;
    odp["wiadomosc"] = "Utworzono pokoj i dolaczono.";
    wyslij(s, odp);

    resetGry(nazwaPokoju);
}

void SerwerWisielec::obsluzRoomJoin(QWebSocket *s, const QJsonObject &obj)
{
    QString nazwaPokoju = obj.value("room").toString().trimmed();
    if(nazwaPokoju.isEmpty() || !pokoje.contains(nazwaPokoju))
    {
        QJsonObject odp;
        odp["typ"] = "error";
        odp["wiadomosc"] = "Nie ma takiego pokoju.";
        wyslij(s, odp);
        return;
    }

    QString stary = klientPokoj.value(s, "");
    if(!stary.isEmpty() && pokoje.contains(stary))
    {
        auto &sp = pokoje[stary];
        sp.klienci.removeAll(s);
        if(sp.ustawiacz == s) sp.ustawiacz = nullptr;
        if(sp.zgadujacy == s) sp.zgadujacy = nullptr;
        if(sp.klienci.isEmpty()) pokoje.remove(stary);
    }

    auto &stan = pokoje[nazwaPokoju];
    if(!stan.klienci.contains(s))
        stan.klienci.append(s);

    klientPokoj[s] = nazwaPokoju;

    if(!stan.zgadujacy && s != stan.ustawiacz)
        stan.zgadujacy = s;

    QJsonObject odp;
    odp["typ"] = "room_ok";
    odp["room"] = nazwaPokoju;
    odp["wiadomosc"] = "Dolaczono do pokoju.";
    wyslij(s, odp);

    resetGry(nazwaPokoju);
}

void SerwerWisielec::resetGry(const QString &pokoj)
{
    if(!pokoje.contains(pokoj)) return;
    auto &stan = pokoje[pokoj];

    stan.aktywnaGra = false;
    stan.slowo.clear();
    stan.uzyte.clear();
    stan.bledy = 0;
    stan.maxBledow = 8;
    if(!stan.ustawiacz && !stan.klienci.isEmpty())
        stan.ustawiacz = stan.klienci.value(0, nullptr);

    if(!stan.zgadujacy)
    {
        for(QWebSocket *s : stan.klienci)
        {
            if(s && s != stan.ustawiacz)
            {
                stan.zgadujacy = s;
                break;
            }
        }
    }
    for(QWebSocket* s : stan.klienci)
    {
        QJsonObject r;
        r["typ"] = "role";
        if(s == stan.ustawiacz) r["rola"] = "ustawiacz";
        else if(s == stan.zgadujacy) r["rola"] = "zgadujacy";
        else r["rola"] = "obserwator";
        wyslij(s, r);
    }
    if(stan.ustawiacz)
    {
        QJsonObject info;
        info["typ"] = "info";
        info["wiadomosc"] = "Twoja rola: ustawiacz. Ustaw slowo (set_word).";
        wyslij(stan.ustawiacz, info);
    }
    if(stan.zgadujacy)
    {
        QJsonObject info;
        info["typ"] = "info";
        info["wiadomosc"] = "Twoja rola: zgadujacy. Czekasz na slowo.";
        wyslij(stan.zgadujacy, info);
    }
    QJsonObject st;
    st["typ"] = "state";
    st["maska"] = "_";
    st["bledy"] = 0;
    st["maxBledow"] = stan.maxBledow;
    st["uzyte"] = QJsonArray();
    broadcast(pokoj, st);
}


QString SerwerWisielec::maska(const StanPokoju &s) const
{
    QString wynik;
    for(QChar c : s.slowo)
    {
        if(c == ' ') wynik += "  ";
        else if(s.uzyte.contains(c)) wynik += c + ' ';
        else wynik += "_ ";
    }
    return wynik.trimmed();
}

bool SerwerWisielec::wygrana(const StanPokoju &s) const
{
    for(QChar c : s.slowo)
    {
        if(c == ' ') continue;
        if(!s.uzyte.contains(c)) return false;
    }
    return true;
}

void SerwerWisielec::wyslijStan(const QString &pokoj)
{
    if(!pokoje.contains(pokoj)) return;
    const auto &s = pokoje[pokoj];

    QJsonArray arr;
    QList<QChar> litery = s.uzyte.values();
    std::sort(litery.begin(), litery.end());
    for(QChar c : litery) arr.append(QString(c));

    QJsonObject obj;
    obj["typ"] = "state";
    obj["maska"] = maska(s);
    obj["bledy"] = s.bledy;
    obj["maxBledow"] = s.maxBledow;
    obj["uzyte"] = arr;

    broadcast(pokoj, obj);
}

void SerwerWisielec::obsluzSetWord(QWebSocket *s, const QJsonObject &obj)
{
    QString pokoj = klientPokoj.value(s, "");
    if(pokoj.isEmpty() || !pokoje.contains(pokoj)) return;

    auto &stan = pokoje[pokoj];

    if(s != stan.ustawiacz)
    {
        QJsonObject odp;
        odp["typ"] = "error";
        odp["wiadomosc"] = "Tylko ustawiacz moze ustawic slowo.";
        wyslij(s, odp);
        return;
    }

    if(!stan.zgadujacy)
    {
        QJsonObject odp;
        odp["typ"] = "error";
        odp["wiadomosc"] = "Brak drugiego gracza w pokoju.";
        wyslij(s, odp);
        return;
    }

    QString slowo = obj.value("slowo").toString().trimmed().toUpper();
    if(slowo.isEmpty())
    {
        QJsonObject odp;
        odp["typ"] = "error";
        odp["wiadomosc"] = "Slowo nie moze byc puste.";
        wyslij(s, odp);
        return;
    }

    for(QChar c : slowo)
    {
        if(c == ' ') continue;
        if(c < 'A' || c > 'Z')
        {
            QJsonObject odp;
            odp["typ"] = "error";
            odp["wiadomosc"] = "Slowo: tylko A-Z i spacje.";
            wyslij(s, odp);
            return;
        }
    }

    stan.slowo = slowo;
    stan.uzyte.clear();
    stan.bledy = 0;
    stan.maxBledow = 8;
    stan.aktywnaGra = true;

    QJsonObject start;
    start["typ"] = "game_start";
    start["maska"] = maska(stan);
    start["maxBledow"] = stan.maxBledow;
    broadcast(pokoj, start);

    wyslijStan(pokoj);
}

void SerwerWisielec::obsluzGuess(QWebSocket *s, const QJsonObject &obj)
{
    QString pokoj = klientPokoj.value(s, "");
    if(pokoj.isEmpty() || !pokoje.contains(pokoj))
    {
        QJsonObject odp;
        odp["typ"] = "error";
        odp["wiadomosc"] = "Nie jestes w pokoju.";
        wyslij(s, odp);
        return;
    }

    auto &stan = pokoje[pokoj];

    if(!stan.aktywnaGra)
    {
        QJsonObject odp;
        odp["typ"] = "error";
        odp["wiadomosc"] = "Gra jeszcze nie startuje (czekamy na slowo).";
        wyslij(s, odp);
        return;
    }

    if(s != stan.zgadujacy)
    {
        QJsonObject odp;
        odp["typ"] = "error";
        odp["wiadomosc"] = "Tylko zgadujacy moze zgadywac litery.";
        wyslij(s, odp);
        return;
    }

    QString lit = obj.value("litera").toString().trimmed().toUpper();
    if(lit.size() != 1)
    {
        QJsonObject odp;
        odp["typ"] = "error";
        odp["wiadomosc"] = "Podaj jedna litere.";
        wyslij(s, odp);
        return;
    }

    QChar l = lit[0];
    if(l < 'A' || l > 'Z')
    {
        QJsonObject odp;
        odp["typ"] = "error";
        odp["wiadomosc"] = "Dozwolone litery A-Z.";
        wyslij(s, odp);
        return;
    }

    if(stan.uzyte.contains(l))
    {
        wyslijStan(pokoj);
        return;
    }

    stan.uzyte.insert(l);

    if(!stan.slowo.contains(l))
        stan.bledy++;

    if(wygrana(stan))
    {
        QJsonObject end;
        end["typ"] = "end";
        end["wygrana"] = true;
        end["slowo"] = stan.slowo;
        broadcast(pokoj, end);

        QWebSocket* tmp = stan.ustawiacz;
        stan.ustawiacz = stan.zgadujacy;
        stan.zgadujacy = tmp;

        resetGry(pokoj);
        return;
    }

    if(stan.bledy >= stan.maxBledow)
    {
        QJsonObject end;
        end["typ"] = "end";
        end["wygrana"] = false;
        end["slowo"] = stan.slowo;
        broadcast(pokoj, end);

        QWebSocket* tmp = stan.ustawiacz;
        stan.ustawiacz = stan.zgadujacy;
        stan.zgadujacy = tmp;

        resetGry(pokoj);
        return;
    }

    wyslijStan(pokoj);
}
