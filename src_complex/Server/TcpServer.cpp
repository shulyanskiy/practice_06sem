// tcpserver.cpp (полный файл)
#include "tcpserver.h"
#include <QHostAddress>
#include <QTimer>
#include <QDateTime>

TcpServer::TcpServer(QObject* parent)
    : IServer(parent)
    , m_server(nullptr)
    , m_serverPort(0)
    , m_isRunning(false)
    , m_clientSocket(nullptr)
    , m_hasClient(false)
    , m_transportSocket(nullptr)
    , m_isTransportConnected(false)
    , m_transportPort(0)
    , m_isProcessing(false)
    , m_totalCommands(0)
    , m_successCommands(0)
    , m_failedCommands(0)
{
    // 1. Создаем сервер для клиентов
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection,
        this, &TcpServer::onClientConnected);

    // 2. Создаем таймер для обработки очереди
    m_processTimer = new QTimer(this);
    m_processTimer->setInterval(PROCESS_INTERVAL_MS);
    connect(m_processTimer, &QTimer::timeout,
        this, &TcpServer::processQueue);

    // 3. Инициализируем буферы (они уже инициализированы в структуре)
    // m_clientBuffer и m_transportBuffer уже имеют default значения
    // nextBlockSize = 0, rawData = empty

    emitInfo("Server instance created");
}

TcpServer::~TcpServer()
{
    stop();
    disconnectFromTransport();
    m_clientBuffer.reset();
    m_transportBuffer.reset();
    emitInfo("Server destroyed");
}

// ===== Основные методы =====

// ===== Запуск/остановка =====

bool TcpServer::start(quint16 port)
{
    if (m_isRunning) {
        emitWarning("Server already running on port " + QString::number(m_serverPort));
        return false;
    }

    if (!m_server->listen(QHostAddress::Any, port)) {
        emitError("Failed to start server on port " + QString::number(port) + ": " +
            m_server->errorString());
        return false;
    }

    m_serverPort = port;
    m_isRunning = true;

    // Запускаем обработку очереди
    if (!m_isProcessing) {
        m_isProcessing = true;
        m_processTimer->start();
        emitInfo("Queue processing started");
    }

    emitInfo("Server started on port " + QString::number(port));
    emit serverStarted(port);

    return true;
}

void TcpServer::stop()
{
    if (!m_isRunning) {
        emitWarning("Server already stopped");
        return;
    }

    m_isRunning = false;
    m_server->close();

    // Останавливаем обработку очереди
    if (m_isProcessing) {
        m_isProcessing = false;
        m_processTimer->stop();
        emitInfo("Queue processing stopped");
    }

    cleanupClient();
    cleanupTransport();

    emitInfo("Server stopped");
    emit serverStopped();
}


bool TcpServer::isRunning() const
{
    return m_isRunning && m_server->isListening();
}

bool TcpServer::hasClient() const
{
    return m_hasClient && m_clientSocket &&
        m_clientSocket->state() == QAbstractSocket::ConnectedState;
}

// ===== Транспорт =====

bool TcpServer::connectToTransport(const QString& address, quint16 port)
{
    if (m_transportSocket) {
        emitWarning("Already connected to transport");
        return false;
    }

    m_transportAddress = address;
    m_transportPort = port;

    m_transportSocket = new QTcpSocket(this);

    connect(m_transportSocket, &QTcpSocket::connected,
        this, &TcpServer::onTransportConnected);
    connect(m_transportSocket, &QTcpSocket::readyRead,
        this, &TcpServer::onTransportReadyRead);
    connect(m_transportSocket, &QTcpSocket::disconnected,
        this, &TcpServer::onTransportDisconnected);
    connect(m_transportSocket, &QTcpSocket::errorOccurred,
        this, &TcpServer::onTransportError);

    m_transportSocket->connectToHost(address, port);

    emitInfo("Connecting to transport at " + address + ":" + QString::number(port));
    return true;
}

void TcpServer::disconnectFromTransport()
{
    cleanupTransport();
    emitInfo("Disconnected from transport");
}

bool TcpServer::isTransportConnected() const
{
    return m_isTransportConnected && m_transportSocket &&
        m_transportSocket->state() == QAbstractSocket::ConnectedState;
}

// ===== Отправка данных =====

void TcpServer::sendToClient(const QByteArray& data)
{
    if (data.isEmpty()) {
        emitWarning("Attempt to send empty data to client");
        return;
    }

    if (!hasClient()) {
        emitError("Cannot send to client: not connected");
        return;
    }

    enqueueMessage(data, true);  // true = клиенту
    emitInfo("Message queued for client: " + QString::number(data.size()) + " bytes");
}

void TcpServer::sendToTransport(const QByteArray& data)
{
    if (data.isEmpty()) {
        emitWarning("Attempt to send empty data to transport");
        return;
    }

    if (!isTransportConnected()) {
        emitError("Cannot send to transport: not connected");
        return;
    }

    enqueueMessage(data, false);  // false = транспорту
    emitInfo("Message queued for transport: " + QString::number(data.size()) + " bytes");
}

void TcpServer::sendToAll(const QByteArray& data)
{
    if (data.isEmpty()) {
        emitWarning("Attempt to send empty data to all");
        return;
    }

    // Отправляем клиенту и транспорту
    sendToClient(data);
    sendToTransport(data);
    emitInfo("Message broadcast to all: " + QString::number(data.size()) + " bytes");
}

// ===== Очередь сообщений =====

void TcpServer::enqueueMessage(const QByteArray& data, bool toClient)
{
    QMutexLocker locker(&m_queueMutex);

    QueuedMessage msg;
    msg.data = data;
    msg.retryCount = 0;
    msg.timestamp = QDateTime::currentMSecsSinceEpoch();
    msg.toClient = toClient;

    m_messageQueue.enqueue(msg);
}

bool TcpServer::sendDirect(QTcpSocket* socket, const QByteArray& data)
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
        emitError("Failed to send data: " + socket->errorString());
        return false;
    }

    socket->flush();
    emitInfo("Sent " + QString::number(data.size()) + " bytes");
    return true;
}

void TcpServer::processNextMessage()
{
    if (m_messageQueue.isEmpty()) {
        return;
    }

    QueuedMessage msg;
    {
        QMutexLocker locker(&m_queueMutex);
        msg = m_messageQueue.dequeue();
    }

    QTcpSocket* targetSocket = msg.toClient ? m_clientSocket : m_transportSocket;
    bool sent = sendDirect(targetSocket, msg.data);

    if (!sent && msg.retryCount < MAX_RETRY_COUNT) {
        msg.retryCount++;
        QMutexLocker locker(&m_queueMutex);
        m_messageQueue.enqueue(msg);
        emitWarning("Retry " + QString::number(msg.retryCount) +
            " for " + (msg.toClient ? "client" : "transport"));
    }
    else if (!sent) {
        emitError("Failed to send to " +
            QString(msg.toClient ? "client" : "transport") +
            " after " + QString::number(MAX_RETRY_COUNT) + " attempts");
    }
}

void TcpServer::processQueue()
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



void TcpServer::sendData(QTcpSocket* socket, const QByteArray& data)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        emitError("Cannot send data: socket not connected");
        return;
    }

    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setVersion(DATA_STREAM_VERSION);
    out << quint16(data.size());
    out.writeRawData(data.data(), data.size());

    qint64 written = socket->write(packet);
    if (written == -1) {
        emitError("Failed to send data: " + socket->errorString());
    }
    else {
        socket->flush();
        emitInfo("Sent " + QString::number(data.size()) + " bytes");
    }
}

// ===== Команды =====

QByteArray TcpServer::execCMD(const QByteArray& request)
{
    try {
        // 1. Парсим команду
        ParsedCMD parsed = parsingToCMD(request);

        // 2. Проверяем валидность
        if (!parsed.isValid()) {
            emitWarning("Invalid command received: " + QString(request));
            return createErrorResponse("Empty or invalid command");
        }

        QString cmd = parsed.name;
        QList<QByteArray> params = parsed.params;

        emitInfo("Executing command: " + cmd + " with " +
            QString::number(params.size()) + " parameters");

        QByteArray response;

        // 3. Обрабатываем команды сервера
        if (cmd == "HELP" || cmd == "H" || cmd == "?") {
            response = cmdHelp();
            emit commandExecuted(cmd, true);
        }
        else if (cmd == "ECHO") {
            response = cmdEcho(params);
            emit commandExecuted(cmd, !response.contains("ERROR"));
        }
        else if (cmd == "STATUS" || cmd == "ST") {
            response = cmdStatus();
            emit commandExecuted(cmd, true);
        }
        else if (cmd == "DISCONNECT" || cmd == "DC") {
            response = cmdDisconnect();
            emit commandExecuted(cmd, !response.contains("ERROR"));
        }
        else {
            // 4. Команда не распознана сервером - отправляем на антенну через транспорт
            if (isTransportConnected()) {
                emitInfo("Forwarding command to antenna: " + cmd);

                // Отправляем команду на антенну через транспорт
                sendToTransport(request);

                response = createSuccessResponse("Command forwarded to antenna: " + cmd);
                emit commandExecuted(cmd, true);
            }
            else {
                emitError("Cannot forward command: transport not connected");
                response = createErrorResponse("Transport not connected");
                emit commandExecuted(cmd, false);
            }
        }

        return response;

    }
    catch (const std::exception& e) {
        QString error = "Exception in execCMD: " + QString(e.what());
        emitError(error);
        return createErrorResponse(error);
    }
}


// ===== Реализация команд =====

QByteArray TcpServer::cmdHelp()
{
    QString help =
        "=== SERVER COMMANDS ===\n"
        "HELP (H, ?)          - Show this help\n"
        "ECHO <message>       - Echo back message\n"
        "STATUS (ST)          - Show server status\n"
        "DISCONNECT (DC)      - Disconnect client\n"
        "\n"
        "=== ANTENNA COMMANDS ===\n"
        "SET_AZIMUTH <angle>  - Set azimuth (0-360)\n"
        "SET_ELEVATION <angle>- Set elevation (-90-90)\n"
        "SET_SPEED <speed>    - Set movement speed\n"
        "MOVE_AZIMUTH <delta> - Move azimuth by delta\n"
        "MOVE_ELEVATION <delta>- Move elevation by delta\n"
        "CALIBRATE            - Calibrate antenna\n"
        "STATUS               - Get antenna status\n"
        "STOP                 - Stop movement\n"
        "======================";

    return help.toUtf8();
}

QByteArray TcpServer::cmdEcho(const QList<QByteArray>& params)
{
    if (!isCommandValid(ParsedCMD{ "ECHO", params }, 1)) {
        emitWarning("ECHO command missing parameter");
        return createErrorResponse("ECHO requires 1 parameter: message");
    }

    emitInfo("ECHO command executed");
    return QByteArray("ECHO: ") + params[0] + "\n";
}

QByteArray TcpServer::cmdStatus()
{
    QString status = QString(
        "=== SERVER STATUS ===\n"
        "Running: %1\n"
        "Client Port: %2\n"
        "Client: %3\n"
        "Transport: %4 (%5:%6)\n"
        "====================="
    )
        .arg(m_isRunning ? "YES" : "NO")
        .arg(m_serverPort)
        .arg(hasClient() ? "CONNECTED" : "DISCONNECTED")
        .arg(isTransportConnected() ? "CONNECTED" : "DISCONNECTED")
        .arg(m_transportAddress.isEmpty() ? "N/A" : m_transportAddress)
        .arg(m_transportPort == 0 ? "N/A" : QString::number(m_transportPort));

    return status.toUtf8();
}

QByteArray TcpServer::cmdDisconnect()
{
    if (!hasClient()) {
        emitWarning("DISCONNECT command: no client connected");
        return createErrorResponse("No client connected");
    }

    emitInfo("Disconnecting client");
    cleanupClient();
    return createSuccessResponse("Client disconnected");
}

// ===== Вспомогательные методы =====

bool TcpServer::isCommandValid(const ParsedCMD& cmd, int expectedParams) const
{
    if (cmd.name.isEmpty()) {
        return false;
    }
    return cmd.paramCount() == expectedParams;
}

QByteArray TcpServer::createErrorResponse(const QString& error) const
{
    return QString("ERROR: %1\n").arg(error).toUtf8();
}

QByteArray TcpServer::createSuccessResponse(const QString& message) const
{
    return QString("OK: %1\n").arg(message).toUtf8();
}

void TcpServer::processSocketData(QTcpSocket* socket)
{
    if (!socket) {
        return;
    }

    if (socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

   bool isClient = (socket == m_clientSocket);
    SocketBuffer* buffer = isClient ? &m_clientBuffer : &m_transportBuffer;

    QDataStream in(socket);
    in.setVersion(DATA_STREAM_VERSION);

    // Защита от бесконечного цикла
    const int MAX_ITERATIONS = 100;
    int iterations = 0;

    while (iterations < MAX_ITERATIONS && socket->bytesAvailable() > 0) {
        iterations++;

        try {
            // Читаем размер
            if (buffer->nextBlockSize == 0) {
                if (socket->bytesAvailable() < static_cast<qint64>(sizeof(quint16))) {
                    break;  // Ждем еще данных
                }

                in >> buffer->nextBlockSize;

                // Проверка валидности размера
                if (buffer->nextBlockSize <= 0 || buffer->nextBlockSize > MAX_MESSAGE_SIZE) {
                    emitWarning(
                        QString("Invalid block size: %1 from %2")
                        .arg(buffer->nextBlockSize)
                        .arg(isClient ? "client" : "transport")
                    );
                    buffer->reset();
                    socket->readAll();  // Очищаем сокет от "мусора"
                    break;
                }
            }

            // Проверка наличия полных данных
            if (socket->bytesAvailable() < static_cast<qint64>(buffer->nextBlockSize)) {
                break;  // Ждем еще данных
            }

            // Читаем данные
            buffer->rawData.resize(buffer->nextBlockSize);
            qint64 bytesRead = in.readRawData(buffer->rawData.data(), buffer->nextBlockSize);

            // Проверка успешности чтения
            if (bytesRead != static_cast<qint64>(buffer->nextBlockSize)) {
                emitWarning(
                    QString("Failed to read full message. Expected: %1, Got: %2 from %3")
                    .arg(buffer->nextBlockSize)
                    .arg(bytesRead)
                    .arg(isClient ? "client" : "transport")
                );
                buffer->reset();
                socket->readAll();  // Очищаем сокет от "битых" данных
                break;
            }

            // Обрабатываем полученные данные
            if (isClient) {
                // Данные от клиента
                emit dataFromClient(buffer->rawData);
                // emitInfo("Received from client: " + QString(buffer->rawData));

                // Выполняем команду
                QByteArray response = execCMD(buffer->rawData);
                sendData(socket, response);

            }
            else {
                // Данные от транспорта (антенны)
                emit dataFromTransport(buffer->rawData);
                // emitInfo("Received from transport: " + QString(buffer->rawData));

                // Отправляем клиенту
                if (hasClient()) {
                    sendToClient(buffer->rawData);
                }
                else {
                    emitWarning("No client connected, data from transport discarded");
                }
            }

            // 10. Сбрасываем буфер для следующего сообщения
            buffer->reset();

        }
        catch (const std::exception& e) {
            emitError(
                QString("Exception in processSocketData: %1").arg(e.what())
            );
            buffer->reset();
            socket->readAll();
            break;
        }
    }

    // 11. Проверка на превышение итераций
    if (iterations >= MAX_ITERATIONS) {
        emitWarning("processSocketData: max iterations reached, clearing socket");
        socket->readAll();
        buffer->reset();
    }
}

// ===== Очистка =====

void TcpServer::cleanupClient()
{
    if (m_clientSocket) {
        m_clientSocket->disconnectFromHost();
        m_clientSocket->deleteLater();
        m_clientSocket = nullptr;
    }

    m_hasClient = false;
    m_clientBuffer.reset();  // ← Сбрасываем буфер

    emit clientDisconnected();
    emitInfo("Client cleaned up");
}


void TcpServer::cleanupTransport()
{
    if (m_transportSocket) {
        m_transportSocket->disconnectFromHost();
        m_transportSocket->deleteLater();
        m_transportSocket = nullptr;
    }

    m_isTransportConnected = false;
    m_transportBuffer.reset();  // ← Сбрасываем буфер

    emit transportDisconnected();
    emitInfo("Transport cleaned up");
}

// ===== Методы для эмита сигналов =====

void TcpServer::emitInfo(const QString& message)
{
    emit infoOccurred("[INFO] " + message);
}

void TcpServer::emitWarning(const QString& message)
{
    emit warningOccurred("[WARNING] " + message);
}

void TcpServer::emitError(const QString& message)
{
    emit errorOccurred("[ERROR] " + message);
}

// ===== Слоты для клиента =====

void TcpServer::onClientConnected()
{
    if (!m_isRunning || !m_server) {
        emitWarning("New connection rejected: server not running");
        return;
    }

    if (m_hasClient) {
        emitWarning("Client already connected, rejecting new connection");
        QTcpSocket* socket = m_server->nextPendingConnection();
        if (socket) {
            QByteArray msg = "ERROR: Server already has a client connected\n";
            sendData(socket, msg);
            socket->disconnectFromHost();
            socket->deleteLater();
        }
        return;
    }

    QTcpSocket* socket = m_server->nextPendingConnection();
    if (!socket) {
        emitError("Failed to get pending connection");
        return;
    }

    m_clientSocket = socket;
    m_hasClient = true;
    m_clientBuffer.reset();

    connect(socket, &QTcpSocket::readyRead,
        this, &TcpServer::onClientReadyRead);
    connect(socket, &QTcpSocket::disconnected,
        this, &TcpServer::onClientDisconnected);
    connect(socket, &QTcpSocket::errorOccurred,
        this, &TcpServer::onClientError);

    emit clientConnected();
    emitInfo("Client connected from " + socket->peerAddress().toString() + ":" +
        QString::number(socket->peerPort()));

    // Отправляем приветствие
    QByteArray welcome =
        "=== WELCOME ===\n"
        "Type HELP for available commands\n"
        "==============\n";

    sendData(socket, welcome);
}

void TcpServer::onClientReadyRead()
{
    if (!hasClient()) {
        return;
    }
    processSocketData(m_clientSocket);
}

void TcpServer::onClientDisconnected()
{
    emitInfo("Client disconnected");
    cleanupClient();
}

void TcpServer::onClientError(QAbstractSocket::SocketError error)
{
    QString errorMsg = m_clientSocket ? m_clientSocket->errorString() : "Unknown error";
    emitError("Client error: " + errorMsg + " (code: " + QString::number(error) + ")");
    cleanupClient();
}

// ===== Слоты для транспорта =====

void TcpServer::onTransportConnected()
{
    m_isTransportConnected = true;
    m_transportBuffer.reset();  
    emit transportConnected();
    emitInfo("Connected to transport at " + m_transportAddress + ":" +
        QString::number(m_transportPort));
}

void TcpServer::onTransportReadyRead()
{
    if (!isTransportConnected()) {
        return;
    }
    processSocketData(m_transportSocket);  
}

void TcpServer::onTransportDisconnected()
{
    m_isTransportConnected = false;
    emit transportDisconnected();
    emitWarning("Disconnected from transport");
}

void TcpServer::onTransportError(QAbstractSocket::SocketError error)
{
    QString errorMsg = m_transportSocket ? m_transportSocket->errorString() : "Unknown error";
    emitError("Transport error: " + errorMsg + " (code: " + QString::number(error) + ")");
    cleanupTransport();
}


