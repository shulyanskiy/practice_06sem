#include "include/ServerWidget.h"
#include <QApplication>
#include <QStyleFactory>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("Antenna Server");
    app.setOrganizationName("AntennaEmulator");

    // Устанавливаем стиль
    app.setStyle(QStyleFactory::create("Fusion"));

    ServerWidget server;
    server.setWindowTitle("Antenna Emulator Server");
    server.resize(1100, 800);
    server.show();

    return app.exec();
}
