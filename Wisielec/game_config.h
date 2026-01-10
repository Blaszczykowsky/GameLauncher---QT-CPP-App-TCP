#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

#include <QString>

enum class GameType {
    Wisielec,
    Kosci,
    Chinczyk
};

enum class GameMode {
    Solo,
    LocalDuo,
    NetHost,
    NetClient
};

struct GameLaunchConfig {
    GameType gameType;
    GameMode mode;
    QString hostIp;
    int port;
};

#endif
