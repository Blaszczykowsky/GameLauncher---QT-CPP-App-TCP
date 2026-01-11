#include "kosci_logic.h"
#include <QRandomGenerator>
#include <QJsonArray>
#include <map>
#include <set>
#include <algorithm>

KosciLogic::KosciLogic(QObject* parent) : QObject(parent)
{
    m_botTimer.setSingleShot(true);
    connect(&m_botTimer, &QTimer::timeout, this, &KosciLogic::botRuch);
    connect(&m_siec, &SiecManager::wiadomoscOdebrana, this, &KosciLogic::sieciowyPakiet);
    connect(&m_siec, &SiecManager::log, this, &KosciLogic::komunikat);
}

int KosciLogic::obliczPunkty(Kategoria k, const std::array<int,5>& d) const
{
    std::map<int,int> m; int sum=0;
    for(int v:d){ m[v]++; sum+=v; }

    auto has = [&](std::initializer_list<int> s)
    {
        std::set<int> set(d.begin(), d.end());
        for(int x:s) if(!set.count(x)) return false; return true;
    };

    switch(k)
    {
    case Kategoria::Jedynki: return m[1]*1;
    case Kategoria::Dwojki: return m[2]*2;
    case Kategoria::Trojki: return m[3]*3;
    case Kategoria::Czworki: return m[4]*4;
    case Kategoria::Piatki: return m[5]*5;
    case Kategoria::Szostki: return m[6]*6;
    case Kategoria::Trojka: for(auto p:m) if(p.second>=3) return sum; return 0;
    case Kategoria::Czworka: for(auto p:m) if(p.second>=4) return sum; return 0;
    case Kategoria::Full: { bool t=0,d=0; for(auto p:m){if(p.second==3)t=1;if(p.second==2)d=1;} return (t&&d)?25:0; }
    case Kategoria::MalyStrit: return (has({1,2,3,4})||has({2,3,4,5})||has({3,4,5,6}))?30:0;
    case Kategoria::DuzyStrit: return (has({1,2,3,4,5})||has({2,3,4,5,6}))?40:0;
    case Kategoria::Yahtzee: for(auto p:m) if(p.second==5) return 50; return 0;
    case Kategoria::Szansa: return sum;
    }
    return 0;
}

void KosciLogic::startLokalnie(QString g1, QString g2)
{
    m_tryb = TrybGry::LOKALNY;
    m_gracze = {{g1,{},{}}, {g2,{},{}}};
    m_typy = {TypGracza::CZLOWIEK, TypGracza::CZLOWIEK};
    m_aktywnyID=0; m_nrRzutu=0;
    emit zmianaStanu();
}

void KosciLogic::startBot(QString g1)
{
    startLokalnie(g1, "Bot Stefan");
    m_tryb = TrybGry::SOLO_BOT;
    m_typy[1] = TypGracza::BOT;
}

void KosciLogic::startHost(QString g1)
{
    m_tryb = TrybGry::HOST;
    m_siec.startSerwer(PORT_GRY);
    m_gracze = {{g1,{},{}}};
    m_typy = {TypGracza::CZLOWIEK};
    emit komunikat("Serwer OK. Czekam...");
}

void KosciLogic::startKlient(QString ip, QString g1)
{
    m_tryb = TrybGry::KLIENT;
    m_siec.startKlient(ip, PORT_GRY);
    m_gracze = {{g1,{},{}}};

    connect(&m_siec, &SiecManager::polaczono, this, [=](){
        QJsonObject j; j["n"]=g1;
        QJsonObject p; p[JsonK::TYP]=JsonK::START; p[JsonK::DANE]=j;
        m_siec.wyslijDoHosta(p);
    });
}

void KosciLogic::rzuc()
{
    if(czyWszyscySkonczyli()) return;
    if(!czyMojaTura()) return;
    if(m_tryb==TrybGry::KLIENT)
    {
        QJsonObject p; p[JsonK::TYP]=JsonK::RZUT;
        m_siec.wyslijDoHosta(p);
    }
    else
    {
        przetworzAkcje(m_aktywnyID, JsonK::RZUT, {});
    }
}
void KosciLogic::przelaczBlokade(int idx)
{
    if(czyWszyscySkonczyli()) return;
    if(!czyMojaTura() || m_nrRzutu==0) return;
    QJsonObject d; d["i"]=idx;
    if(m_tryb==TrybGry::KLIENT)
    {
        QJsonObject p; p[JsonK::TYP]=JsonK::BLOKADA; p[JsonK::DANE]=d;
        m_siec.wyslijDoHosta(p);
    }
    else
    {
        przetworzAkcje(m_aktywnyID, JsonK::BLOKADA, d);
    }
}
void KosciLogic::wybierz(Kategoria k)
{
    if(czyWszyscySkonczyli()) return;
    if(!czyMojaTura() || m_nrRzutu==0) return;
    QJsonObject d; d["k"]=(int)k;
    if(m_tryb==TrybGry::KLIENT)
    {
        QJsonObject p; p[JsonK::TYP]=JsonK::WYBOR; p[JsonK::DANE]=d;
        m_siec.wyslijDoHosta(p);
    }
    else
    {
        przetworzAkcje(m_aktywnyID, JsonK::WYBOR, d);
    }
}

void KosciLogic::przetworzAkcje(int id, QString typ, QJsonObject d)
{
    if(id != m_aktywnyID) return;

    if(typ == JsonK::RZUT && m_nrRzutu < MAX_RZUTOW)
    {
        wykonajRzutLogika();
        m_nrRzutu++;
        emit zmianaStanu();
        if(m_tryb == TrybGry::HOST) wyslijStan();
    }
    else if(typ == JsonK::BLOKADA)
    {
        int i = d["i"].toInt();
        if(i>=0 && i<5) m_blokady[i] = !m_blokady[i];
        emit zmianaStanu();
        if(m_tryb == TrybGry::HOST) wyslijStan();
    }
    else if(typ == JsonK::WYBOR)
    {
        Kategoria k = (Kategoria)d["k"].toInt();
        if(!m_gracze[id].zajete[k])
        {
            m_gracze[id].wynik[k] = obliczPunkty(k, m_oczka);
            m_gracze[id].zajete[k] = true;

            emit zmianaStanu();
            if(m_tryb == TrybGry::HOST) wyslijStan();

            if(czyWszyscySkonczyli())
            {
                sprawdzKoniecGry();
            }
            else
            {
                nastepny();
            }
        }
    }
}

void KosciLogic::wykonajRzutLogika()
{
    for(int i=0;i<5;i++) if(!m_blokady[i]) m_oczka[i] = QRandomGenerator::global()->bounded(1,7);
}

void KosciLogic::nastepny()
{
    m_aktywnyID = (m_aktywnyID + 1) % m_gracze.size();
    m_nrRzutu = 0;
    m_blokady.fill(false);

    emit zmianaStanu();
    if(m_tryb == TrybGry::HOST) wyslijStan();

    if(m_typy[m_aktywnyID] == TypGracza::BOT)
    {
        m_botTimer.start(1000);
    }
}

bool KosciLogic::czyWszyscySkonczyli() const
{
    if(m_gracze.empty()) return false;
    for(const auto& g : m_gracze)
    {
        if(!g.koniec()) return false;
    }
    return true;
}

void KosciLogic::sprawdzKoniecGry()
{
    m_botTimer.stop();
    int maxPkt = -1;
    QString zwyciezca = "";

    for(const auto& g : m_gracze)
    {
        int total = g.total();
        if(total > maxPkt)
        {
            maxPkt = total;
            zwyciezca = g.nazwa;
        }
        else if (total == maxPkt)
        {
            zwyciezca += " & " + g.nazwa;
        }
    }
    emit graZakonczona(zwyciezca, maxPkt);
}

void KosciLogic::botRuch()
{
    if(m_typy[m_aktywnyID] != TypGracza::BOT)
    {
        m_botTimer.stop(); return;
    }
    if(czyWszyscySkonczyli()) return;

    if(m_nrRzutu < 3)
    {
        for(int i=0;i<5;i++)
        {
            if(m_oczka[i] >= 4) m_blokady[i] = true;
        }
        przetworzAkcje(m_aktywnyID, JsonK::RZUT, {});
        m_botTimer.start(800);
    }
    else
    {
        int maxPkt = -1;
        Kategoria najlepszaKat = Kategoria::Szansa;
        bool znaleziono = false;

        for(int k=0; k<=12; k++)
        {
            Kategoria kat = (Kategoria)k;
            if(!m_gracze[m_aktywnyID].zajete[kat])
            {
                int pkt = obliczPunkty(kat, m_oczka);
                if (pkt > 0 && (kat == Kategoria::Yahtzee || kat == Kategoria::DuzyStrit))
                {
                    pkt += 50;
                }
                if(pkt > maxPkt)
                {
                    maxPkt = pkt;
                    najlepszaKat = kat;
                    znaleziono = true;
                }
            }
        }

        if(!znaleziono || maxPkt == 0)
        {
            for(int k=0; k<=12; k++) {
                if(!m_gracze[m_aktywnyID].zajete[(Kategoria)k])
                {
                    najlepszaKat = (Kategoria)k;
                    break;
                }
            }
        }
        QJsonObject d; d["k"]=(int)najlepszaKat;
        przetworzAkcje(m_aktywnyID, JsonK::WYBOR, d);
    }
}

bool KosciLogic::czyMojaTura() const
{
    if(czyWszyscySkonczyli()) return false;
    if(m_tryb==TrybGry::LOKALNY || m_tryb==TrybGry::SOLO_BOT) return m_typy[m_aktywnyID]==TypGracza::CZLOWIEK;
    if(m_tryb==TrybGry::HOST) return m_aktywnyID==0;
    if(m_tryb==TrybGry::KLIENT) return m_aktywnyID==1;
    return false;
}

void KosciLogic::wyslijStan()
{
    QJsonObject stan;
    stan["id"] = m_aktywnyID;
    stan["nr"] = m_nrRzutu;
    QJsonArray k; for(int v:m_oczka) k.append(v); stan["k"]=k;
    QJsonArray b; for(bool v:m_blokady) b.append(v); stan["b"]=b;

    QJsonArray gArr;
    for(const auto& g : m_gracze)
    {
        QJsonObject gObj; gObj["n"]=g.nazwa;
        QJsonObject res;
        for(auto key : g.wynik.keys()) res[QString::number((int)key)] = g.wynik[key];
        gObj["res"]=res;
        gArr.append(gObj);
    }
    stan["g"]=gArr;

    QJsonObject p; p[JsonK::TYP]=JsonK::STAN; p[JsonK::DANE]=stan;
    m_siec.wyslijDoKlienta(p);
}

void KosciLogic::sieciowyPakiet(QJsonObject json)
{
    QString t = json[JsonK::TYP].toString();
    QJsonObject d = json[JsonK::DANE].toObject();

    if(m_tryb == TrybGry::HOST)
    {
        if(t == JsonK::START)
        {
            m_gracze.push_back({d["n"].toString(),{},{}});
            m_typy.push_back(TypGracza::SIECIOWY);
            wyslijStan();
            emit zmianaStanu();
        }
        else
        {
            przetworzAkcje(1, t, d);
        }
    }
    else if(m_tryb == TrybGry::KLIENT && t == JsonK::STAN)
    {
        m_aktywnyID = d["id"].toInt();
        m_nrRzutu = d["nr"].toInt();
        QJsonArray k=d["k"].toArray(); for(int i=0;i<5;i++) m_oczka[i]=k[i].toInt();
        QJsonArray b=d["b"].toArray(); for(int i=0;i<5;i++) m_blokady[i]=b[i].toBool();

        QJsonArray gArr = d["g"].toArray();
        if(m_gracze.size() != gArr.size())
        {
            m_gracze.clear();
            for(auto _ : gArr) m_gracze.push_back({"",{},{}});
        }
        for(int i=0; i<gArr.size(); ++i)
        {
            QJsonObject gObj = gArr[i].toObject();
            m_gracze[i].nazwa = gObj["n"].toString();
            QJsonObject res = gObj["res"].toObject();
            for(auto key : res.keys())
            {
                Kategoria kat = (Kategoria)key.toInt();
                m_gracze[i].wynik[kat] = res[key].toInt();
                m_gracze[i].zajete[kat] = true;
            }
        }
        emit zmianaStanu();
        if(czyWszyscySkonczyli()) sprawdzKoniecGry();
    }
}
