#ifndef MYCLIENT_H
#define MYCLIENT_H

#include <QDataStream>
#include <QWidget>
#include <QTcpSocket>
#include <QTextEdit>
#include <QTime>
#include <QLineEdit>
#include <QPushButton>
#include <QBoxLayout>
#include <QLabel>
// #include <QIcon>

class MyClient : public QWidget
{
    Q_OBJECT
private:
    QTcpSocket *p_TcpSocket;
    QTextEdit  *p_txtInfo;
    QLineEdit  *p_txtInput;
    quint16     n_NextBlockSize;

public:
    explicit MyClient(const QString &strHost, int nPort, QWidget *p_wgt = nullptr);

private slots:
    void slotReadyRead();
    void slotError(QAbstractSocket::SocketError);
    void slotSendToServer();
    void slotConnected();
};

#endif // MYCLIENT_H
