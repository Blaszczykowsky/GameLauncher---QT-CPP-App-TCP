#include <QApplication>
#include "launcher.h"
#include "wisielec_window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    Launcher launcher;
    WisielecWindow *gameWindow = nullptr;

    QObject::connect(&launcher, &Launcher::launchGame, [&](const GameLaunchConfig &config){
        if(config.gameType == GameType::Wisielec) {
            gameWindow = new WisielecWindow(config);

            QObject::connect(gameWindow, &WisielecWindow::gameClosed, [&](){
                gameWindow->deleteLater();
                gameWindow = nullptr;
                launcher.show();
            });

            launcher.hide();
            gameWindow->show();
        }
    });

    launcher.show();

    return app.exec();
}
