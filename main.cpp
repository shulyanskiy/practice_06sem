#include <QApplication>
#include <QThread>
#include "include/AntennaClient.h"
#include "include/MyAntenn.h"
#include "include/emulator.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // Создаем объекты
    MyAntenn* antenn = new MyAntenn();
    EmuManager* manager = new EmuManager(antenn);

    // Создаем отдельные потоки
    QThread* antennaThread = new QThread();
    QThread* serverThread = new QThread();

    // Перемещаем объекты в их потоки
    antenn->moveToThread(antennaThread);
    manager->moveToThread(serverThread);

    // Запуск потоков
    QObject::connect(antennaThread, &QThread::started, [antenn]() {
        antenn->start();
        });

    QObject::connect(serverThread, &QThread::started, [manager]() {
        manager->start(12345);
        });

    // Остановка потоков
    QObject::connect(serverThread, &QThread::finished, [manager]() {
        manager->stop();
        });

    QObject::connect(antennaThread, &QThread::finished, [antenn]() {
        antenn->stop();
        });

    // Запускаем потоки
    antennaThread->start();
    serverThread->start();

    // Даем время на инициализацию
    QThread::msleep(100);

    // Запускаем клиент в главном потоке
    AntennaClient client;
    client.show();

    int result = app.exec();

    // Останавливаем потоки
    serverThread->quit();
    serverThread->wait();
    antennaThread->quit();
    antennaThread->wait();

    delete serverThread;
    delete antennaThread;
    delete manager;
    delete antenn;

    return result;
}