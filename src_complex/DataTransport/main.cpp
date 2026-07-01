#include <QtCore/QCoreApplication>
#include <QDebug>
#include "TcpTransport.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    TcpTransport* data_transport = new TcpTransport(&app);


    QObject::connect(
        data_transport, &TcpTransport::msgOccurred,
        &app, [](const QString msg) {
            qDebug() << QString("<MESSAGE>: %1").arg(msg);
        }
    );
    QObject::connect(
        data_transport, &TcpTransport::warningOccurred,
        &app, [](const QString warning) {
            qDebug() << QString("<WARNING>: %1").arg(warning);
        }
    );

    QObject::connect(
        data_transport, &TcpTransport::errorOccurred,
        &app, [](const QString error) {
            qDebug() << QString("<ERROR>: %1").arg(error);
        }
    );


    QObject::connect(
        data_transport, &TcpTransport::serverConnected,
        &app, []() {
            qDebug() << "SERVER CONNECTED";
        }
    );
    QObject::connect(
        data_transport, &TcpTransport::serverDisconnected,
        &app, []() {
            qDebug() << "SERVER DISONNECTED";
        }
    );

    QObject::connect(
        data_transport, &TcpTransport::antennaConnected,
        &app, []() {
            qDebug() << "ANTENNA CONNECTED";
        }
    );
    QObject::connect(
        data_transport, &TcpTransport::antennaDisconnected,
        &app, []() {
            qDebug() << "ANTENNA DISONNECTED";
        }
    );

    // логика отправки данных
    QObject::connect(
        data_transport, &TcpTransport::dataFromAntenna,
        &app, [data_transport](const QByteArray& data) {
            qDebug() << "RECEIVED DATA FROM ANTENNA";
            data_transport->sendToServer(data);
        }
    );

    QObject::connect(
        data_transport, &TcpTransport::dataFromServer,
        &app, [data_transport](const QByteArray& data) {
            qDebug() << "RECEIVED DATA FROM SERVER";
            data_transport->sendToAntenna(data);
        }
    );

    // запуск очереди
    data_transport->startProcessing();

    data_transport->startAntennaListener(2323);
    data_transport->startServerListener(2324);



    return app.exec();
}
