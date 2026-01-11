#include "wisielec_window.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QPainter>
#include <cmath>
#include <QRandomGenerator>

const QString STYLE_BTN_DEFAULT = "QPushButton { background-color: #2196F3; color: white; font-weight: bold; border-radius: 5px; font-size: 14px; } QPushButton:hover { background-color: #1976D2; } QPushButton:disabled { background-color: #ccc; color: #666; }";
const QString STYLE_BTN_CORRECT = "QPushButton { background-color: #4CAF50; color: white; font-weight: bold; border-radius: 5px; font-size: 14px; }";
const QString STYLE_BTN_WRONG   = "QPushButton { background-color: #f44336; color: white; font-weight: bold; border-radius: 5px; font-size: 14px; }";

WisielecWindow::WisielecWindow(const GameLaunchConfig &cfg, QWidget *parent)
    : QMainWindow(parent), config(cfg), server(nullptr), socket(nullptr)
{
    logic = new WisielecLogic(this);

    connect(logic, &WisielecLogic::wordSet, this, &WisielecWindow::onWordSet);
    connect(logic, &WisielecLogic::letterGuessed, this, &WisielecWindow::onLetterGuessed);
    connect(logic, &WisielecLogic::gameStateChanged, this, &WisielecWindow::onGameStateChanged);
    connect(logic, &WisielecLogic::errorsChanged, this, &WisielecWindow::onErrorsChanged);

    setupUI();
    createHangmanImages();

    if(config.mode == GameMode::NetHost) amISetter = true;
    else if(config.mode == GameMode::NetClient) amISetter = false;
    else amISetter = true;

    initGame();
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

void WisielecWindow::onBackToMenu() {
    emit gameClosed();
}

void WisielecWindow::setupUI() {
    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);

    QHBoxLayout *topBar = new QHBoxLayout();
    QPushButton *btnMenu = new QPushButton("Powrót do Menu", this);
    connect(btnMenu, &QPushButton::clicked, this, &WisielecWindow::onBackToMenu);
    topBar->addWidget(btnMenu);
    topBar->addStretch();
    mainLayout->addLayout(topBar);

    stack = new QStackedWidget(this);
    mainLayout->addWidget(stack);

    pageWait = new QWidget(this);
    QVBoxLayout *waitLay = new QVBoxLayout(pageWait);
    waitLabel = new QLabel("Inicjalizacja...", this);
    waitLabel->setAlignment(Qt::AlignCenter);
    waitLabel->setStyleSheet("font-size: 18px;");
    waitLay->addWidget(waitLabel);
    stack->addWidget(pageWait);

    pageSetup = new QWidget(this);
    QVBoxLayout *setupLay = new QVBoxLayout(pageSetup);
    QLabel *setupLbl = new QLabel("Ustaw hasło dla przeciwnika:", this);
    setupLbl->setAlignment(Qt::AlignCenter);
    setupLay->addWidget(setupLbl);
    wordInput = new QLineEdit(this);
    wordInput->setEchoMode(QLineEdit::Password);
    wordInput->setAlignment(Qt::AlignCenter);
    setupLay->addWidget(wordInput);
    QPushButton *btnConfirm = new QPushButton("Zatwierdź", this);
    connect(btnConfirm, &QPushButton::clicked, this, &WisielecWindow::confirmWord);
    setupLay->addWidget(btnConfirm);
    setupLay->addStretch();
    stack->addWidget(pageSetup);

    pageGame = new QWidget(this);
    QHBoxLayout *gameLay = new QHBoxLayout(pageGame);

    QVBoxLayout *left = new QVBoxLayout();
    hangmanLabel = new QLabel(this);
    hangmanLabel->setStyleSheet("border: 2px solid #ccc; background: white;");
    hangmanLabel->setAlignment(Qt::AlignCenter);
    left->addWidget(hangmanLabel);
    errorsLabel = new QLabel("Błędy: 0/0");
    errorsLabel->setAlignment(Qt::AlignCenter);
    left->addWidget(errorsLabel);
    gameLay->addLayout(left, 1);

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
        b->setStyleSheet(STYLE_BTN_DEFAULT);
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
    if(config.mode == GameMode::Solo) {
        amISetter = false;
        stack->setCurrentWidget(pageGame);
        logic->generateRandomWord();
        resetBoard();
    }
    else if(config.mode == GameMode::LocalDuo) {
        amISetter = true;
        stack->setCurrentWidget(pageSetup);
    }
    else if(config.mode == GameMode::NetHost) {
        stack->setCurrentWidget(pageWait);
        waitLabel->setText("Serwer startuje na porcie " + QString::number(config.port));
        server = new QTcpServer(this);
        connect(server, &QTcpServer::newConnection, this, &WisielecWindow::onNewConnection);
        if(!server->listen(QHostAddress::Any, config.port)) {
            QMessageBox::critical(this, "Błąd", "Nie można uruchomić serwera!");
            emit gameClosed();
        }
    }
    else if(config.mode == GameMode::NetClient) {
        stack->setCurrentWidget(pageWait);
        waitLabel->setText("Łączenie...");
        socket = new QTcpSocket(this);
        connect(socket, &QTcpSocket::connected, this, &WisielecWindow::onConnected);
        connect(socket, &QTcpSocket::readyRead, this, &WisielecWindow::onSocketReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &WisielecWindow::onSocketDisconnected);
        socket->connectToHost(config.hostIp, config.port);
    }
}

void WisielecWindow::confirmWord() {
    QString w = wordInput->text().trimmed();
    if(!WisielecLogic::isValidWord(w.toUpper())) {
        QMessageBox::warning(this, "Błąd", "Tylko litery!");
        return;
    }

    if(config.mode == GameMode::NetClient && amISetter) {
        sendNetworkPacket("SET_WORD", w);
        stack->setCurrentWidget(pageGame);
        for(auto b : letterButtons) b->setEnabled(false);
        statusLabel->setText("Czekam na ruch Hosta...");
    } else {
        logic->setWord(w);
    }
}

void WisielecWindow::onLetterClicked() {
    QPushButton *b = qobject_cast<QPushButton*>(sender());
    if(!b) return;
    QChar c = b->text()[0];

    if(amISetter && config.mode != GameMode::LocalDuo) return;

    if(config.mode == GameMode::NetClient) {
        sendNetworkPacket("GUESS", QString(c));
        b->setEnabled(false);
    } else {
        logic->guessLetter(c);
    }
}

void WisielecWindow::sendNetworkPacket(const QString &type, const QString &payload) {
    if(socket && socket->state() == QAbstractSocket::ConnectedState) {
        socket->write((type + "|" + payload + "\n").toUtf8());
    }
}

void WisielecWindow::onSocketReadyRead() {
    while(socket->canReadLine()) {
        QString line = QString::fromUtf8(socket->readLine()).trimmed();
        processNetworkPacket("", line);
    }
}

void WisielecWindow::processNetworkPacket(const QString&, const QString &line) {
    QStringList parts = line.split('|');
    if(parts.empty()) return;
    QString type = parts[0];
    QString data = parts.length() > 1 ? parts[1] : "";

    if(config.mode == GameMode::NetHost) {
        if(type == "GUESS" && !data.isEmpty()) logic->guessLetter(data[0]);
        else if(type == "SET_WORD") logic->setWord(data);
    } else {
        if(type == "UPDATE") updateNetworkUI(data);
    }
}

void WisielecWindow::updateNetworkUI(const QString &payload) {
    QStringList p = payload.split(';');
    if(p.size() < 4) return;

    if(p.size() > 5) amISetter = !(p[5].toInt() == 1);

    int st = p[3].toInt();
    if(st == (int)WisielecLogic::GameState::WaitingForWord) {
        resetBoard();
        wordInput->clear();
        stack->setCurrentWidget(amISetter ? pageSetup : pageWait);
        if(!amISetter) waitLabel->setText("Przeciwnik ustawia słowo...");
        return;
    } else if (stack->currentWidget() != pageGame) {
        stack->setCurrentWidget(pageGame);
        resetBoard();
    }

    maskedWordLabel->setText(p[0]);
    errorsLabel->setText(QString("Błędy: %1/%2").arg(p[1]).arg(p[2]));

    int err = p[1].toInt();
    if(hangmanImages.contains(err)) hangmanLabel->setPixmap(hangmanImages[err]);

    QString used = p.size() > 4 ? p[4] : "";
    for(auto k : letterButtons.keys()) {
        if(used.contains(k)) {
            QPushButton *btn = letterButtons[k];
            btn->setEnabled(false);

            if(p[0].contains(k)) {
                btn->setStyleSheet(STYLE_BTN_CORRECT);
            } else {
                btn->setStyleSheet(STYLE_BTN_WRONG);
            }
        }
    }

    if(st == (int)WisielecLogic::GameState::Won) handleGameOver(true);
    else if(st == (int)WisielecLogic::GameState::Lost) handleGameOver(false);
}

void WisielecWindow::onWordSet(const QString &m) {
    if(stack->currentWidget() != pageGame) {
        stack->setCurrentWidget(pageGame);
        resetBoard();
    }

    if (config.mode != GameMode::LocalDuo && amISetter) {
        for(auto b : letterButtons) b->setEnabled(false);
        statusLabel->setText("Przeciwnik zgaduje Twoje hasło...");
    } else {
        statusLabel->setText("Zgaduj hasło!");
    }

    maskedWordLabel->setText(m);
    updateHangmanImage();

    if(config.mode == GameMode::NetHost) {
        QString pl = QString("%1;0;%2;%3;;%4").arg(m).arg(logic->getMaxErrors())
        .arg((int)WisielecLogic::GameState::Playing).arg(amISetter?1:0);
        sendNetworkPacket("UPDATE", pl);
    }
}

void WisielecWindow::onLetterGuessed(QChar c, bool ok) {
    if(letterButtons.contains(c)) {
        QPushButton *btn = letterButtons[c];
        btn->setEnabled(false);
        btn->setStyleSheet(ok ? STYLE_BTN_CORRECT : STYLE_BTN_WRONG);
    }

    maskedWordLabel->setText(logic->getMaskedWord());
    updateHangmanImage();

    if(config.mode == GameMode::NetHost) {
        QString u; for(auto x : logic->getUsedLetters()) u+=x;
        QString pl = QString("%1;%2;%3;%4;%5;%6")
                         .arg(logic->getMaskedWord()).arg(logic->getErrors()).arg(logic->getMaxErrors())
                         .arg((int)logic->getState()).arg(u).arg(amISetter?1:0);
        sendNetworkPacket("UPDATE", pl);
    }
}

void WisielecWindow::onGameStateChanged(WisielecLogic::GameState s) {
    if(config.mode != GameMode::NetClient && (s==WisielecLogic::GameState::Won || s==WisielecLogic::GameState::Lost))
        handleGameOver(s==WisielecLogic::GameState::Won);
}

void WisielecWindow::onErrorsChanged(int e) { errorsLabel->setText(QString("Błędy: %1").arg(e)); }

void WisielecWindow::createHangmanImages() {
    for(int i=0; i<=8; i++) {
        QPixmap p(300, 400); p.fill(Qt::white);
        QPainter pt(&p); pt.setPen(QPen(Qt::black, 3));
        if(i>=1) pt.drawLine(50,380,250,380);
        if(i>=2) pt.drawLine(100,380,100,50);
        if(i>=3) pt.drawLine(100,50,200,50);
        if(i>=4) pt.drawLine(200,50,200,100);
        if(i>=5) pt.drawEllipse(175,100,50,50);
        if(i>=6) pt.drawLine(200,150,200,250);
        if(i>=7) { pt.drawLine(200,170,160,210); pt.drawLine(200,170,240,210); }
        if(i>=8) { pt.drawLine(200,250,170,320); pt.drawLine(200,250,230,320); }
        hangmanImages[i] = p;
    }
}

void WisielecWindow::updateHangmanImage() {
    int e = logic->getErrors();
    if(hangmanImages.contains(e)) hangmanLabel->setPixmap(hangmanImages[e]);
}

void WisielecWindow::startNextRound() {
    if(config.mode == GameMode::NetHost || config.mode == GameMode::NetClient) amISetter = !amISetter;

    if(config.mode != GameMode::NetClient) {
        logic->resetGame();

        if(config.mode == GameMode::Solo) {
            logic->generateRandomWord();
        }
        else if(config.mode == GameMode::NetHost) {
            QString pl = QString("...;0;%1;%2;;%3")
            .arg(logic->getMaxErrors())
                .arg((int)WisielecLogic::GameState::WaitingForWord)
                .arg(amISetter ? 1 : 0);
            sendNetworkPacket("UPDATE", pl);
        }
    }

    wordInput->clear();
    resetBoard();

    if(config.mode == GameMode::Solo) {
        stack->setCurrentWidget(pageGame);
    } else {
        stack->setCurrentWidget(amISetter ? pageSetup : pageWait);
        if(!amISetter) waitLabel->setText("Przeciwnik ustawia słowo...");
    }
}

void WisielecWindow::resetBoard() {
    for(auto b : letterButtons) {
        b->setEnabled(true);
        b->setStyleSheet(STYLE_BTN_DEFAULT);
    }
    updateHangmanImage();
}

void WisielecWindow::handleGameOver(bool won) {
    QMessageBox::StandardButton r = QMessageBox::information(this, won?"Wygrana":"Przegrana",
                                                             "Koniec gry. Rewanż?", QMessageBox::Yes|QMessageBox::No);
    if(r == QMessageBox::Yes) startNextRound();
    else emit gameClosed();
}

void WisielecWindow::onNewConnection() {
    if(socket) socket->close();
    socket = server->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &WisielecWindow::onSocketReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &WisielecWindow::onSocketDisconnected);
    stack->setCurrentWidget(pageSetup);
}

void WisielecWindow::onSocketDisconnected() {
    QMessageBox::warning(this, "Rozłączono", "Połączenie zostało zerwane.");
    emit gameClosed();
}

void WisielecWindow::onConnectionError(QAbstractSocket::SocketError) {
    QMessageBox::warning(this, "Błąd", "Błąd połączenia.");
    emit gameClosed();
}

void WisielecWindow::onConnected() { waitLabel->setText("Połączono."); }
