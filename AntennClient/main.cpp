#include "AntennaClient.h"
#include <QApplication>
#include <QStyleFactory>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("Antenna Client");
    app.setOrganizationName("AntennaEmulator");

    // Устанавливаем стиль
    app.setStyle(QStyleFactory::create("Fusion"));

    AntennaClient client;
    client.show();

    return app.exec();
}
