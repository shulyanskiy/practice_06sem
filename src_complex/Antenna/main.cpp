// antenna_main.cpp
#include "tcpantenna.h"
#include <QCoreApplication>
#include <QDebug>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    TcpAntenna* antenna = new TcpAntenna(&app);

    // === ПОДКЛЮЧЕНИЕ СИГНАЛОВ ЛОГИРОВАНИЯ ===
    QObject::connect(
        antenna, &TcpAntenna::infoOccurred,
        &app, [](const QString msg) {
            qDebug().noquote() << QString("[INFO] %1").arg(msg);
        }
    );

    QObject::connect(
        antenna, &TcpAntenna::warningOccurred,
        &app, [](const QString warning) {
            qDebug().noquote() << QString("[WARNING] %1").arg(warning);
        }
    );

    QObject::connect(
        antenna, &TcpAntenna::errorOccurred,
        &app, [](const QString error) {
            qDebug().noquote() << QString("[ERROR] %1").arg(error);
        }
    );

    // === ПОДКЛЮЧЕНИЕ СИГНАЛОВ СОСТОЯНИЯ ===
    QObject::connect(
        antenna, &TcpAntenna::azimuthChanged,
        &app, [](double az) {
            qDebug().noquote() << QString("[AZIMUTH] %1").arg(az, 0, 'f', 2);
        }
    );

    QObject::connect(
        antenna, &TcpAntenna::elevationChanged,
        &app, [](double el) {
            qDebug().noquote() << QString("[ELEVATION] %1").arg(el, 0, 'f', 2);
        }
    );

    QObject::connect(
        antenna, &TcpAntenna::positionReached,
        &app, [](double az, double el) {
            qDebug().noquote() << QString("[POSITION] Reached: AZ=%1 EL=%2")
                .arg(az, 0, 'f', 2)
                .arg(el, 0, 'f', 2);
        }
    );

    QObject::connect(
        antenna, &TcpAntenna::connected,
        &app, []() {
            qDebug().noquote() << "[CONNECTION] Connected to transport";
        }
    );

    QObject::connect(
        antenna, &TcpAntenna::disconnected,
        &app, []() {
            qDebug().noquote() << "[CONNECTION] Disconnected from transport";
        }
    );

    // === ПОДКЛЮЧЕНИЕ К ТРАНСПОРТУ ===
    antenna->connectToServer("127.0.0.1", 2323);

    // === ЗАПУСК ===
    antenna->start();

    qDebug().noquote() << "========================================";
    qDebug().noquote() << "ANTENNA IS RUNNING";
    qDebug().noquote() << "  Connected to transport on port: 2323";
    qDebug().noquote() << "  Status: Waiting for commands...";
    qDebug().noquote() << "========================================";

    return app.exec();
}