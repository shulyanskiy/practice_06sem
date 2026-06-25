#ifndef EMUAPP_H
#define EMUAPP_H

#include <QWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMessageBox>
#include <QTextEdit>
#include <QLayout>
#include <QLabel>
#include <QTime>

// class EmuApp: public QWidget
// {
//     Q_OBJECT
// private:
//     QTcpServer *p_tcpServer;

// public:
//     EmuApp(QWidget *p_wgt = 0, QString wTitle = "Эмулятор антенны");

//     bool quit_connect(QApplication *app);
// };


// =================================================================
// ======================= Класс сервера ===========================
// =================================================================
class MyServer : public QWidget{
Q_OBJECT
private:
    QTcpServer *p_tcpServer;
    QTextEdit  *p_txt;
    quint16     n_NextBlockSize;

//private:
    void sendToClient(QTcpSocket *p_Socket, const QString &str);

public:
    explicit MyServer(int nPort, QWidget *p_wgt = 0);

public slots:
    virtual void slotNewConnection();
            void slotReadClient   ();
};

#endif // EMUAPP_H
