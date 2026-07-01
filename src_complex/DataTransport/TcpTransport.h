// tcptransport.h
#ifndef TCPTRANSPORT_H
#define TCPTRANSPORT_H

#include "Transport.h"
#include <QTcpSocket>
#include <QTcpServer>
#include <QTimer>
#include <QQueue>
#include <QMutex>
#include <QPointer>

struct QueuedMessage
{
    QByteArray data;
    int retryCount;
    qint64 timestamp;
    bool toServer;  // true - отправить серверу, false - антенне
};

class TcpTransport : public ITransport
{
public:
    explicit TcpTransport(QObject* parent = nullptr);
    ~TcpTransport();

    // ===== Управление подключениями =====

    // Запуск сервера для ожидания подключения СЕРВЕРА
    bool startServerListener(quint16 port);

    // Запуск сервера для ожидания подключения АНТЕННЫ
    bool startAntennaListener(quint16 port);

    // Остановка прослушивания
    void stopServerListener();
    void stopAntennaListener();

    // Отключение
    void disconnectServer();
    void disconnectAntenna();

    // Проверка состояния
    bool isServerConnected() const;
    bool isAntennaConnected() const;

    // ===== Отправка данных =====
    void sendToServer(const QByteArray& data);
    void sendToAntenna(const QByteArray& data);
    void sendToAll(const QByteArray& data);

    // ===== Управление очередью =====
    void startProcessing();
    void stopProcessing();
    int queueSize() const;

//signals:
//    // Данные от получателей
//    void dataFromServer(const QByteArray& data);
//    void dataFromAntenna(const QByteArray& data);
//
//    // События подключения
//    void serverConnected();
//    void serverDisconnected();
//    void antennaConnected();
//    void antennaDisconnected();

    // Ошибки
    // void errorOccurred(const QString& error);

private slots:
    void processQueue();
    void onServerConnected();
    void onAntennaConnected();
    void onReadyRead();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);

private:
    void enqueueMessage(const QByteArray& data, bool toServer);
    bool sendDirect(QTcpSocket* socket, const QByteArray& data);
    void processNextMessage();
    void handleReceivedData(QTcpSocket* socket, const QByteArray& data);
    void processSocketData(QTcpSocket* socket);
    void cleanupSocket(QTcpSocket*& socket);

    // Структура для буфера чтения
    struct SocketBuffer {
        quint16 nextBlockSize = 0;
        QByteArray rawData;

        void reset() {
            nextBlockSize = 0;
            rawData.clear();
        }
    };

    // Два сокета - по одному на каждое подключение
    QTcpSocket* m_serverSocket;
    QTcpSocket* m_antennaSocket;

    // Два сервера - для ожидания подключений
    QTcpServer* m_serverListener;   // Для сервера
    QTcpServer* m_antennaListener;  // Для антенны

    quint16 m_serverPort;
    quint16 m_antennaPort;

    // Буферы для чтения
    SocketBuffer m_serverBuffer;
    SocketBuffer m_antennaBuffer;

    // Очередь сообщений
    QQueue<QueuedMessage> m_messageQueue;
    mutable QMutex m_queueMutex;

    // Таймер
    QTimer* m_processTimer;
    bool m_isProcessing;

    // Константы
    static const int MAX_RETRY_COUNT = 3;
    static const int PROCESS_INTERVAL_MS = 50;
    static const int MAX_MESSAGE_SIZE = 10 * 1024 * 1024;  // 10 MB
    static const int DATA_STREAM_VERSION = QDataStream::Qt_5_15;
};

#endif // TCPTRANSPORT_H