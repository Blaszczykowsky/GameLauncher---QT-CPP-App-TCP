#pragma once

#include <QMainWindow>
#include <QJsonObject>

#include "klient_sieciowy.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnPolacz_clicked();
    void on_btnUtworz_clicked();
    void on_btnDolacz_clicked();
    void on_btnUstawSlowo_clicked();
    void on_btnZgadnij_clicked();

    void onPolaczono();
    void onRozlaczono();
    void onBlad(const QString &tekst);
    void onOdebrano(const QJsonObject &obj);

private:
    Ui::MainWindow *ui;
    KlientSieciowy klient;

    QString rola = "brak";

    bool rundaAktywna = false;
    bool czekamyNaSlowo = true;

    void ustawStanPoczatkowy();
    void ustawRoleUi();
    void odswiezStan(const QJsonObject &obj);
    void ustawStatus(const QString &txt);
};
