#include "launcher.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QStandardItemModel>

Launcher::Launcher(QWidget *parent) : QWidget(parent)
{
    setupUI();
    setWindowTitle("Multi Game Launcher");
    resize(400, 600);
}

void Launcher::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);

    QLabel *title = new QLabel("CENTRUM GIER", this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 24px; font-weight: bold; color: #333;");
    mainLayout->addWidget(title);

    QGroupBox *nickGroup = new QGroupBox("Twój Nick", this);
    QVBoxLayout *nickLay = new QVBoxLayout(nickGroup);
    QLineEdit *nickInput = new QLineEdit(this);
    nickInput->setPlaceholderText("Wpisz swoją ksywę...");
    nickInput->setText("Gracz");
    this->nameInput = nickInput;
    nickLay->addWidget(nickInput);
    mainLayout->addWidget(nickGroup);

    QGroupBox *gameGroup = new QGroupBox("Wybierz Grę", this);
    QVBoxLayout *gameLay = new QVBoxLayout(gameGroup);
    gameSelector = new QComboBox(this);
    gameSelector->addItem("Wisielec", (int)GameType::Wisielec);
    gameSelector->addItem("Kości (Yahtzee)", (int)GameType::Kosci);
    gameSelector->setStyleSheet("padding: 5px; font-size: 14px;");
    gameLay->addWidget(gameSelector);
    mainLayout->addWidget(gameGroup);

    QGroupBox *modeGroup = new QGroupBox("Tryb Gry", this);
    QVBoxLayout *modeLayout = new QVBoxLayout(modeGroup);
    modeSolo = new QRadioButton("Solo / Z Botem", this);
    modeLocal = new QRadioButton("Lokalnie (2 Graczy)", this);
    modeHost = new QRadioButton("Sieć: Hostuj", this);
    modeClient = new QRadioButton("Sieć: Dołącz", this);

    modeSolo->setChecked(true);
    modeLayout->addWidget(modeSolo);
    modeLayout->addWidget(modeLocal);
    modeLayout->addWidget(modeHost);
    modeLayout->addWidget(modeClient);
    mainLayout->addWidget(modeGroup);

    QGroupBox *netGroup = new QGroupBox("Ustawienia Sieciowe", this);
    QVBoxLayout *netLayout = new QVBoxLayout(netGroup);

    netLayout->addWidget(new QLabel("Adres IP:"));
    ipInput = new QLineEdit("127.0.0.1", this);
    netLayout->addWidget(ipInput);

    netLayout->addWidget(new QLabel("Port:"));
    portInput = new QLineEdit("5000", this);
    netLayout->addWidget(portInput);

    mainLayout->addWidget(netGroup);

    startBtn = new QPushButton("GRAJ", this);
    startBtn->setMinimumHeight(50);
    startBtn->setStyleSheet("background-color: #2196F3; color: white; font-weight: bold; font-size: 16px; border-radius: 5px;");
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
    bool isNet = modeClient->isChecked() || modeHost->isChecked();
    bool isClient = modeClient->isChecked();

    ipInput->setEnabled(isClient);
    portInput->setEnabled(isNet);
}

void Launcher::onStartClicked()
{
    GameLaunchConfig config;
    config.gameType = (GameType)gameSelector->currentData().toInt();
    config.hostIp = ipInput->text();
    config.port = portInput->text().toInt();
    config.playerName = nameInput->text().isEmpty() ? "Gracz" : nameInput->text();

    if (modeSolo->isChecked()) config.mode = GameMode::Solo;
    else if (modeLocal->isChecked()) config.mode = GameMode::LocalDuo;
    else if (modeHost->isChecked()) config.mode = GameMode::NetHost;
    else if (modeClient->isChecked()) config.mode = GameMode::NetClient;

    emit launchGame(config);
}
