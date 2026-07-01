// server_main.cpp
#include "tcpserver.h"
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    TcpServer* server = new TcpServer(&app);

    // === ПОДКЛЮЧЕНИЕ СИГНАЛОВ ЛОГИРОВАНИЯ ===
    QObject::connect(
        server, &TcpServer::infoOccurred,
        &app, [](const QString msg) {
            qDebug().noquote() << QString("[INFO] %1").arg(msg);
        }
    );

    QObject::connect(
        server, &TcpServer::warningOccurred,
        &app, [](const QString warning) {
            qDebug().noquote() << QString("[WARNING] %1").arg(warning);
        }
    );

    QObject::connect(
        server, &TcpServer::errorOccurred,
        &app, [](const QString error) {
            qDebug().noquote() << QString("[ERROR] %1").arg(error);
        }
    );

    // === ПОДКЛЮЧЕНИЕ СИГНАЛОВ СОСТОЯНИЯ ===
    QObject::connect(
        server, &TcpServer::serverStarted,
        &app, [](quint16 port) {
            qDebug().noquote() << QString("[STATUS] Server started on port %1").arg(port);
        }
    );

    QObject::connect(
        server, &TcpServer::clientConnected,
        &app, []() {
            qDebug().noquote() << "[STATUS] Client connected!";
        }
    );

    QObject::connect(
        server, &TcpServer::transportConnected,
        &app, []() {
            qDebug().noquote() << "[STATUS] Transport connected!";
        }
    );

    QObject::connect(
        server, &TcpServer::clientDisconnected,
        &app, []() {
            qDebug().noquote() << "[STATUS] Client disconnected!";
        }
    );

    QObject::connect(
        server, &TcpServer::transportDisconnected,
        &app, []() {
            qDebug().noquote() << "[STATUS] Transport disconnected!";
        }
    );

    // === ПОДКЛЮЧЕНИЕ СИГНАЛОВ ДАННЫХ ===
    QObject::connect(
        server, &TcpServer::dataFromClient,
        &app, [server](const QByteArray& data) {
            qDebug().noquote() << "[DATA] From client: " + QString(data);
            // Отправляем команду на антенну
            server->sendToTransport(data);
        }
    );

    QObject::connect(
        server, &TcpServer::dataFromTransport,
        &app, [server](const QByteArray& data) {
            qDebug().noquote() << "[DATA] From transport: " + QString(data);
            // Отправляем ответ клиенту
            server->sendToClient(data);
        }
    );

    // === ПЕРИОДИЧЕСКИЙ ЗАПРОС СТАТУСА АНТЕННЫ ===
    // Создаем таймер для периодической отправки запроса статуса
    QTimer* statusTimer = new QTimer(&app);
    QObject::connect(
        statusTimer, &QTimer::timeout,
        &app, [server]() {
            // Проверяем, подключен ли транспорт
            if (server->isTransportConnected()) {
                // Отправляем запрос статуса на антенну
                server->sendToTransport("STATUS");
                qDebug().noquote() << "[STATUS] Status request sent to antenna";
            }
            else {
                qDebug().noquote() << "[STATUS] Cannot send status request: transport not connected";
            }
        }
    );

    // Запускаем таймер с интервалом 5 секунд
    statusTimer->start(5000);

    // === ЗАПУСК СЕРВЕРА ===
    server->start(8888);                           // Порт для клиента (GUI)
    server->connectToTransport("127.0.0.1", 2324); // Порт для транспорта

    qDebug().noquote() << "========================================";
    qDebug().noquote() << "SERVER IS RUNNING";
    qDebug().noquote() << "  Client port: 8888 (for GUI)";
    qDebug().noquote() << "  Transport port: 2324 (to antenna)";
    qDebug().noquote() << "  Status request: every 5 seconds";
    qDebug().noquote() << "========================================";

    return app.exec();
}