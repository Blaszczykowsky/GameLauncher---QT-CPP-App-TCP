#include "game_logic.h"
#include <QRegularExpression>
#include <QRandomGenerator>

game_logic::game_logic(QObject *parent)
    : QObject(parent), errors(0), maxErrors(8), state(GameState::WaitingForWord)
{
    dictionary << "PROGRAMOWANIE" << "KOMPUTER" << "INTERNET" << "KLAWIATURA"
               << "RZEKA" << "SAMOCHÓD" << "KSIĄŻKA" << "TELEFON"
               << "WARSZAWA" << "KRAKÓW" << "PROGRAMISTA" << "APLIKACJA"
               << "MONITOR" << "MYSZKA" << "GŁOŚNIKI" << "SŁUCHAWKI"
               << "ŻÓŁĆ" << "GĘŚ" << "CHRZĄSZCZ" << "SZCZĘŚCIE";
}

void game_logic::generateRandomWord()
{
    if (dictionary.isEmpty()) return;
    int index = QRandomGenerator::global()->bounded(dictionary.size());
    setWord(dictionary[index]);
}

bool game_logic::setWord(const QString &newWord)
{
    QString trimmed = newWord.trimmed().toUpper();
    if (!isValidWord(trimmed)) return false;

    word = trimmed;
    usedLetters.clear();
    errors = 0;

    int calculatedMax = word.length() + 2;
    if (calculatedMax < 5) calculatedMax = 5;
    if (calculatedMax > 15) calculatedMax = 15;
    maxErrors = calculatedMax;

    state = GameState::Playing;

    emit wordSet(getMaskedWord());
    emit gameStateChanged(state);
    emit errorsChanged(errors);
    return true;
}

bool game_logic::guessLetter(QChar letter)
{
    if (state != GameState::Playing) return false;

    QChar upper = letter.toUpper();
    if (!isValidLetter(upper) || usedLetters.contains(upper)) return false;

    usedLetters.insert(upper);
    bool correct = word.contains(upper);

    if (!correct) {
        errors++;
        emit errorsChanged(errors);
    }
    emit letterGuessed(upper, correct);
    checkWinCondition();
    return true;
}

void game_logic::resetGame()
{
    word.clear();
    usedLetters.clear();
    errors = 0;
    state = GameState::WaitingForWord;
    emit gameStateChanged(state);
    emit errorsChanged(errors);
}

QString game_logic::getMaskedWord() const { return createMask(); }
game_logic::GameState game_logic::getState() const { return state; }
int game_logic::getErrors() const { return errors; }
int game_logic::getMaxErrors() const { return maxErrors; }
QSet<QChar> game_logic::getUsedLetters() const { return usedLetters; }
QString game_logic::getWord() const { return word; }

bool game_logic::isValidWord(const QString &word) {
    static QRegularExpression regex("^[A-ZĄĆĘŁŃÓŚŹŻ ]+$");
    return regex.match(word).hasMatch();
}

bool game_logic::isValidLetter(QChar letter) { return letter.isLetter(); }

void game_logic::checkWinCondition() {
    if (errors >= maxErrors) {
        state = GameState::Lost;
        emit gameStateChanged(state);
        return;
    }
    for (QChar c : word) {
        if (c != ' ' && !usedLetters.contains(c)) return;
    }
    state = GameState::Won;
    emit gameStateChanged(state);
}

QString game_logic::createMask() const {
    QString mask;
    for (QChar c : word) {
        if (c == ' ') mask += "  ";
        else if (usedLetters.contains(c)) mask += QString(c) + " ";
        else mask += "_ ";
    }
    return mask.trimmed();
}
