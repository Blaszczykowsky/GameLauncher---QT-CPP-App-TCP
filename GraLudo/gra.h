#pragma once
#include <QObject>
#include <QVector>
#include "gracz.h"
#include <QJsonObject>

class Gra : public QObject
{
    Q_OBJECT
public:
    explicit Gra(QObject* parent = nullptr);

    void nowaGra(int liczbaGraczy);

    QVector<Gracz>& gracze() { return m_gracze; }
    const QVector<Gracz>& gracze() const { return m_gracze; }
    int aktualnyIndex() const { return m_aktualnyIndex; }
    const Gracz& aktualnyGracz() const { return m_gracze[m_aktualnyIndex]; }
    Gracz& aktualnyGracz() { return m_gracze[m_aktualnyIndex]; }

    int ostatniRzut() const { return m_ostatniRzut; }
    bool czyRzucono() const { return m_rzucono; }
    bool czyOczekujeNaDecyzje() const { return m_oczekujeNaDecyzje; }
    int rzutKostka();
    QVector<Pionek*> mozliwePionki();

    bool wykonajRuch(Pionek* pionek);

    bool moznaKontynuowacPoWygranej() const;
    void kontynuujPoWygranej();
    int idGry() const { return m_idGry; }

    QJsonObject stanJson() const;
    void ustawStanJson(const QJsonObject& j);


signals:
    void stanZmieniony();
    void komunikat(const QString& tekst);
    void koniecGry(const QString& zwyciezca);

private:
    int absIndexDla(const Gracz& g, int krokRel) const;
    int policzNaPoluAbs(int abs, KolorGracza kolor) const;
    int maksNaPoluAbs(int abs) const; 
    bool czyJestBlokadaNaAbs(int abs) const;

    bool czyPrzejscieZablokowane(const Gracz& g, const Pionek& p, int oczka) const;
    bool czyRuchDozwolony(const Gracz& g, const Pionek& p, int oczka) const;

    void zbicieJesliTrzeba(const Gracz& gRuszajacy, int absDocelowy, KolorGracza kolorRuszajacego);
    void nastepnyGracz();
    bool m_oczekujeNaDecyzje = false;

    QVector<Gracz> m_gracze;
    int m_aktualnyIndex = 0;
    int m_ostatniRzut = 0;
    bool m_rzucono = false;
    int m_idGry = 0;
};

