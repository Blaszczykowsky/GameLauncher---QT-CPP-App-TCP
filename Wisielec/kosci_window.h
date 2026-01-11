#ifndef KOSCI_WINDOW_H
#define KOSCI_WINDOW_H

#pragma once
#include <QMainWindow>
#include <QToolButton>
#include <QTimer>
#include "kosci_logic.h"
#include "game_config.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
class OknoGry;
}
QT_END_NAMESPACE

class KosciWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit KosciWindow(const GameLaunchConfig &config, QWidget* parent = nullptr);
    ~KosciWindow();

signals:
    void gameClosed();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void odswiez();
    void onAnimacja();
    void onBackToMenu();

private:
    Ui::OknoGry* ui;
    KosciLogic* logic;
    QTimer animTimer;
    int animKroki = 0;

    void ustawKosc(int idx, int val, bool blocked);
};

#endif // KOSCI_WINDOW_H
