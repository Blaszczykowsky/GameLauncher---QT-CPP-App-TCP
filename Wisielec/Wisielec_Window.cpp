#include "wisielec_window.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QPainter>
#include <cmath>

WisielecWindow::WisielecWindow(const GameLaunchConfig &cfg, QWidget *parent)
    : QMainWindow(parent), config(cfg), server(nullptr), socket(nullptr)
{
    logic = new game_logic(this);

    connect(logic, &game_logic::wordSet, this, &WisielecWindow::onWordSet);
    connect(logic, &game_logic::letterGuessed, this, &WisielecWindow::onLetterGuessed);
    connect(logic, &game_logic::gameStateChanged, this, &WisielecWindow::onGameStateChanged);
    connect(logic, &game_logic::errorsChanged, this, &WisielecWindow::onErrorsChanged);

    setupUI();
    createHangmanImages();
    initGame(); // Automatyczny start na podstawie configu
    resize(800, 600);
}

WisielecWindow::~WisielecWindow() {
    if(server) server->close();
    if(socket) socket->close();
}

void WisielecWindow::closeEvent(QCloseEvent *event) {
    emit gameClosed();
    QMainWindow::closeEvent(event);
}

void WisielecWindow::setupUI() {
    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);

    stack = new QStackedWidget(this);
    mainLayout->addWidget(stack);

    // --- STRONA 0: Oczekiwanie (Sieć) ---
    pageWait = new QWidget(this);
    QVBoxLayout *waitLay = new QVBoxLayout(pageWait);
    waitLabel = new QLabel("Inicjalizacja...", this);
    waitLabel->setAlignment(Qt::AlignCenter);
    waitLabel->setStyleSheet("font-size: 18px;");
    waitLay->addWidget(waitLabel);
    stack->addWidget(pageWait);

    // --- STRONA 1: Ustawianie słowa ---
    pageSetup = new QWidget(this);
    QVBoxLayout *setupLay = new QVBoxLayout(pageSetup);
    QLabel *setupLbl = new QLabel("Wpisz hasło do odgadnięcia:", this);
    setupLay->addWidget(setupLbl);
    wordInput = new QLineEdit(this);
    wordInput->setEchoMode(QLineEdit::Password);
    setupLay->addWidget(wordInput);
    QPushButton *btnConfirm = new QPushButton("Zatwierdź", this);
    connect(btnConfirm, &QPushButton::clicked, this, &WisielecWindow::confirmWord);
    setupLay->addWidget(btnConfirm);
    setupLay->addStretch();
    stack->addWidget(pageSetup);

    // --- STRONA 2: Gra ---
    pageGame = new QWidget(this);
    QHBoxLayout *gameLay = new QHBoxLayout(pageGame);

    // Lewa: Wisielec
    QVBoxLayout *left = new QVBoxLayout();
    hangmanLabel = new QLabel(this);
    hangmanLabel->setStyleSheet("border: 1px solid #ccc; bg-color: white;");
    hangmanLabel->setAlignment(Qt::AlignCenter);
    left->addWidget(hangmanLabel);
    errorsLabel = new QLabel("Błędy: 0/0");
    errorsLabel->setAlignment(Qt::AlignCenter);
    left->addWidget(errorsLabel);
    gameLay->addLayout(left, 1);

    // Prawa: Słowo i Klawisze
    QVBoxLayout *right = new QVBoxLayout();
    maskedWordLabel = new QLabel("...");
    maskedWordLabel->setStyleSheet("font-size: 24px; font-weight: bold;");
    maskedWordLabel->setAlignment(Qt::AlignCenter);
    right->addWidget(maskedWordLabel);

    statusLabel = new QLabel("");
    statusLabel->setAlignment(Qt::AlignCenter);
    right->addWidget(statusLabel);

    QWidget *kbd = new QWidget();
    QGridLayout *grid = new QGridLayout(kbd);
    QString chars = "AĄBCĆDEĘFGHIJKLŁMNŃOÓPQRSŚTUVWXYZŹŻ";
    int r=0, c=0;
    for(QChar ch : chars) {
        QPushButton *b = new QPushButton(QString(ch));
        b->setFixedSize(35,35);
        connect(b, &QPushButton::clicked, this, &WisielecWindow::onLetterClicked);
        letterButtons[ch] = b;
        grid->addWidget(b, r, c++);
        if(c>8) { c=0; r++; }
    }
    right->addWidget(kbd);
    gameLay->addLayout(right, 1);

    stack->addWidget(pageGame);
}

void WisielecWindow::initGame() {
    switch(config.mode) {
    case GameMode::Solo:
        stack->setCurrentWidget(pageGame);
        logic->generateRandomWord();
        resetBoard();
        break;

    case GameMode::LocalDuo:
        stack->setCurrentWidget(pageSetup);
        break;

    case GameMode::NetHost:
        stack->setCurrentWidget(pageWait);
        waitLabel->setText("Uruchamianie serwera...");
        server = new QTcpServer(this);
        connect(server, &QTcpServer::newConnection, this, &WisielecWindow::onNewConnection);
        if(server->listen(QHostAddress::Any, config.port)) {
            waitLabel->setText("Serwer nasłuchuje na porcie " + QString::number(config.port) + "\nOczekiwanie na gracza...");
        } else {
            QMessageBox::critical(this, "Błąd", "Nie można uruchomić serwera");
        }
        break;

    case GameMode::NetClient:
        stack->setCurrentWidget(pageWait);
        waitLabel->setText("Łączenie z " + config.hostIp + "...");
        socket = new QTcpSocket(this);
        connect(socket, &QTcpSocket::connected, this, &WisielecWindow::onConnected);
        connect(socket, &QTcpSocket::readyRead, this, &WisielecWindow::onSocketReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &WisielecWindow::onSocketDisconnected);
        connect(socket, &QTcpSocket::errorOccurred, this, &WisielecWindow::onConnectionError);
        socket->connectToHost(config.hostIp, config.port);
        break;
    }
}

// --- Logika Sieciowa i Sterowanie ---

void WisielecWindow::onNewConnection() {
    if(socket) socket->close();
    socket = server->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &WisielecWindow::onSocketReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &WisielecWindow::onSocketDisconnected);

    // Host przechodzi do ustawiania słowa
    stack->setCurrentWidget(pageSetup);
}

void WisielecWindow::onConnected() {
    waitLabel->setText("Połączono. Czekam na ustawienie słowa przez Hosta...");
}

void WisielecWindow::confirmWord() {
    QString w = wordInput->text();
    if(!game_logic::isValidWord(w.toUpper())) {
        QMessageBox::warning(this, "Błąd", "Nieprawidłowe znaki");
        return;
    }
    logic->setWord(w);

    if(config.mode == GameMode::NetHost) {
        // Host wysyła start, wchodzi w tryb podglądu
        stack->setCurrentWidget(pageGame);
        resetBoard();
        for(auto b : letterButtons) b->setEnabled(false); // Host nie klika
        statusLabel->setText("Klient zgaduje...");
        onWordSet(logic->getMaskedWord()); // Trigger network send
    } else {
        // Local Duo
        stack->setCurrentWidget(pageGame);
        resetBoard();
    }
}

void WisielecWindow::onLetterClicked() {
    QPushButton *b = qobject_cast<QPushButton*>(sender());
    if(!b) return;
    QChar c = b->text()[0];

    if(config.mode == GameMode::NetClient) {
        sendNetworkPacket("GUESS", QString(c));
        b->setEnabled(false);
    } else {
        logic->guessLetter(c);
    }
}

// --- Protokół Sieciowy ---

void WisielecWindow::sendNetworkPacket(const QString &type, const QString &payload) {
    if(socket && socket->state() == QAbstractSocket::ConnectedState) {
        QString msg = type + "|" + payload + "\n";
        socket->write(msg.toUtf8());
    }
}

void WisielecWindow::onSocketReadyRead() {
    while(socket->canReadLine()) {
        QString line = QString::fromUtf8(socket->readLine()).trimmed();
        QStringList parts = line.split('|');
        if(parts.empty()) continue;

        QString type = parts[0];
        QString data = parts.length() > 1 ? parts[1] : "";

        if(config.mode == GameMode::NetHost && type == "GUESS") {
            if(!data.isEmpty()) logic->guessLetter(data[0]);
        }
        else if(config.mode == GameMode::NetClient && type == "UPDATE") {
            updateNetworkUI(data);
        }
    }
}

void WisielecWindow::updateNetworkUI(const QString &payload) {
    // Format: MASK;ERR;MAX;STATE;USED
    QStringList p = payload.split(';');
    if(p.size() < 5) return;

    if(stack->currentWidget() != pageGame) {
        stack->setCurrentWidget(pageGame);
        resetBoard();
    }

    maskedWordLabel->setText(p[0]);
    int err = p[1].toInt();
    int max = p[2].toInt();
    int st = p[3].toInt();
    QString used = p[4];

    errorsLabel->setText(QString("Błędy: %1/%2").arg(err).arg(max));

    // Rysowanie
    int img = 0;
    if(err > 0 && max > 0) {
        img = std::round((float)err/max * 8.0f);
        if(img==0 && err>0) img=1;
        if(err>=max) img=8;
    }
    if(hangmanImages.contains(img)) hangmanLabel->setPixmap(hangmanImages[img]);

    // Klawisze
    for(auto k : letterButtons.keys()) {
        if(used.contains(k)) letterButtons[k]->setEnabled(false);
    }

    game_logic::GameState gState = (game_logic::GameState)st;
    if(gState == game_logic::GameState::Playing) statusLabel->setText("Twoja kolej!");
    else if(gState == game_logic::GameState::Won) {
        QMessageBox::information(this, "Koniec", "WYGRANA!");
        emit gameClosed();
    }
    else if(gState == game_logic::GameState::Lost) {
        QMessageBox::information(this, "Koniec", "PRZEGRANA!");
        emit gameClosed();
    }
}

// --- Aktualizacja UI (Lokalna/Host) ---

void WisielecWindow::onWordSet(const QString &masked) {
    maskedWordLabel->setText(masked);
    updateHangmanImage();

    if(config.mode == GameMode::NetHost) {
        // Sync początkowy
        QString pl = QString("%1;0;%2;%3;")
                         .arg(masked).arg(logic->getMaxErrors())
                         .arg((int)game_logic::GameState::Playing);
        sendNetworkPacket("UPDATE", pl);
    }
}

void WisielecWindow::onLetterGuessed(QChar l, bool corr) {
    if(letterButtons.contains(l)) {
        QPushButton *b = letterButtons[l];
        b->setEnabled(false);
        b->setStyleSheet(corr ? "background-color: #4CAF50" : "background-color: #f44336");
    }
    maskedWordLabel->setText(logic->getMaskedWord());
    updateHangmanImage();

    if(config.mode == GameMode::NetHost) {
        QString used;
        for(auto c : logic->getUsedLetters()) used += c;
        QString pl = QString("%1;%2;%3;%4;%5")
                         .arg(logic->getMaskedWord())
                         .arg(logic->getErrors()).arg(logic->getMaxErrors())
                         .arg((int)logic->getState()).arg(used);
        sendNetworkPacket("UPDATE", pl);
    }
}

void WisielecWindow::onGameStateChanged(game_logic::GameState ns) {
    if(config.mode == GameMode::NetClient) return; // Klient ma to z UPDATE

    if(ns == game_logic::GameState::Won || ns == game_logic::GameState::Lost) {
        QString msg = (ns == game_logic::GameState::Won) ? "WYGRANA!" : "PRZEGRANA!";
        msg += "\nHasło: " + logic->getWord();
        QMessageBox::information(this, "Koniec", msg);
        emit gameClosed(); // Wróć do launchera
    }
}

void WisielecWindow::onErrorsChanged(int err) {
    errorsLabel->setText(QString("Błędy: %1/%2").arg(err).arg(logic->getMaxErrors()));
}

void WisielecWindow::updateHangmanImage() {
    int cur = logic->getErrors();
    int max = logic->getMaxErrors();
    int idx = 0;
    if(cur>0 && max>0) {
        idx = std::round((float)cur/max * 8.0f);
        if(idx==0 && cur>0) idx=1;
        if(cur>=max) idx=8;
    }
    if(hangmanImages.contains(idx)) hangmanLabel->setPixmap(hangmanImages[idx]);
}

void WisielecWindow::resetBoard() {
    for(auto b : letterButtons) {
        b->setEnabled(true);
        b->setStyleSheet("");
    }
    updateHangmanImage();
}

void WisielecWindow::onConnectionError(QAbstractSocket::SocketError) {
    QMessageBox::critical(this, "Błąd Sieci", "Błąd połączenia!");
    emit gameClosed();
}
void WisielecWindow::onSocketDisconnected() {
    QMessageBox::information(this, "Info", "Rozłączono.");
    emit gameClosed();
}

void WisielecWindow::createHangmanImages() {
    // Generowanie grafik (takie samo jak wcześniej)
    for(int i=0; i<=8; i++) {
        QPixmap p(200, 300); p.fill(Qt::white);
        QPainter pt(&p); pt.setPen(QPen(Qt::black, 3));
        if(i>=1) pt.drawLine(20,280,180,280);
        if(i>=2) pt.drawLine(50,280,50,20);
        if(i>=3) pt.drawLine(50,20,120,20);
        if(i>=4) pt.drawLine(120,20,120,50);
        if(i>=5) pt.drawEllipse(100,50,40,40);
        if(i>=6) pt.drawLine(120,90,120,180);
        if(i>=7) { pt.drawLine(120,110,90,150); pt.drawLine(120,110,150,150); }
        if(i>=8) { pt.drawLine(120,180,90,230); pt.drawLine(120,180,150,230); }
        hangmanImages[i] = p;
    }
}
