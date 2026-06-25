#include "EmuApp.h"

// EmuApp::EmuApp(QWidget *p_wgt, QString wTitle): QWidget(p_wgt) {
//     quit_button = new QPushButton(p_wgt);
//     quit_button->setText("UNDEFIENED");

//     // QBoxLayout *boxLayout = new QBoxLayout(QBoxLayout::LeftToRight);
//     // boxLayout->addWidget(quit_button);

//     // setLayout(boxLayout);
// }

// bool EmuApp::quit_connect(QApplication *app){
//     QObject::connect(quit_button, quit_button->clicked(),
//                      app, QApplication::quit());

//     quit_button->setText("QUIT");

//     return true;
// }

// ===================================================================
// ======================= Класс сервера =============================
// ===================================================================


MyServer::MyServer(int nPort, QWidget *p_wgt)
    : QWidget(p_wgt), n_NextBlockSize(0)
{
    p_tcpServer = new QTcpServer(this);

    if(!p_tcpServer->listen(QHostAddress::Any, nPort)){
        QMessageBox::critical(0,
                              "SERVER ERROR",
                              "Unable to start the server:"
                                  + p_tcpServer->errorString());
        // qDebug() << "SERVER ERROR: Unable to start the server:\n"
        //             + p_tcpServer->errorString();
        p_tcpServer->close();
        return;
    }

    connect(p_tcpServer, &QTcpServer::newConnection,
            this,        &MyServer::slotNewConnection);

    p_txt = new QTextEdit;
    p_txt->setReadOnly(true);

    // Layout setup
    QVBoxLayout *p_VBoxLayout = new QVBoxLayout;
    p_VBoxLayout->addWidget(new QLabel("<H1>Server</H1>"));
    p_VBoxLayout->addWidget(p_txt);
    setLayout(p_VBoxLayout);
}

void MyServer::slotReadClient()
{
    QTcpSocket *p_ClientSocket = qobject_cast<QTcpSocket*>(sender());
    QDataStream in(p_ClientSocket);
    in.setVersion(QDataStream::Qt_5_15); // изменить на константу
    for(;;){
        if(!n_NextBlockSize){
            if(p_ClientSocket->bytesAvailable() < static_cast<qint64>(sizeof(quint16)))
                break;

            in >> n_NextBlockSize;
        }

        // можно добавить ограничение на размер пакета и соотв. проверку

        if(p_ClientSocket->bytesAvailable() < static_cast<qint64>(n_NextBlockSize))
            break;

        // жесткая логика отправки сообщения
        QTime   time;
        QString str ;
        in >> time >> str;

        QString strMessage =
            time.toString() + " " + "Client has sent - " + str;

        p_txt->append(strMessage);

        n_NextBlockSize = 0;
        sendToClient(p_ClientSocket, "Server Response: Received \"" + str + "\"");
    }
}

void MyServer::slotNewConnection()
{
    QTcpSocket *p_ClientSocket = p_tcpServer->nextPendingConnection();

    connect(p_ClientSocket, &QTcpSocket::disconnected,
            this,           &QObject::deleteLater);

    connect(p_ClientSocket, &QIODevice::readyRead,
            this,           &MyServer::slotReadClient);
}

void MyServer::sendToClient(QTcpSocket *p_Socket, const QString &str)
{
    QByteArray arrBlock;

    QDataStream out(&arrBlock, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_15); // изменить на константу
    out << quint16(0) << QTime::currentTime() << str;

    out.device()->seek(0);
    out << quint16(arrBlock.size() - sizeof(quint16));

    p_Socket->write(arrBlock);
}


