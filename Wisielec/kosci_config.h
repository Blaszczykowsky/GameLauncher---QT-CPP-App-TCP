#ifndef KOSCI_CONFIG_H
#define KOSCI_CONFIG_H

#pragma once
#include <QString>
#include <QMap>
#include <vector>

constexpr int MAX_RZUTOW = 3;
constexpr int PORT_GRY = 5000;

enum class TrybGry
{
    LOKALNY,
    SOLO_BOT,
    HOST,
    KLIENT
};

enum class TypGracza
{
    CZLOWIEK,
    SIECIOWY,
    BOT
};

enum class Kategoria
{
    Jedynki,
    Dwojki,
    Trojki,
    Czworki,
    Piatki,
    Szostki,
    Trojka,
    Czworka,
    Full,
    MalyStrit,
    DuzyStrit,
    Yahtzee,
    Szansa
};


struct StanGracza
{
    QString nazwa;
    QMap<Kategoria, int> wynik;
    QMap<Kategoria, bool> zajete;

    int sumaGor() const
    {
        int s = 0;
        std::vector<Kategoria> k = { Kategoria::Jedynki, Kategoria::Dwojki, Kategoria::Trojki, Kategoria::Czworki, Kategoria::Piatki, Kategoria::Szostki };
        for (auto cat : k) if (zajete.value(cat)) s += wynik.value(cat);
        return s;
    }
    int bonus() const
    {
        return sumaGor() >= 63 ? 35 : 0;
    }

    int sumaDol() const
    {
        int s = 0;
        for (auto k : wynik.keys()) s += wynik[k];
        return s - sumaGor();
    }

    int total() const
    {
        return sumaGor() + bonus() + sumaDol();
    }

    bool koniec() const
    {
        return zajete.size() >= 13;
    }
};

namespace JsonK {
    const QString TYP = "t";
    const QString DANE = "d";
    const QString START = "START";
    const QString STAN = "STAN";
    const QString RZUT = "RZUT";
    const QString BLOKADA = "BLOK";
    const QString WYBOR = "WYBOR";
}

#endif // KOSCI_CONFIG_H
