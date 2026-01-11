#ifndef KOSCI_LOGIC_H
#define KOSCI_LOGIC_H

#pragma once
#include <QObject>
#include <QTimer>
#include <array>
#include "kosci_config.h"
#include "kosci_network.h"

class KosciLogic : public QObject
{
    Q_OBJECT

public:
    explicit KosciLogic(QObject* parent = nullptr);

    void startLokalnie(QString g1, QString g2);
    void startBot(QString g1);
    void startHost(QString g1);
    void startKlient(QString ip, QString g1);

    void rzuc();
    void przelaczBlokade(int idx);
    void wybierz(Kategoria kat);

    const std::vector<StanGracza>& gracze() const { return m_gracze; }
    int tura() const { return m_aktywnyID; }
    int rzutNr() const { return m_nrRzutu; }
    const std::array<int, 5>& kosci() const { return m_oczka; }
    const std::array<bool, 5>& blokady() const { return m_blokady; }

    bool czyMojaTura() const;
    int obliczPunkty(Kategoria k, const std::array<int, 5>& dice) const;
    bool czyWszyscySkonczyli() const;

signals:
    void zmianaStanu();
    void komunikat(QString msg);
    void graZakonczona(QString zwyciezca, int punkty); // NOWY SYGNAŁ

private slots:
    void botRuch();
    void sieciowyPakiet(QJsonObject json);

private:
    SiecManager m_siec;
    QTimer m_botTimer;

    TrybGry m_tryb = TrybGry::LOKALNY;
    std::vector<StanGracza> m_gracze;
    std::vector<TypGracza> m_typy;

    int m_aktywnyID = 0;
    int m_nrRzutu = 0;
    std::array<int, 5> m_oczka{ 1,1,1,1,1 };
    std::array<bool, 5> m_blokady{ false,false,false,false,false };

    void nastepny();
    void wykonajRzutLogika();
    void wyslijStan();
    void przetworzAkcje(int id, QString typ, QJsonObject d);
    void sprawdzKoniecGry();
};

#endif // KOSCI_LOGIC_H
