#pragma once
#include <QVector>
#include "pionek.h"

class Gracz
{
public:
    Gracz(KolorGracza kolor, int indeksStartu, int liczbaPionkow = 4);

    KolorGracza kolor() const { return m_kolor; }
    int indeksStartu() const { return m_indeksStartu; }

    QVector<Pionek>& pionki() { return m_pionki; }
    const QVector<Pionek>& pionki() const { return m_pionki; }

    bool wygral() const;

private:
    KolorGracza m_kolor;
    int m_indeksStartu; 
    QVector<Pionek> m_pionki; 
};

