#include <QCoreApplication>
#include <QLoggingCategory>
#include "serwer_wisielec.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    SerwerWisielec serwer(12345);
    return app.exec();
}
