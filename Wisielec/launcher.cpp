#include "launcher.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QStandardItemModel>

Launcher::Launcher(QWidget *parent) : QWidget(parent)
{
    setupUI();
    setWindowTitle("Game Launcher");
    resize(400, 500);
}

void Launcher::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);

    QLabel *title = new QLabel("WYBIERZ GRĘ", this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 24px; font-weight: bold;");
    mainLayout->addWidget(title);

    gameSelector = new QComboBox(this);
    gameSelector->addItem("Wisielec", (int)GameType::Wisielec);
    gameSelector->addItem("Kości (Wkrótce)", (int)GameType::Kosci);
    gameSelector->addItem("Chińczyk (Wkrótce)", (int)GameType::Chinczyk);

    QStandardItemModel *model = qobject_cast<QStandardItemModel*>(gameSelector->model());
    if(model) {
        model->item(1)->setEnabled(false);
        model->item(2)->setEnabled(false);
    }
    mainLayout->addWidget(gameSelector);

    QGroupBox *modeGroup = new QGroupBox("Tryb Gry", this);
    QVBoxLayout *modeLayout = new QVBoxLayout(modeGroup);

    modeSolo = new QRadioButton("Solo (Losowe słowa)", this);
    modeLocal = new QRadioButton("Lokalnie (2 Graczy)", this);
    modeHost = new QRadioButton("Sieć: Host (Serwer)", this);
    modeClient = new QRadioButton("Sieć: Dołącz (Klient)", this);

    modeSolo->setChecked(true);

    modeLayout->addWidget(modeSolo);
    modeLayout->addWidget(modeLocal);
    modeLayout->addWidget(modeHost);
    modeLayout->addWidget(modeClient);
    mainLayout->addWidget(modeGroup);

    QGroupBox *netGroup = new QGroupBox("Ustawienia Sieciowe", this);
    QVBoxLayout *netLayout = new QVBoxLayout(netGroup);

    QLabel *ipLbl = new QLabel("Adres IP Hosta:", this);
    netLayout->addWidget(ipLbl);
    ipInput = new QLineEdit("127.0.0.1", this);
    netLayout->addWidget(ipInput);

    mainLayout->addWidget(netGroup);

    startBtn = new QPushButton("URUCHOM GRĘ", this);
    startBtn->setMinimumHeight(50);
    startBtn->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; font-size: 16px;");
    mainLayout->addWidget(startBtn);

    connect(startBtn, &QPushButton::clicked, this, &Launcher::onStartClicked);
    connect(modeSolo, &QRadioButton::toggled, this, &Launcher::updateUIState);
    connect(modeLocal, &QRadioButton::toggled, this, &Launcher::updateUIState);
    connect(modeHost, &QRadioButton::toggled, this, &Launcher::updateUIState);
    connect(modeClient, &QRadioButton::toggled, this, &Launcher::updateUIState);

    updateUIState();
}

void Launcher::updateUIState()
{
    bool isNet = modeClient->isChecked();
    ipInput->setEnabled(isNet);
}

void Launcher::onStartClicked()
{
    GameLaunchConfig config;
    config.gameType = (GameType)gameSelector->currentData().toInt();
    config.port = 12345;
    config.hostIp = ipInput->text();

    if (modeSolo->isChecked()) config.mode = GameMode::Solo;
    else if (modeLocal->isChecked()) config.mode = GameMode::LocalDuo;
    else if (modeHost->isChecked()) config.mode = GameMode::NetHost;
    else if (modeClient->isChecked()) config.mode = GameMode::NetClient;

    emit launchGame(config);
}
