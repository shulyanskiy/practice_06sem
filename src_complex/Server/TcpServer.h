#ifndef TCPSERVER_H
#define TCPSERVER_H

#include "iserver.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QAtomicInt>
#include <QQueue>
#include <QMutex>
#include <QDataStream>

struct QueuedMessage
{
    QByteArray data;
    int retryCount;
    qint64 timestamp;
    bool toClient;  // true - клиенту, false - транспорту
};

class TcpServer : public IServer
{
    Q_OBJECT
public:
    explicit TcpServer(QObject* parent = nullptr);
    ~TcpServer();

    QByteArray execCMD(const QByteArray& request);

    // IServer interface
    bool start(quint16 port) override;
    void stop() override;
    bool isRunning() const override;
    bool hasClient() const override;

    bool connectToTransport(const QString& address, quint16 port) override;
    void disconnectFromTransport() override;
    bool isTransportConnected() const override;

    // Отправка данных
    void sendToClient(const QByteArray& data) override;
    void sendToTransport(const QByteArray& data) override;
    void sendToAll(const QByteArray& data) override;

private slots:
    // Слоты для клиента
    void onClientConnected();
    void onClientReadyRead();
    void onClientDisconnected();
    void onClientError(QAbstractSocket::SocketError error);

    // Слоты для транспорта
    void onTransportConnected();
    void onTransportReadyRead();
    void onTransportDisconnected();
    void onTransportError(QAbstractSocket::SocketError error);

    // Очередь сообщений
    void processQueue();

private:
    // Команды сервера
    QByteArray cmdHelp();
    QByteArray cmdEcho(const QList<QByteArray>& params);
    QByteArray cmdStatus();
    QByteArray cmdDisconnect();
    QByteArray cmdClearStats();

    // Вспомогательные методы
    bool isCommandValid(const ParsedCMD& cmd, int expectedParams = 0) const;
    QByteArray createErrorResponse(const QString& error) const;
    QByteArray createSuccessResponse(const QString& message) const;
    void sendData(QTcpSocket* socket, const QByteArray& data);
    void processSocketData(QTcpSocket* socket); 
    void cleanupClient();
    void cleanupTransport();

    // === Методы для очереди ===
    void enqueueMessage(const QByteArray& data, bool toClient);
    bool sendDirect(QTcpSocket* socket, const QByteArray& data);
    void processNextMessage();

    // Методы для эмита сигналов
    void emitInfo(const QString& message);
    void emitWarning(const QString& message);
    void emitError(const QString& message);

private:
    // === Структура буфера (как в транспорте) ===
    struct SocketBuffer {
        quint16 nextBlockSize = 0;
        QByteArray rawData;

        void reset() {
            nextBlockSize = 0;
            rawData.clear();
        }
    };

    // === Сервер для клиента ===
    QTcpServer* m_server;
    quint16 m_serverPort;
    QAtomicInt m_isRunning;

    // === Клиент (только 1) ===
    QTcpSocket* m_clientSocket;
    QAtomicInt m_hasClient;
    SocketBuffer m_clientBuffer;  

    // === Транспорт ===
    QTcpSocket* m_transportSocket;
    bool m_isTransportConnected;
    QString m_transportAddress;
    quint16 m_transportPort;
    SocketBuffer m_transportBuffer;  

    // === Очередь сообщений ===
    QQueue<QueuedMessage> m_messageQueue;
    mutable QMutex m_queueMutex;

    // === Таймер очереди ===
    QTimer* m_processTimer;
    bool m_isProcessing;

    // === Статистика ===
    qint64 m_totalCommands;
    qint64 m_successCommands;
    qint64 m_failedCommands;

    // Константы
    static constexpr int MAX_MESSAGE_SIZE = 10 * 1024 * 1024;
    static constexpr int DATA_STREAM_VERSION = QDataStream::Qt_5_15;
    static constexpr int MAX_RETRY_COUNT = 3;
    static constexpr int PROCESS_INTERVAL_MS = 50;
};

#endif // TCPSERVER_H