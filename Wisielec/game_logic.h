#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include <QString>
#include <QSet>
#include <QObject>
#include <QStringList>

class WisielecLogic : public QObject
{
    Q_OBJECT

public:
    enum class GameState
    {
        WaitingForWord,
        Playing,
        Won,
        Lost
    };

    explicit WisielecLogic(QObject *parent = nullptr);

    bool setWord(const QString &word);
    void generateRandomWord();
    bool guessLetter(QChar letter);
    void resetGame();

    QString getMaskedWord() const;
    GameState getState() const;
    int getErrors() const;
    int getMaxErrors() const;
    QSet<QChar> getUsedLetters() const;
    QString getWord() const;

    static bool isValidWord(const QString &word);
    static bool isValidLetter(QChar letter);

signals:
    void wordSet(const QString &maskedWord);
    void letterGuessed(QChar letter, bool correct);
    void gameStateChanged(GameState newState);
    void errorsChanged(int errors);

private:
    QString word;
    QSet<QChar> usedLetters;
    int errors;
    int maxErrors;
    GameState state;
    QStringList dictionary;

    void checkWinCondition();
    QString createMask() const;
};

#endif
