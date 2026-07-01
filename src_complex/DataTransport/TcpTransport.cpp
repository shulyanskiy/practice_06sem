// tcptransport.cpp
#include "TcpTransport.h"
#include <QDebug>
#include <QHostAddress>
#include <QDateTime>

TcpTransport::TcpTransport(QObject* parent)
    : ITransport(parent)
    , m_serverSocket(nullptr)
    , m_antennaSocket(nullptr)
    , m_serverListener(nullptr)
    , m_antennaListener(nullptr)
    , m_serverPort(0)
    , m_antennaPort(0)
    , m_isProcessing(false)
{
    m_processTimer = new QTimer(this);
    m_processTimer->setInterval(PROCESS_INTERVAL_MS);
    connect(m_processTimer, &QTimer::timeout, this, &TcpTransport::processQueue);
}

TcpTransport::~TcpTransport()
{
    stopProcessing();

    cleanupSocket(m_serverSocket);
    cleanupSocket(m_antennaSocket);

    stopServerListener();
    stopAntennaListener();
}

// ===== Управление прослушиванием =====

bool TcpTransport::startServerListener(quint16 port)
{
    if (m_serverListener) {
        emit warningOccurred(QString("Server listener already running on port %1").arg(m_serverPort));
        return false;
    }

    m_serverPort = port;
    m_serverListener = new QTcpServer(this);
    connect(m_serverListener, &QTcpServer::newConnection,
        this, &TcpTransport::onServerConnected);

    if (!m_serverListener->listen(QHostAddress::Any, port)) {
        emit errorOccurred(QString("Failed to listen for server on port %1").arg(port));
        delete m_serverListener;
        m_serverListener = nullptr;
        return false;
    }

    emit msgOccurred(QString("Waiting for SERVER on port %1").arg(port));
    return true;
}

bool TcpTransport::startAntennaListener(quint16 port)
{
    if (m_antennaListener) {
        emit warningOccurred(QString("Antenna listener already running on port %1").arg(m_antennaPort));
        return false;
    }

    m_antennaPort = port;
    m_antennaListener = new QTcpServer(this);
    connect(m_antennaListener, &QTcpServer::newConnection,
        this, &TcpTransport::onAntennaConnected);

    if (!m_antennaListener->listen(QHostAddress::Any, port)) {
        emit errorOccurred(QString("Failed to listen for antenna on port %1").arg(port));
        delete m_antennaListener;
        m_antennaListener = nullptr;
        return false;
    }

    emit msgOccurred(QString("Waiting for antenna on port %1").arg(port));
    return true;
}

void TcpTransport::stopServerListener()
{
    if (m_serverListener) {
        m_serverListener->close();
        delete m_serverListener;
        m_serverListener = nullptr;
        
        emit msgOccurred(QString("Server listener stopped"));
    }
}

void TcpTransport::stopAntennaListener()
{
    if (m_antennaListener) {
        m_antennaListener->close();
        delete m_antennaListener;
        m_antennaListener = nullptr;
        
        emit msgOccurred(QString("Antenna listener stopped"));
    }
}

// ===== Подключения (теперь только обработка входящих) =====

void TcpTransport::onServerConnected()
{
    if (!m_serverListener) {
        return;
    }

    // Проверяем, нет ли уже подключенного сервера
    if (m_serverSocket) {
        
        emit warningOccurred(QString("Server already connected, rejecting new connection"));
        QTcpSocket* socket = m_serverListener->nextPendingConnection();
        if (socket) {
            socket->disconnectFromHost();
            socket->deleteLater();
        }
        return;
    }

    QTcpSocket* socket = m_serverListener->nextPendingConnection();
    if (!socket) {
        return;
    }

    m_serverSocket = socket;
    m_serverBuffer.reset();

    connect(socket, &QTcpSocket::readyRead, this, &TcpTransport::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &TcpTransport::onDisconnected);
    connect(socket, &QTcpSocket::errorOccurred, this, &TcpTransport::onError);

    emit serverConnected();
}

void TcpTransport::onAntennaConnected()
{
    if (!m_antennaListener) {
        return;
    }

    // Проверяем, нет ли уже подключенной антенны
    if (m_antennaSocket) {
        // qWarning() << "Antenna already connected, rejecting new connection";
        emit warningOccurred(QString("Antenna already connected, rejecting new connection"));
        QTcpSocket* socket = m_antennaListener->nextPendingConnection();
        if (socket) {
            socket->disconnectFromHost();
            socket->deleteLater();
        }
        return;
    }

    QTcpSocket* socket = m_antennaListener->nextPendingConnection();
    if (!socket) {
        return;
    }

    m_antennaSocket = socket;
    m_antennaBuffer.reset();

    connect(socket, &QTcpSocket::readyRead, this, &TcpTransport::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &TcpTransport::onDisconnected);
    connect(socket, &QTcpSocket::errorOccurred, this, &TcpTransport::onError);

    emit antennaConnected();
}

// ===== Отключение =====

void TcpTransport::disconnectServer()
{
    if (m_serverSocket) {
        m_serverSocket->disconnectFromHost();
        cleanupSocket(m_serverSocket);

        emit serverDisconnected();
    }
}

void TcpTransport::disconnectAntenna()
{
    if (m_antennaSocket) {
        m_antennaSocket->disconnectFromHost();
        cleanupSocket(m_antennaSocket);

        emit antennaDisconnected();
    }
}

// ===== Проверка состояния =====

bool TcpTransport::isServerConnected() const
{
    return m_serverSocket && m_serverSocket->state() == QAbstractSocket::ConnectedState;
}

bool TcpTransport::isAntennaConnected() const
{
    return m_antennaSocket && m_antennaSocket->state() == QAbstractSocket::ConnectedState;
}

// ===== Отправка данных =====

void TcpTransport::sendToServer(const QByteArray& data)
{
    if (data.isEmpty()) {
        return;
    }

    if (!isServerConnected()) {
        emit errorOccurred(QString("Server not connected"));
        return;
    }

    enqueueMessage(data, true);
}

void TcpTransport::sendToAntenna(const QByteArray& data)
{
    if (data.isEmpty()) {
        return;
    }

    if (!isAntennaConnected()) {
        emit errorOccurred(QString("Antenna not connected"));
        return;
    }

    enqueueMessage(data, false);
}

void TcpTransport::sendToAll(const QByteArray& data)
{
    sendToServer(data);
    sendToAntenna(data);
}

bool TcpTransport::sendDirect(QTcpSocket* socket, const QByteArray& data)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        return false;
    }

    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setVersion(DATA_STREAM_VERSION);
    out << quint16(data.size());
    out.writeRawData(data.data(), data.size());

    qint64 bytesWritten = socket->write(packet);
    if (bytesWritten == -1) {
        return false;
    }

    socket->flush();
    return true;
}

// ===== Очередь сообщений =====

void TcpTransport::enqueueMessage(const QByteArray& data, bool toServer)
{
    QMutexLocker locker(&m_queueMutex);

    QueuedMessage msg;
    msg.data = data;
    msg.retryCount = 0;
    msg.timestamp = QDateTime::currentMSecsSinceEpoch();
    msg.toServer = toServer;

    m_messageQueue.enqueue(msg);
}

void TcpTransport::processNextMessage()
{
    if (m_messageQueue.isEmpty()) {
        return;
    }

    QueuedMessage msg;
    {
        QMutexLocker locker(&m_queueMutex);
        msg = m_messageQueue.dequeue();
    }

    QTcpSocket* targetSocket = msg.toServer ? m_serverSocket : m_antennaSocket;
    bool sent = sendDirect(targetSocket, msg.data);

    if (!sent && msg.retryCount < MAX_RETRY_COUNT) {
        msg.retryCount++;
        QMutexLocker locker(&m_queueMutex);
        m_messageQueue.enqueue(msg);

        emit msgOccurred(
            QString("Retry sending to %1 with attempt %2")
            .arg((msg.toServer ? "server" : "antenna"))
            .arg(msg.retryCount)
        );
    }
    else if (!sent) {
        emit errorOccurred(
            QString("Failed to send to %1 after attempts: %2")
            .arg(msg.toServer ? "server" : "antenna")
            .arg(MAX_RETRY_COUNT)
        );
    }
}

void TcpTransport::processQueue()
{
    if (m_messageQueue.isEmpty()) {
        return;
    }

    const int MAX_PER_CYCLE = 20;
    int processed = 0;

    while (!m_messageQueue.isEmpty() && processed < MAX_PER_CYCLE) {
        processNextMessage();
        processed++;
    }
}

// ===== Обработка входящих данных =====

void TcpTransport::onReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        return;
    }

    processSocketData(socket);
}

void TcpTransport::processSocketData(QTcpSocket* socket)
{
    bool isServer = (socket == m_serverSocket);
    SocketBuffer* buffer = isServer ? &m_serverBuffer : &m_antennaBuffer;

    QDataStream in(socket);
    in.setVersion(DATA_STREAM_VERSION);

    while (true) {
        // Читаем размер
        if (buffer->nextBlockSize == 0) {
            if (socket->bytesAvailable() < static_cast<qint64>(sizeof(quint16))) {
                break;
            }
            in >> buffer->nextBlockSize;

            if (buffer->nextBlockSize <= 0 || buffer->nextBlockSize > MAX_MESSAGE_SIZE) {
                emit warningOccurred(
                    QString("Invalid block size: ") + QString(buffer->nextBlockSize)
                    + QString("from ") + QString((isServer ? "server" : "antenna"))
                );

                buffer->reset();
                socket->readAll();
                break;
            }
        }

        // Читаем данные
        if (socket->bytesAvailable() < static_cast<qint64>(buffer->nextBlockSize)) {
            break;
        }

        buffer->rawData.resize(buffer->nextBlockSize);
        qint64 bytesRead = in.readRawData(buffer->rawData.data(), buffer->nextBlockSize);

        if (bytesRead != static_cast<qint64>(buffer->nextBlockSize)) {
            emit warningOccurred(
                QString("Failed to read full message. Expected: %1, Got: %2")
                .arg(buffer->nextBlockSize)
                .arg(bytesRead)
            );
            buffer->reset();
            break;
        }

        // Обрабатываем данные
        handleReceivedData(socket, buffer->rawData);
        buffer->reset();
    }
}

void TcpTransport::handleReceivedData(QTcpSocket* socket, const QByteArray& data)
{
    if (data.isEmpty()) {
        return;
    }

    if (socket == m_serverSocket) {
        emit dataFromServer(data);
    }
    else if (socket == m_antennaSocket) {
        emit dataFromAntenna(data);
    }
}

// ===== Очистка =====

void TcpTransport::cleanupSocket(QTcpSocket*& socket)
{
    if (!socket) {
        return;
    }

    socket->disconnectFromHost();
    socket->deleteLater();
    socket = nullptr;
}

// ===== Слоты отключения и ошибок =====

void TcpTransport::onDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        return;
    }

    if (socket == m_serverSocket) {
        m_serverBuffer.reset();
        cleanupSocket(m_serverSocket);

        emit serverDisconnected();
    }
    else if (socket == m_antennaSocket) {
        m_antennaBuffer.reset();
        cleanupSocket(m_antennaSocket);

        emit antennaDisconnected();
    }
}

void TcpTransport::onError(QAbstractSocket::SocketError error)
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        return;
    }

    QString errorMsg = socket->errorString();

    if (socket == m_serverSocket) {
        emit errorOccurred(QString("Server error: %1").arg(errorMsg));
    }
    else if (socket == m_antennaSocket) {
        emit errorOccurred(QString("Antenna error: %1").arg(errorMsg));
    }

    emit warningOccurred(QString("Socket error: %1 (code: %2").arg(errorMsg).arg(error));
}

// ===== Управление очередью =====

void TcpTransport::startProcessing()
{
    if (!m_isProcessing) {
        m_isProcessing = true;
        m_processTimer->start();

        emit msgOccurred(QString("Queue processing started"));
    }
}

void TcpTransport::stopProcessing()
{
    if (m_isProcessing) {
        m_isProcessing = false;

        emit msgOccurred(QString("Queue processing stopped"));
    }
}

int TcpTransport::queueSize() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_messageQueue.size();
}