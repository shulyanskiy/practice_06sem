#include "myclient.h"


MyClient::MyClient(const QString &strHost, int nPort, QWidget *p_wgt)
    : QWidget(p_wgt), n_NextBlockSize(0)
{
    // Работа с сетевым подключением
    p_TcpSocket = new QTcpSocket(this);
    p_TcpSocket->connectToHost(strHost, nPort);

    connect(p_TcpSocket, &QTcpSocket::connected,
            this,        &MyClient::slotConnected);
    connect(p_TcpSocket, &QTcpSocket::readyRead,
            this,        &MyClient::slotReadyRead);
    connect(p_TcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, QOverload<QAbstractSocket::SocketError>::of(&MyClient::slotError));

    // Работа с отправкой пользовательского сообщения
    p_txtInfo  = new QTextEdit(this);
    p_txtInput = new QLineEdit(this);

    QPushButton *p_btn_send = new QPushButton(this);
    // QIcon p_send_Icon = QIcon::fromTheme("send");
    // if(!p_send_Icon.isNull())
    //     p_btn_send->setIcon(p_send_Icon);
    // else
        p_btn_send->setText("Send");

    connect(p_btn_send, &QPushButton::clicked,
            this,  &MyClient::slotSendToServer);
    connect(p_txtInput, &QLineEdit::returnPressed,
            this,      &MyClient::slotSendToServer);

    // Кнопки отправки определенных команд
    QPushButton *p_btn_ConnectToServer = new QPushButton("Connect to Server", this);
    connect(p_btn_ConnectToServer, &QPushButton::clicked,
            this, [this](){
                // QString tmp = p_txtInput->text();
                // p_txtInput->setText("<CONNECT TO SERVER>");
                // slotSendToServer();
                // p_txtInput->setText(tmp);
                p_TcpSocket->connectToHost("localhost", 2323);
            });

    QPushButton *p_btn_cmd_GETPOS  = new QPushButton("Get position", this);
    connect(p_btn_cmd_GETPOS, &QPushButton::clicked,
            this,  [this](){
                QString tmp = p_txtInput->text();
                p_txtInput->setText("<GETPOS>");
                slotSendToServer();
                p_txtInput->setText(tmp);
            });

    QPushButton *p_btn_cmd_SETPOS  = new QPushButton("Set position", this);
    connect(p_btn_cmd_SETPOS, &QPushButton::clicked,
            this,  [this](){
                QString tmp = p_txtInput->text();
                p_txtInput->setText("<SETPOS...>");
                slotSendToServer();
                p_txtInput->setText(tmp);
            });

    QPushButton *p_btn_cmd_CONNECT = new QPushButton("Connect to Antenna", this);
    connect(p_btn_cmd_CONNECT, &QPushButton::clicked,
            this,  [this](){
                QString tmp = p_txtInput->text();
                p_txtInput->setText("<TRY CONNECT TO ANTENNA>");
                slotSendToServer();
                p_txtInput->setText(tmp);
            });


    // Layout setup
    QHBoxLayout *p_Sender_layout = new QHBoxLayout;
    p_Sender_layout->addWidget(p_txtInput);
    p_Sender_layout->addWidget(p_btn_send);

    QVBoxLayout *p_InfoInput_layout = new QVBoxLayout;
    p_InfoInput_layout->addWidget(new QLabel("<H1>Client</H1>"));
    p_InfoInput_layout->addWidget(p_txtInfo);
    p_InfoInput_layout->addLayout(p_Sender_layout);

    QVBoxLayout *p_CMD_layout = new QVBoxLayout;
    p_CMD_layout->addWidget(new QLabel("<H1>Commands</H1>"));
    p_CMD_layout->addWidget(p_btn_ConnectToServer);
    p_CMD_layout->addWidget(p_btn_cmd_CONNECT);
    p_CMD_layout->addWidget(p_btn_cmd_GETPOS);
    p_CMD_layout->addWidget(p_btn_cmd_SETPOS);

    QHBoxLayout *p_HLayout = new QHBoxLayout;
    p_HLayout->addLayout(p_InfoInput_layout);
    p_HLayout->addLayout(p_CMD_layout);


    setLayout(p_HLayout);

}

void MyClient::slotReadyRead()
{
    QDataStream in(p_TcpSocket);
    in.setVersion(QDataStream::Qt_5_15);
    for(;;){
        if(!n_NextBlockSize){
            if(p_TcpSocket->bytesAvailable() < static_cast<qint64>(sizeof(quint16)))
                break;

            in >> n_NextBlockSize;
        }

        if(p_TcpSocket->bytesAvailable() < static_cast<qint64>(n_NextBlockSize))
            break;

        QTime  time;
        QString str;
        in >> time >> str;

        p_txtInfo->append(time.toString()+" "+str);
        n_NextBlockSize = 0;
    }
}

void MyClient::slotError(QAbstractSocket::SocketError err)
{
    QString strError =
        "ERROR: " + (err == QAbstractSocket::HostNotFoundError ?
                     "The host was not found." :
                     err == QAbstractSocket::RemoteHostClosedError ?
                     "The remote host is closed" :
                     err == QAbstractSocket::ConnectionRefusedError ?
                     "The connection was refused" :
                     QString(p_TcpSocket->errorString())
                    );
    p_txtInfo->append(strError);
}

void MyClient::slotSendToServer()
{
    QByteArray arrBlock;
    QDataStream out(&arrBlock, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_15);
    out << quint16(0) << QTime::currentTime() << p_txtInput->text();

    out.device()->seek(0);
    out << quint16(arrBlock.size() - sizeof(quint16));

    p_TcpSocket->write(arrBlock);
    p_txtInput->clear();
}



void MyClient::slotConnected()
{
    p_txtInfo->append("Received the connected() signal");
}




















