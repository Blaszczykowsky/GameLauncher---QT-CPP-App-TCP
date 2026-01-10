#ifndef WISIELEC_WINDOW_H
#define WISIELEC_WINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include "game_logic.h"
#include "game_config.h"

class WisielecWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit WisielecWindow(const GameLaunchConfig &config, QWidget *parent = nullptr);
    ~WisielecWindow();

signals:
    void gameClosed();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onNewConnection();
    void onSocketReadyRead();
    void onSocketDisconnected();
    void onConnected();
    void onConnectionError(QAbstractSocket::SocketError err);

    void confirmWord();
    void onLetterClicked();

    void onWordSet(const QString &masked);
    void onLetterGuessed(QChar c, bool correct);
    void onGameStateChanged(game_logic::GameState newState);
    void onErrorsChanged(int err);

private:
    void initGame();
    void setupUI();
    void createHangmanImages();
    void updateHangmanImage();

    void processNetworkPacket(const QString &type, const QString &payload);
    void sendNetworkPacket(const QString &type, const QString &payload);
    void updateNetworkUI(const QString &payload);

    void resetBoard();
    void startNextRound();
    void handleGameOver(bool won);

    GameLaunchConfig config;
    game_logic *logic;

    QTcpServer *server;
    QTcpSocket *socket;

    bool amISetter;

    QStackedWidget *stack;
    QWidget *pageSetup;
    QWidget *pageGame;
    QWidget *pageWait;

    QLineEdit *wordInput;
    QLabel *statusLabel;
    QLabel *waitLabel;

    QLabel *hangmanLabel;
    QLabel *maskedWordLabel;
    QLabel *errorsLabel;
    QMap<QChar, QPushButton*> letterButtons;
    QMap<int, QPixmap> hangmanImages;
};

#endif
