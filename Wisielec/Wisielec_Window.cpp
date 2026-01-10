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

void WisielecWindow::setupUI() {
    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);

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
    QLabel *setupLbl = new QLabel("Twoja kolej! Wpisz hasło dla przeciwnika:", this);
    setupLbl->setStyleSheet("font-size: 16px; font-weight: bold;");
    setupLbl->setAlignment(Qt::AlignCenter);
    setupLay->addWidget(setupLbl);

    wordInput = new QLineEdit(this);
    wordInput->setEchoMode(QLineEdit::Password);
    wordInput->setAlignment(Qt::AlignCenter);
    setupLay->addWidget(wordInput);

    QPushButton *btnConfirm = new QPushButton("Zatwierdź Słowo", this);
    btnConfirm->setMinimumHeight(40);
    connect(btnConfirm, &QPushButton::clicked, this, &WisielecWindow::confirmWord);
    setupLay->addWidget(btnConfirm);
    setupLay->addStretch();
    stack->addWidget(pageSetup);

    pageGame = new QWidget(this);
    QHBoxLayout *gameLay = new QHBoxLayout(pageGame);

    QVBoxLayout *left = new QVBoxLayout();
    hangmanLabel = new QLabel(this);
    hangmanLabel->setStyleSheet("border: 2px solid #ccc; background-color: white; border-radius: 10px;");
    hangmanLabel->setAlignment(Qt::AlignCenter);
    left->addWidget(hangmanLabel);
    errorsLabel = new QLabel("Błędy: 0/0");
    errorsLabel->setAlignment(Qt::AlignCenter);
    errorsLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
    left->addWidget(errorsLabel);
    gameLay->addLayout(left, 1);

    QVBoxLayout *right = new QVBoxLayout();
    maskedWordLabel = new QLabel("...");
    maskedWordLabel->setStyleSheet("font-size: 28px; font-weight: bold; letter-spacing: 4px;");
    maskedWordLabel->setAlignment(Qt::AlignCenter);
    right->addWidget(maskedWordLabel);

    statusLabel = new QLabel("");
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet("font-size: 14px; margin: 10px;");
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
        amISetter = false;
        stack->setCurrentWidget(pageGame);
        logic->generateRandomWord();
        resetBoard();
        break;
    case GameMode::LocalDuo:
        amISetter = true;
        stack->setCurrentWidget(pageSetup);
        break;
    case GameMode::NetHost:
        stack->setCurrentWidget(pageWait);
        waitLabel->setText("Serwer: Oczekiwanie na gracza...");
        server = new QTcpServer(this);
        connect(server, &QTcpServer::newConnection, this, &WisielecWindow::onNewConnection);
        if(!server->listen(QHostAddress::Any, config.port)) {
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

void WisielecWindow::startNextRound() {
    if(config.mode == GameMode::NetHost || config.mode == GameMode::NetClient) {
        amISetter = !amISetter;
    } else if (config.mode == GameMode::Solo) {
        logic->generateRandomWord();
        resetBoard();
        return;
    }

    if(config.mode != GameMode::NetClient) {
        logic->resetGame();

        if(config.mode == GameMode::NetHost) {
            // Dodano 6. parametr: amISetter (czy Host ustawia)
            // Jeśli amISetter==true (Host), to wysyłamy 1.
            QString pl = QString("...;0;%1;%2;;%3")
                             .arg(logic->getMaxErrors())
                             .arg((int)game_logic::GameState::WaitingForWord)
                             .arg(amISetter ? 1 : 0);
            sendNetworkPacket("UPDATE", pl);
        }
    }

    wordInput->clear();
    resetBoard();

    if(amISetter) {
        stack->setCurrentWidget(pageSetup);
    } else {
        stack->setCurrentWidget(pageWait);
        waitLabel->setText("Przeciwnik ustawia słowo...");
    }
}

void WisielecWindow::onNewConnection() {
    if(socket) socket->close();
    socket = server->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &WisielecWindow::onSocketReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &WisielecWindow::onSocketDisconnected);

    stack->setCurrentWidget(pageSetup);
}

void WisielecWindow::onConnected() {
    waitLabel->setText("Połączono. Czekam na ruch Hosta...");
}

void WisielecWindow::confirmWord() {
    QString w = wordInput->text().trimmed();
    if(!game_logic::isValidWord(w.toUpper())) {
        QMessageBox::warning(this, "Błąd", "Nieprawidłowe znaki! Tylko litery.");
        return;
    }

    if(config.mode == GameMode::NetClient && amISetter) {
        sendNetworkPacket("SET_WORD", w);
        stack->setCurrentWidget(pageGame);
        statusLabel->setText("Obserwujesz grę Hosta...");
        for(auto b : letterButtons) b->setEnabled(false);
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
        QString msg = type + "|" + payload + "\n";
        socket->write(msg.toUtf8());
    }
}

void WisielecWindow::onSocketReadyRead() {
    while(socket->canReadLine()) {
        QString line = QString::fromUtf8(socket->readLine()).trimmed();
        processNetworkPacket("", line);
    }
}

void WisielecWindow::processNetworkPacket(const QString &, const QString &line) {
    QStringList parts = line.split('|');
    if(parts.empty()) return;

    QString type = parts[0];
    QString data = parts.length() > 1 ? parts[1] : "";

    if(config.mode == GameMode::NetHost) {
        if(type == "GUESS") {
            if(!data.isEmpty()) logic->guessLetter(data[0]);
        }
        else if(type == "SET_WORD") {
            logic->setWord(data);
        }
    }
    else if(config.mode == GameMode::NetClient) {
        if(type == "UPDATE") {
            updateNetworkUI(data);
        }
    }
}

void WisielecWindow::updateNetworkUI(const QString &payload) {
    QStringList p = payload.split(';');
    if(p.size() < 4) return;

    int st = p[3].toInt();
    game_logic::GameState gState = (game_logic::GameState)st;

    // Odczytaj rolę z pakietu (parametr 6), jeśli dostępny
    if(p.size() > 5) {
        bool hostIsSetter = (p[5].toInt() == 1);
        // Jeśli Host ustawia, to ja (Klient) zgaduję -> amISetter = false
        // Jeśli Host zgaduje, to ja (Klient) ustawiam -> amISetter = true
        amISetter = !hostIsSetter;
    }

    if (gState == game_logic::GameState::WaitingForWord) {
        wordInput->clear();
        if(amISetter) {
            if(stack->currentWidget() != pageSetup) {
                stack->setCurrentWidget(pageSetup);
            }
        } else {
            if(stack->currentWidget() != pageWait) {
                stack->setCurrentWidget(pageWait);
                waitLabel->setText("Przeciwnik ustawia słowo...");
            }
        }
        return;
    }

    if(stack->currentWidget() != pageGame) {
        stack->setCurrentWidget(pageGame);
        resetBoard();

        if(amISetter) {
            for(auto b : letterButtons) b->setEnabled(false);
            statusLabel->setText("Przeciwnik zgaduje Twoje słowo...");
        } else {
            statusLabel->setText("Zgaduj hasło!");
        }
    }

    maskedWordLabel->setText(p[0]);
    int err = p[1].toInt();
    int max = p[2].toInt();
    QString used = (p.size() > 4) ? p[4] : "";

    errorsLabel->setText(QString("Błędy: %1/%2").arg(err).arg(max));

    int img = 0;
    if(err > 0 && max > 0) {
        img = std::round((float)err/max * 8.0f);
        if(img==0 && err>0) img=1;
        if(err>=max) img=8;
    }
    if(hangmanImages.contains(img)) hangmanLabel->setPixmap(hangmanImages[img]);

    for(auto k : letterButtons.keys()) {
        if(used.contains(k)) {
            QPushButton *btn = letterButtons[k];
            btn->setEnabled(false);

            bool isCorrect = false;
            if(p[0].contains(k)) isCorrect = true;
            if(amISetter && !maskedWordLabel->text().contains('_') && logic->getWord().contains(k)) isCorrect = true;

            btn->setStyleSheet(isCorrect ? "background-color: #4CAF50; color: white;"
                                         : "background-color: #f44336; color: white;");
        }
    }

    if(gState == game_logic::GameState::Won) handleGameOver(true);
    else if(gState == game_logic::GameState::Lost) handleGameOver(false);
}

void WisielecWindow::onWordSet(const QString &masked) {
    if(config.mode == GameMode::NetHost && !amISetter) {
        stack->setCurrentWidget(pageGame);
        resetBoard();
        statusLabel->setText("Zgaduj hasło Klienta!");
    } else if (config.mode == GameMode::NetHost && amISetter) {
        stack->setCurrentWidget(pageGame);
        resetBoard();
        for(auto b : letterButtons) b->setEnabled(false);
        statusLabel->setText("Klient zgaduje Twoje słowo...");
    } else if (config.mode == GameMode::LocalDuo) {
        stack->setCurrentWidget(pageGame);
        resetBoard();
        statusLabel->setText("Zgaduj!");
    }

    maskedWordLabel->setText(masked);
    updateHangmanImage();

    if(config.mode == GameMode::NetHost) {
        // Dodano 6 parametr: amISetter
        QString pl = QString("%1;0;%2;%3;;%4")
                         .arg(masked).arg(logic->getMaxErrors())
                         .arg((int)game_logic::GameState::Playing)
                         .arg(amISetter ? 1 : 0);
        sendNetworkPacket("UPDATE", pl);
    }
}

void WisielecWindow::onLetterGuessed(QChar l, bool corr) {
    if(letterButtons.contains(l)) {
        QPushButton *b = letterButtons[l];
        b->setEnabled(false);
        b->setStyleSheet(corr ? "background-color: #4CAF50; color: white;"
                              : "background-color: #f44336; color: white;");
    }
    maskedWordLabel->setText(logic->getMaskedWord());
    updateHangmanImage();

    if(config.mode == GameMode::NetHost) {
        QString used;
        for(auto c : logic->getUsedLetters()) used += c;
        // Dodano 6 parametr: amISetter
        QString pl = QString("%1;%2;%3;%4;%5;%6")
                         .arg(logic->getMaskedWord())
                         .arg(logic->getErrors()).arg(logic->getMaxErrors())
                         .arg((int)logic->getState()).arg(used)
                         .arg(amISetter ? 1 : 0);
        sendNetworkPacket("UPDATE", pl);
    }
}

void WisielecWindow::onGameStateChanged(game_logic::GameState ns) {
    if(config.mode == GameMode::NetClient) return;

    if(ns == game_logic::GameState::Won) handleGameOver(true);
    else if(ns == game_logic::GameState::Lost) handleGameOver(false);
}

void WisielecWindow::handleGameOver(bool won) {
    QString title, msg;

    if(amISetter) {
        title = won ? "Koniec Rundy" : "Sukces!";
        msg = won ? "Przeciwnik zgadł Twoje słowo!"
                  : "Przeciwnik nie zgadł! Wygrałeś rundę.";
    } else {
        title = won ? "WYGRANA!" : "PRZEGRANA";
        msg = won ? "Gratulacje! Odgadłeś hasło."
                  : "Niestety, wisielec kompletny.";
    }

    if(config.mode != GameMode::NetClient && config.mode != GameMode::NetHost) {
        msg += "\nSłowo to: " + logic->getWord();
    }

    QMessageBox::StandardButton reply;
    reply = QMessageBox::information(this, title, msg + "\n\nCzy chcecie zamienić się rolami i grać dalej?",
                                     QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        startNextRound();
    } else {
        emit gameClosed();
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
    QMessageBox::critical(this, "Błąd Sieci", "Utracono połączenie.");
    emit gameClosed();
}
void WisielecWindow::onSocketDisconnected() {
    QMessageBox::information(this, "Info", "Rozłączono.");
    emit gameClosed();
}

void WisielecWindow::createHangmanImages() {
    for(int i=0; i<=8; i++) {
        QPixmap p(300, 400); p.fill(Qt::white);
        QPainter pt(&p); pt.setRenderHint(QPainter::Antialiasing);
        QPen pen(Qt::black, 4); pt.setPen(pen);
        if(i>=1) pt.drawLine(50,380,250,380);
        if(i>=2) pt.drawLine(100,380,100,50);
        if(i>=3) pt.drawLine(100,50,200,50);
        if(i>=4) pt.drawLine(200,50,200,100);
        if(i>=5) pt.drawEllipse(175,100,50,50);
        if(i>=6) pt.drawLine(200,150,200,250);
        if(i>=7) { pt.drawLine(200,170,160,210); pt.drawLine(200,170,240,210); }
        if(i>=8) {
            pt.drawLine(200,250,170,320); pt.drawLine(200,250,230,320);
            pt.setPen(QPen(Qt::red, 2)); pt.drawArc(180,125,40,20,0,-180*16);
        }
        hangmanImages[i] = p;
    }
}
