#include <QApplication>
#include "EmuApp.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MyServer     mServer(2323);

    mServer.show();

    return app.exec();
}
