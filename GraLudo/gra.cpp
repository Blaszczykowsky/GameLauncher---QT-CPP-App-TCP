#include "gra.h"
#include "kostka.h"
#include <QTimer>
#include <QJsonArray>
#include <QJsonObject>

static QVector<KolorGracza> koloryDlaLiczbyGraczy(int n)
{
    if (n == 2) return { KolorGracza::Czerwony, KolorGracza::Niebieski };
    if (n == 3) return { KolorGracza::Czerwony, KolorGracza::Zielony, KolorGracza::Niebieski };
    return { KolorGracza::Czerwony, KolorGracza::Zielony, KolorGracza::Niebieski, KolorGracza::Zolty };
}

static int startIndexDlaKoloru(KolorGracza k)
{
    switch (k)
    {
    case KolorGracza::Czerwony:  return 2;
    case KolorGracza::Zielony:   return 15;
    case KolorGracza::Niebieski: return 28;
    case KolorGracza::Zolty:     return 41;
    }
    return 2;
}

Gra::Gra(QObject* parent) : QObject(parent) {}

void Gra::nowaGra(int liczbaGraczy)
{
    m_gracze.clear();
    m_idGry++;

    auto kolory = koloryDlaLiczbyGraczy(liczbaGraczy);
    for (auto k : kolory)
        m_gracze.push_back(Gracz(k, startIndexDlaKoloru(k)));

    m_aktualnyIndex = 0;
    m_ostatniRzut = 0;
    m_rzucono = false;
    m_oczekujeNaDecyzje = false;

    emit komunikat("Nowa gra.");
    emit stanZmieniony();
}

int Gra::absIndexDla(const Gracz& g, int krokRel) const
{
    return (g.indeksStartu() + krokRel) % 52;
}

int Gra::policzNaPoluAbs(int abs, KolorGracza kolor) const
{
    int licznik = 0;
    for (const auto& gr : m_gracze)
    {
        for (const auto& p : gr.pionki())
        {
            if (p.naTorzeGlownym())
            {
                int a = absIndexDla(gr, p.krok());
                if (a == abs && p.kolor() == kolor)
                    ++licznik;
            }
        }
    }
    return licznik;
}

int Gra::maksNaPoluAbs(int abs) const
{
    int m = 0;
    m = std::max(m, policzNaPoluAbs(abs, KolorGracza::Czerwony));
    m = std::max(m, policzNaPoluAbs(abs, KolorGracza::Zielony));
    m = std::max(m, policzNaPoluAbs(abs, KolorGracza::Niebieski));
    m = std::max(m, policzNaPoluAbs(abs, KolorGracza::Zolty));
    return m;
}

bool Gra::czyJestBlokadaNaAbs(int abs) const
{
    return maksNaPoluAbs(abs) >= 2;
}

bool Gra::czyPrzejscieZablokowane(const Gracz& g, const Pionek& p, int oczka) const
{
    if (!p.naTorzeGlownym()) return false;

    for (int k = 1; k <= oczka - 1; ++k)
    {
        int krokTmp = p.krok() + k;
        if (krokTmp <= 51)
        {
            int abs = absIndexDla(g, krokTmp);
            if (czyJestBlokadaNaAbs(abs))
                return true;
        }
        else
        {
            break;
        }
    }
    return false;
}

bool Gra::czyRuchDozwolony(const Gracz& g, const Pionek& p, int oczka) const
{
    if (p.wBazie())
    {
        if (oczka != 6) return false;

        int abs = absIndexDla(g, 0);
        if (czyJestBlokadaNaAbs(abs) && policzNaPoluAbs(abs, p.kolor()) < 2)
            return false;
        if (policzNaPoluAbs(abs, p.kolor()) >= 2)
            return false;

        return true;
    }

    int nowyKrok = p.krok() + oczka;
    if (nowyKrok > 55) return false;

    if (czyPrzejscieZablokowane(g, p, oczka))
        return false;

    if (nowyKrok <= 51)
    {
        int abs = absIndexDla(g, nowyKrok);

        if (czyJestBlokadaNaAbs(abs) && policzNaPoluAbs(abs, p.kolor()) < 2)
            return false;

        if (policzNaPoluAbs(abs, p.kolor()) >= 2)
            return false;

        return true;
    }

    int od = std::max(52, p.krok() + 1);
    for (int k = od; k <= nowyKrok; ++k)
    {
        for (const auto& pp : g.pionki())
            if (&pp != &p && pp.krok() == k)
                return false;
    }
    return true;
}

int Gra::rzutKostka()
{
    if (m_oczekujeNaDecyzje)
        return 0; 

    if (m_rzucono)
    {
        emit komunikat("Najpierw wykonaj ruch.");
        return m_ostatniRzut;
    }

    m_ostatniRzut = Kostka::rzut();
    m_rzucono = true;

    emit komunikat(kolorNaTekst(aktualnyGracz().kolor()) + " rzuca: " + QString::number(m_ostatniRzut));

    auto mozliwe = mozliwePionki();
    if (mozliwe.isEmpty())
    {
        emit komunikat("Brak ruchu - tura przepada.");
        QTimer::singleShot(650, this, [this](){
            m_rzucono = false;
            nastepnyGracz();      
            emit stanZmieniony(); 
        });

        emit stanZmieniony(); 
        return m_ostatniRzut;
    }

    emit stanZmieniony();
    return m_ostatniRzut;
}

QVector<Pionek*> Gra::mozliwePionki()
{
    QVector<Pionek*> wynik;
    if (!m_rzucono) return wynik;

    auto& g = aktualnyGracz();
    for (auto& p : g.pionki())
        if (czyRuchDozwolony(g, p, m_ostatniRzut))
            wynik.push_back(&p);

    return wynik;
}

void Gra::zbicieJesliTrzeba(const Gracz& gRuszajacy, int absDocelowy, KolorGracza kolorRuszajacego)
{
    if (czyJestBlokadaNaAbs(absDocelowy))
        return;

    int przeciwnikow = 0;
    for (const auto& gr : m_gracze)
    {
        for (const auto& p : gr.pionki())
        {
            if (p.naTorzeGlownym())
            {
                int a = absIndexDla(gr, p.krok());
                if (a == absDocelowy && p.kolor() != kolorRuszajacego)
                    ++przeciwnikow;
            }
        }
    }

    if (przeciwnikow != 1) return;

    for (auto& gr : m_gracze)
    {
        for (auto& p : gr.pionki())
        {
            if (p.naTorzeGlownym())
            {
                int a = absIndexDla(gr, p.krok());
                if (a == absDocelowy && p.kolor() != kolorRuszajacego)
                {
                    p.ustawKrok(-1);
                    emit komunikat("Zbicie! Pionek wraca do bazy.");
                    return;
                }
            }
        }
    }
}

bool Gra::wykonajRuch(Pionek* pionek)
{
    if (m_oczekujeNaDecyzje)
        return false; 

    if (!m_rzucono || !pionek)
        return false;

    auto& g = aktualnyGracz();
    if (pionek->kolor() != g.kolor())
    {
        emit komunikat("To nie twoj pionek.");
        return false;
    }

    if (!czyRuchDozwolony(g, *pionek, m_ostatniRzut))
    {
        emit komunikat("Ten ruch jest niedozwolony.");
        return false;
    }

    int nowyKrok = 0;

    if (pionek->wBazie())
    {
        nowyKrok = 0;
        int abs = absIndexDla(g, 0);
        zbicieJesliTrzeba(g, abs, pionek->kolor());
        pionek->ustawKrok(nowyKrok);
    }
    else
    {
        nowyKrok = pionek->krok() + m_ostatniRzut;
        if (nowyKrok > 55)
        {
            emit komunikat("Za duzo oczek - musisz trafic idealnie do mety.");
            return false;
        }

        if (nowyKrok <= 51)
        {
            int abs = absIndexDla(g, nowyKrok);
            zbicieJesliTrzeba(g, abs, pionek->kolor());
        }

        pionek->ustawKrok(nowyKrok);
    }
    if (g.wygral())
    {
        m_oczekujeNaDecyzje = true;
        m_rzucono = false;
        emit koniecGry(kolorNaTekst(g.kolor()));
        emit stanZmieniony();
        return true;
    }

    if (m_ostatniRzut == 6)
    {
        emit komunikat("Szostka - dodatkowy rzut!");
        m_rzucono = false;
        m_ostatniRzut = 0;
    }
    else
    {
        m_rzucono = false;
        nastepnyGracz();
    }

    emit stanZmieniony();
    return true;
}

void Gra::nastepnyGracz()
{
    m_ostatniRzut = 0;
    if (m_gracze.isEmpty()) return;

    int proby = 0;
    do
    {
        m_aktualnyIndex = (m_aktualnyIndex + 1) % m_gracze.size();
        proby++;
        if (proby > m_gracze.size()) break; 
    }
    while (aktualnyGracz().wygral());

    emit komunikat("Tura: " + kolorNaTekst(aktualnyGracz().kolor()));
}

bool Gra::moznaKontynuowacPoWygranej() const
{
    int grajacy = 0;
    for (const auto& g : m_gracze)
        if (!g.wygral())
            grajacy++;
    return grajacy >= 2;
}

void Gra::kontynuujPoWygranej()
{
    if (!m_oczekujeNaDecyzje) return;

    m_oczekujeNaDecyzje = false;
    m_rzucono = false;
    m_ostatniRzut = 0;
    nastepnyGracz();
    emit stanZmieniony();
}

QJsonObject Gra::stanJson() const
{
    QJsonObject j;
    j["gid"] = m_idGry;
    j["tura"] = m_aktualnyIndex;
    j["rzut"] = m_ostatniRzut;
    j["rzucono"] = m_rzucono;

    QJsonArray aGracze;
    for (const auto& g : m_gracze)
    {
        QJsonObject jg;
        jg["kolor"] = (int)g.kolor();
        jg["start"] = g.indeksStartu();

        QJsonArray aPionki;
        for (const auto& p : g.pionki())
        {
            QJsonObject jp;
            jp["id"] = p.id();
            jp["krok"] = p.krok();
            aPionki.append(jp);
        }

        jg["pionki"] = aPionki;
        aGracze.append(jg);
    }

    j["gracze"] = aGracze;
    return j;
}

void Gra::ustawStanJson(const QJsonObject& j)
{
    m_gracze.clear();

    QJsonArray aGracze = j.value("gracze").toArray();
    for (const auto& vg : aGracze)
    {
        QJsonObject jg = vg.toObject();
        KolorGracza k = (KolorGracza)jg.value("kolor").toInt();
        int start = jg.value("start").toInt();

        Gracz g(k, start);

        QJsonArray aP = jg.value("pionki").toArray();
        for (const auto& vp : aP)
        {
            QJsonObject jp = vp.toObject();
            int id = jp.value("id").toInt();
            int krok = jp.value("krok").toInt();

            if (id >= 0 && id < g.pionki().size())
                g.pionki()[id].ustawKrok(krok);
        }

        m_gracze.push_back(g);
    }

    m_idGry = j.value("gid").toInt(m_idGry);
    m_aktualnyIndex = j.value("tura").toInt(0);
    if (m_aktualnyIndex < 0) m_aktualnyIndex = 0;
    if (!m_gracze.isEmpty() && m_aktualnyIndex >= m_gracze.size())
        m_aktualnyIndex = 0;

    m_ostatniRzut = j.value("rzut").toInt(0);
    m_rzucono = j.value("rzucono").toBool(false);

    emit stanZmieniony();
}

