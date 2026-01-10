#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QJsonArray>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->editAdres->setText("ws://127.0.0.1:12345");
    ui->editNazwa->setText("Gracz");
    ui->labelMaska->setText("_ _ _");
    ui->labelBledy->setText("Bledy: 0/8");
    ui->labelUzyte->setText("Uzyte:");
    ui->labelStatus->setText("Rozlaczono");

    connect(&klient, &KlientSieciowy::polaczono, this, &MainWindow::onPolaczono);
    connect(&klient, &KlientSieciowy::rozlaczono, this, &MainWindow::onRozlaczono);
    connect(&klient, &KlientSieciowy::blad, this, &MainWindow::onBlad);
    connect(&klient, &KlientSieciowy::odebrano, this, &MainWindow::onOdebrano);

    ustawStanPoczatkowy();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::ustawStatus(const QString &txt)
{
    ui->labelStatus->setText(txt);
}

void MainWindow::ustawStanPoczatkowy()
{
    rola = "brak";
    rundaAktywna = false;
    czekamyNaSlowo = true;

    ui->panelUstawSlowo_3->setVisible(false);
    ui->panelZgaduj_3->setVisible(false);
}

void MainWindow::ustawRoleUi()
{

    if(rola == "ustawiacz")
    {
        ui->panelUstawSlowo_3->setVisible(czekamyNaSlowo);
        ui->panelZgaduj_3->setVisible(false);

        if(czekamyNaSlowo)
            ui->editSlowo->setFocus();
    }
    else if(rola == "zgadujacy")
    {
        ui->panelUstawSlowo_3->setVisible(false);
        ui->panelZgaduj_3->setVisible(rundaAktywna);

        if(rundaAktywna)
            ui->editLitera->setFocus();
    }
    else
    {
        ui->panelUstawSlowo_3->setVisible(false);
        ui->panelZgaduj_3->setVisible(false);
    }
}

void MainWindow::on_btnPolacz_clicked()
{
    if(klient.czyPolaczony())
    {
        klient.rozlacz();
        return;
    }

    klient.polacz(QUrl(ui->editAdres->text().trimmed()));
    ustawStatus("Laczenie...");
}

void MainWindow::onPolaczono()
{
    ustawStatus("Polaczono");

    QJsonObject obj;
    obj["typ"] = "hello";
    obj["nazwa"] = ui->editNazwa->text().trimmed();
    klient.wyslij(obj);
}

void MainWindow::onRozlaczono()
{
    ustawStatus("Rozlaczono");
    ustawStanPoczatkowy();
}

void MainWindow::onBlad(const QString &tekst)
{
    ustawStatus("Blad: " + tekst);
}

void MainWindow::on_btnUtworz_clicked()
{
    QJsonObject obj;
    obj["typ"] = "room_create";
    obj["gra"] = "wisielec";
    obj["room"] = ui->editPokoj->text().trimmed();
    klient.wyslij(obj);

    rundaAktywna = false;
    czekamyNaSlowo = true;
    ui->panelUstawSlowo_3->setVisible(false);
    ui->panelZgaduj_3->setVisible(false);
}

void MainWindow::on_btnDolacz_clicked()
{
    QJsonObject obj;
    obj["typ"] = "room_join";
    obj["room"] = ui->editPokoj->text().trimmed();
    klient.wyslij(obj);

    rundaAktywna = false;
    czekamyNaSlowo = true;
    ui->panelUstawSlowo_3->setVisible(false);
    ui->panelZgaduj_3->setVisible(false);
}

void MainWindow::on_btnUstawSlowo_clicked()
{
    QString slowo = ui->editSlowo->text().trimmed().toUpper();
    if(slowo.isEmpty()) return;

    QJsonObject obj;
    obj["typ"] = "set_word";
    obj["slowo"] = slowo;
    klient.wyslij(obj);

    ui->editSlowo->clear();

    czekamyNaSlowo = false;
    ustawRoleUi();
}

void MainWindow::on_btnZgadnij_clicked()
{
    QString lit = ui->editLitera->text().trimmed().toUpper();
    ui->editLitera->clear();

    if(lit.size() != 1) return;

    QJsonObject obj;
    obj["typ"] = "guess";
    obj["litera"] = lit;
    klient.wyslij(obj);
}

void MainWindow::onOdebrano(const QJsonObject &obj)
{
    const QString typ = obj.value("typ").toString();

    if(typ == "error")
    {
        if(obj.contains("wiadomosc"))
            ustawStatus("Blad: " + obj.value("wiadomosc").toString());
        return;
    }

    if(typ == "info" || typ == "hello_ok" || typ == "room_ok")
    {
        if(obj.contains("wiadomosc"))
            ustawStatus(obj.value("wiadomosc").toString());
        return;
    }

    if(typ == "role")
    {
        rola = obj.value("rola").toString();
        rundaAktywna = false;
        czekamyNaSlowo = true;

        ustawRoleUi();
        return;
    }

    if(typ == "game_start")
    {
        rundaAktywna = true;
        czekamyNaSlowo = false;

        odswiezStan(obj);
        ustawRoleUi();

        ustawStatus("Start rundy!");
        return;
    }

    if(typ == "state")
    {
        odswiezStan(obj);
        return;
    }

    if(typ == "end")
    {
        bool czyWygrana = obj.value("wygrana").toBool();
        QString slowo = obj.value("slowo").toString();

        QString tresc = czyWygrana
                            ? ("Wygrana! Slowo: " + slowo)
                            : ("Przegrana. Slowo: " + slowo);

        QMessageBox::information(this, "Koniec rundy", tresc);

        ustawStatus(tresc);
        rundaAktywna = false;
        czekamyNaSlowo = true;

        ui->panelUstawSlowo_3->setVisible(false);
        ui->panelZgaduj_3->setVisible(false);

        return;
    }


    if(obj.contains("wiadomosc"))
        ustawStatus(obj.value("wiadomosc").toString());
}

void MainWindow::odswiezStan(const QJsonObject &obj)
{
    if(obj.contains("maska"))
        ui->labelMaska->setText(obj.value("maska").toString());

    const int b = obj.value("bledy").toInt(0);
    const int maxB = obj.value("maxBledow").toInt(8);
    ui->labelBledy->setText(QString("Bledy: %1/%2").arg(b).arg(maxB));

    if(obj.contains("uzyte") && obj.value("uzyte").isArray())
    {
        QJsonArray arr = obj.value("uzyte").toArray();
        QStringList l;
        for(const auto &v : arr) l << v.toString();
        ui->labelUzyte->setText("Uzyte: " + l.join(", "));
    }
}
