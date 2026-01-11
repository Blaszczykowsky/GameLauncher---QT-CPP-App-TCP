#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <QWidget>
#include <QComboBox>
#include <QRadioButton>
#include <QLineEdit>
#include <QPushButton>
#include "game_config.h"

class Launcher : public QWidget
{
    Q_OBJECT

public:
    explicit Launcher(QWidget *parent = nullptr);

signals:
    void launchGame(const GameLaunchConfig &config);

private slots:
    void onStartClicked();
    void updateUIState();

private:
    void setupUI();

    QComboBox *gameSelector;
    QRadioButton *modeSolo;
    QRadioButton *modeLocal;
    QRadioButton *modeHost;
    QRadioButton *modeClient;
    QLineEdit *ipInput;
    QLineEdit *nameInput;
    QLineEdit *portInput;
    QPushButton *startBtn;
};

#endif
