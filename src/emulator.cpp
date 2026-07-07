#include "include/emulator.h"
#include <QHostAddress>
#include <QDateTime>
#include <QMetaObject>
#include <QThread>

const QStringList EmuManager::MANAGER_COMMANDS = {
    "HELP", "H", "?",
    "STATUS", "ST",
    "DISCONNECT", "DC",
    "ANTENN_STATUS", "AS",
    //"CONFIG", "CFG"
};

EmuManager::EmuManager(IAntenn* antenn, QObject* parent)
    : QObject(parent)
    , m_p_antenn(antenn)
    , m_server(nullptr)
    , m_serverPort(0)
    , m_isRunning(false)
{
    if (!m_p_antenn) {
        errorOccurred("EmuManager: antenna pointer is null");
        return;
    }

    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection,
        this, &EmuManager::onClientConnected);

    infoOccurred("EmuManager created");
}

EmuManager::~EmuManager()
{
    stop();
    infoOccurred("EmuManager destroyed");
}

bool EmuManager::start(quint16 port)
{
    if (m_isRunning) {
        warningOccurred("Server already running on port " + QString::number(m_serverPort));
        return false;
    }

    if (!m_server->listen(QHostAddress::Any, port)) {
        errorOccurred("Failed to start server on port " + QString::number(port) + ": " +
            m_server->errorString());
        return false;
    }

    m_serverPort = port;
    m_isRunning = true;

    infoOccurred("Server started on port " + QString::number(port));
    emit serverStarted(port);

    return true;
}

void EmuManager::stop()
{
    if (!m_isRunning) {
        warningOccurred("Server already stopped");
        return;
    }

    m_isRunning = false;
    m_server->close();

    if (m_clientSocket) {
        m_clientSocket->disconnectFromHost();
        m_clientSocket->deleteLater();
        m_clientSocket = nullptr;
    }
    m_hasClient = false;
    m_clientBuffer.reset();

    infoOccurred("Server stopped");
    emit serverStopped();
}

bool EmuManager::isRunning() const
{
    return m_isRunning && m_server->isListening();
}

bool EmuManager::hasClient() const
{
    return m_hasClient && m_clientSocket &&
        m_clientSocket->state() == QAbstractSocket::ConnectedState;
}

void EmuManager::sendToClient(const QByteArray& data)
{
    if (data.isEmpty()) {
        warningOccurred("Attempt to send empty data to client");
        return;
    }

    if (!m_hasClient || !m_clientSocket) {
        errorOccurred("Cannot send to client: no client connected");
        return;
    }

    sendData(m_clientSocket, data);
}

void EmuManager::sendToAllClients(const QByteArray& data)
{
    sendToClient(data);
}

void EmuManager::sendData(QTcpSocket* socket, const QByteArray& data)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setVersion(DATA_STREAM_VERSION);
    out << quint16(data.size());
    out.writeRawData(data.data(), data.size());

    qint64 written = socket->write(packet);
    if (written == -1) {
        errorOccurred("Failed to send data: " + socket->errorString());
    }
    else {
        socket->flush();
    }
}

// ===== Обработка клиентов =====

void EmuManager::onClientConnected()
{
    if (!m_isRunning || !m_server) {
        return;
    }

    if (m_hasClient) {
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
        errorOccurred("Failed to get pending connection");
        return;
    }

    m_clientSocket = socket;
    m_hasClient = true;
    m_clientBuffer.reset();

    connect(socket, &QTcpSocket::readyRead, this, &EmuManager::onClientReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &EmuManager::onClientDisconnected);
    connect(socket, &QTcpSocket::errorOccurred, this, &EmuManager::onClientError);

    emit clientConnected();
    infoOccurred("Client connected from " + socket->peerAddress().toString() + ":" +
        QString::number(socket->peerPort()));

    QByteArray welcome =
        "=== EMULATOR ANTENN ===\n"
        "Type HELP for available commands\n"
        "========================\n";

    sendData(socket, welcome);
}

void EmuManager::onClientReadyRead()
{
    if (!m_hasClient || !m_clientSocket) {
        return;
    }
    processSocketData(m_clientSocket);
}

void EmuManager::onClientDisconnected()
{
    infoOccurred("Client disconnected");
    cleanupClient();
}

void EmuManager::onClientError(QAbstractSocket::SocketError error)
{
    QString errorMsg = m_clientSocket ? m_clientSocket->errorString() : "Unknown error";
    errorOccurred("Client error: " + errorMsg + " (code: " + QString::number(error) + ")");
    cleanupClient();
}

void EmuManager::cleanupClient()
{
    if (m_clientSocket) {
        m_clientSocket->disconnectFromHost();
        m_clientSocket->deleteLater();
        m_clientSocket = nullptr;
    }
    m_hasClient = false;
    m_clientBuffer.reset();
    emit clientDisconnected();
}

void EmuManager::processSocketData(QTcpSocket* socket)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QDataStream in(socket);
    in.setVersion(DATA_STREAM_VERSION);

    SocketBuffer& buffer = m_clientBuffer;

    const int MAX_ITERATIONS = 100;
    int iterations = 0;

    while (iterations < MAX_ITERATIONS && socket->bytesAvailable() > 0) {
        iterations++;

        try {
            if (buffer.nextBlockSize == 0) {
                if (socket->bytesAvailable() < static_cast<qint64>(sizeof(quint16))) {
                    break;
                }

                in >> buffer.nextBlockSize;

                if (buffer.nextBlockSize <= 0 || buffer.nextBlockSize > MAX_MESSAGE_SIZE) {
                    warningOccurred("Invalid block size: " + QString::number(buffer.nextBlockSize) + " from client");
                    buffer.reset();
                    socket->readAll();
                    break;
                }
            }

            if (socket->bytesAvailable() < static_cast<qint64>(buffer.nextBlockSize)) {
                break;
            }

            buffer.rawData.resize(buffer.nextBlockSize);
            qint64 bytesRead = in.readRawData(buffer.rawData.data(), buffer.nextBlockSize);

            if (bytesRead != static_cast<qint64>(buffer.nextBlockSize)) {
                warningOccurred("Failed to read full message. Expected: " +
                    QString::number(buffer.nextBlockSize) + ", Got: " + QString::number(bytesRead));
                buffer.reset();
                socket->readAll();
                break;
            }

            emit dataFromClient(buffer.rawData);

            QByteArray response = execCMD(buffer.rawData);
            sendData(socket, response);

            buffer.reset();

        }
        catch (const std::exception& e) {
            errorOccurred("Exception in processSocketData: " + QString(e.what()));
            buffer.reset();
            socket->readAll();
            break;
        }
    }

    if (iterations >= MAX_ITERATIONS) {
        warningOccurred("processSocketData: max iterations reached");
        socket->readAll();
        buffer.reset();
    }
}

// ===== Обработка команд =====

EmuManager::ParsedCMD EmuManager::parsingToCMD(const QByteArray& data)
{
    ParsedCMD result;
    QByteArray trimmed = data.trimmed();

    if (trimmed.isEmpty()) {
        return result;
    }

    int spacePos = trimmed.indexOf(' ');

    if (spacePos == -1) {
        result.name = QString(trimmed).toUpper();
    }
    else {
        result.name = QString(trimmed.left(spacePos)).toUpper();
        QByteArray paramParts = trimmed.mid(spacePos + 1);

        for (const QByteArray& param : paramParts.split(' ')) {
            QByteArray trimmedParam = param.trimmed();
            if (!trimmedParam.isEmpty()) {
                result.params.append(trimmedParam);
            }
        }
    }

    result.isAntennaCommand = !MANAGER_COMMANDS.contains(result.name);
    return result;
}

QByteArray EmuManager::execCMD(const QByteArray& request)
{
    try {
        ParsedCMD parsed = parsingToCMD(request);

        if (!parsed.isValid()) {
            warningOccurred("Invalid command received: " + QString(request));
            return createErrorResponse("Empty or invalid command");
        }

        infoOccurred("Executing command: " + parsed.name +
            " (target: " + (parsed.isAntennaCommand ? "antenna" : "manager") + ")" +
            " with " + QString::number(parsed.params.size()) + " parameters");

        QByteArray response;

        if (parsed.isAntennaCommand) {
            response = processAntennaCommand(parsed);
        }
        else {
            response = processManagerCommand(parsed);
        }

        return response;

    }
    catch (const std::exception& e) {
        QString error = "Exception in execCMD: " + QString(e.what());
        errorOccurred(error);
        return createErrorResponse(error);
    }
}

QByteArray EmuManager::processManagerCommand(const ParsedCMD& parsed)
{
    QString cmd = parsed.name;
    QByteArray response;

    if (cmd == "HELP" || cmd == "H" || cmd == "?") {
        response = cmdHelp();
    }
    else if (cmd == "STATUS" || cmd == "ST") {
        response = cmdStatus();
    }
    else if (cmd == "DISCONNECT" || cmd == "DC") {
        response = cmdDisconnect();
    }
    else if (cmd == "ANTENN_STATUS" || cmd == "AS") {
        response = cmdAntennStatus();
    }
    else {
        response = createErrorResponse("Unknown manager command: " + cmd);
    }

    return response;
}

QByteArray EmuManager::processAntennaCommand(const ParsedCMD& parsed)
{
    if (!m_p_antenn) {
        return createErrorResponse("Antenna not available");
    }

    QString cmd = parsed.name;
    QByteArray response;

    // КОМАНДЫ ДЛЯ ЧТЕНИЯ
    if (cmd == "STATUS") {
        return cmdAntennStatus();
    }

    if (cmd == "CONFIG") {
        if (parsed.paramCount() >= 2) {
            QString action = QString(parsed.params[0]).toUpper();
            QString path = QString(parsed.params[1]);

            if (action == "LOAD") {
                bool result;
                QMetaObject::invokeMethod(m_p_antenn, [this, path, &result]() {
                    result = m_p_antenn->loadConfig(path);
                    }, Qt::BlockingQueuedConnection);

                return result ?
                    createSuccessResponse("Config loaded: " + path) :
                    createErrorResponse("Failed to load config: " + path);
            }
            else if (action == "SAVE") {
                bool result;
                QMetaObject::invokeMethod(m_p_antenn, [this, path, &result]() {
                    result = m_p_antenn->saveConfig(path);
                    }, Qt::BlockingQueuedConnection);

                return result ?
                    createSuccessResponse("Config saved: " + path) :
                    createErrorResponse("Failed to save config: " + path);
            }
            else {
                return createErrorResponse("Unknown config action: " + action);
            }
        }
        return createErrorResponse("CONFIG requires action and path");
    }

    // КОМАНДЫ ДЛЯ ИЗМЕНЕНИЯ
    bool success = false;

    // Вспомогательная лямбда для выполнения команды
    auto executeCommand = [this, &success](auto&& func) {
        QMetaObject::invokeMethod(m_p_antenn, [this, &success, &func]() {
            success = func();
            }, Qt::BlockingQueuedConnection);
        return success;
        };

    if (cmd == "SET_AZIMUTH") {
        if (parsed.paramCount() < 1) {
            return createErrorResponse("SET_AZIMUTH requires angle parameter");
        }
        double angle = parsed.params[0].toDouble();

        success = executeCommand([this, angle]() {
            return m_p_antenn->setAzimuth(angle);
            });

        response = success ?
            createSuccessResponse("Azimuth set to " + QString::number(angle, 'f', 2)) :
            createErrorResponse("Failed to set azimuth to " + QString::number(angle, 'f', 2));
    }
    else if (cmd == "SET_ELEVATION") {
        if (parsed.paramCount() < 1) {
            return createErrorResponse("SET_ELEVATION requires angle parameter");
        }
        double angle = parsed.params[0].toDouble();

        success = executeCommand([this, angle]() {
            return m_p_antenn->setElevation(angle);
            });

        response = success ?
            createSuccessResponse("Elevation set to " + QString::number(angle, 'f', 2)) :
            createErrorResponse("Failed to set elevation to " + QString::number(angle, 'f', 2));
    }
    else if (cmd == "SET_POLARIZATION") {
        if (parsed.paramCount() < 1) {
            return createErrorResponse("SET_POLARIZATION requires angle parameter");
        }
        double angle = parsed.params[0].toDouble();

        success = executeCommand([this, angle]() {
            return m_p_antenn->setPolarization(angle);
            });

        response = success ?
            createSuccessResponse("Polarization set to " + QString::number(angle, 'f', 2)) :
            createErrorResponse("Failed to set polarization to " + QString::number(angle, 'f', 2));
    }
    else if (cmd == "SET_SPEED_MODE") {
        if (parsed.paramCount() < 1) {
            return createErrorResponse("SET_SPEED_MODE requires mode parameter");
        }
        int mode = parsed.params[0].toInt();

        success = executeCommand([this, mode]() {
            return m_p_antenn->setSpeedMode(mode);
            });

        response = success ?
            createSuccessResponse("Speed mode set to " + QString::number(mode)) :
            createErrorResponse("Failed to set speed mode to " + QString::number(mode));
    }
    else if (cmd == "CALIBRATE") {
        // calibrate не возвращает bool
        QMetaObject::invokeMethod(m_p_antenn, [this]() {
            m_p_antenn->calibrate();
            }, Qt::BlockingQueuedConnection);
        return createSuccessResponse("Calibration completed");
    }
    else if (cmd == "START") {
        success = executeCommand([this]() {
            return m_p_antenn->start();
            });

        response = success ?
            createSuccessResponse("Antenna started") :
            createErrorResponse("Failed to start antenna");
    }
    else if (cmd == "STOP") {
        success = executeCommand([this]() {
            return m_p_antenn->stop();
            });

        response = success ?
            createSuccessResponse("Antenna stopped") :
            createErrorResponse("Failed to stop antenna");
    }
    else {
        return createErrorResponse("Unknown antenna command: " + cmd);
    }

    return response;
}

QByteArray EmuManager::cmdHelp()
{
    QString help =
        "=== EMULATOR MANAGER COMMANDS ===\n"
        "HELP (H, ?)                - Show this help\n"
        "STATUS (ST)                - Show manager status\n"
        "DISCONNECT (DC)            - Disconnect all clients\n"
        "ANTENN_STATUS (AS)         - Show antenna status\n"
        "\n"
        "=== ANTENNA COMMANDS ===\n"
        "SET_AZIMUTH <angle>        - Set azimuth angle (degrees)\n"
        "SET_ELEVATION <angle>      - Set elevation angle (degrees)\n"
        "SET_POLARIZATION <angle>   - Set polarization angle (degrees)\n"
        "SET_SPEED_MODE <mode>      - Set speed mode (0, 1, 2, ...)\n"
        "CALIBRATE                  - Calibrate antenna\n"
        "START                      - Start antenna\n"
        "STOP                       - Stop antenna\n"
        "STATUS                     - Get antenna status\n"
        "CONFIG LOAD <path>         - Load config from file\n"
        "CONFIG SAVE <path>         - Save config to file\n"
        "================================";

    return help.toUtf8();
}

QByteArray EmuManager::cmdStatus()
{
    QString status = QString(
        "=== MANAGER STATUS ===\n"
        "Running: %1\n"
        "Port: %2\n"
        "Client: %3\n"
        "Antenna: %4\n"
        "Antenna running: %5\n"
        "====================="
    )
        .arg(m_isRunning ? "YES" : "NO")
        .arg(m_serverPort)
        .arg(hasClient())
        .arg(m_p_antenn ? "AVAILABLE" : "NOT AVAILABLE")
        .arg(m_p_antenn && m_p_antenn->isRunning() ? "YES" : "NO");

    return status.toUtf8();
}

QByteArray EmuManager::cmdDisconnect()
{
    if (!m_hasClient || !m_clientSocket) {
        return createErrorResponse("No clients connected");
    }

    m_clientSocket->disconnectFromHost();
    m_clientSocket->deleteLater();
    m_clientSocket = nullptr;
    m_hasClient = false;
    m_clientBuffer.reset();

    emit clientDisconnected();
    return createSuccessResponse("Client disconnected");
}

QByteArray EmuManager::cmdAntennStatus()
{
    if (!m_p_antenn) {
        return createErrorResponse("Antenna not available");
    }

    AntennaStatus status;
    AntennaConfig config;

    // Получаем данные из антенны
    QMetaObject::invokeMethod(m_p_antenn, [this, &status, &config]() {
        status = m_p_antenn->getStatus();
        config = m_p_antenn->getConfig();
        }, Qt::BlockingQueuedConnection);

    QString response;

    // === СТАТУС ===
    response += "=== ANTENNA STATUS ===\n";
    response += "Azimuth: " + QString::number(status.azimuth, 'f', 2) + "\n";
    response += "Elevation: " + QString::number(status.elevation, 'f', 2) + "\n";
    response += "Polarization: " + QString::number(status.polarization, 'f', 2) + "\n";
    response += "Target Azimuth: " + QString::number(status.targetAzimuth, 'f', 2) + "\n";
    response += "Target Elevation: " + QString::number(status.targetElevation, 'f', 2) + "\n";
    response += "Target Polarization: " + QString::number(status.targetPolarization, 'f', 2) + "\n";
    response += "Speed: " + QString::number(status.speed, 'f', 2) + " deg/sec\n";
    response += "Speed Mode: " + QString::number(status.speedMode) + "\n";
    response += "Moving: " + QString(status.isMoving ? "YES" : "NO") + "\n";
    response += "Calibrated: " + QString(status.isCalibrated ? "YES" : "NO") + "\n";
    response += "Running: " + QString(status.isRunning ? "YES" : "NO") + "\n";
    response += "Uptime: " + QString::number(status.uptimeSeconds) + " seconds\n";

    // === КОНФИГУРАЦИЯ (ЛИМИТЫ + РЕЖИМЫ СКОРОСТЕЙ) ===
    response += "\n=== ANTENNA CONFIG ===\n";

    // Лимиты
    response += "Min Azimuth: " + QString::number(config.limits.minAzimuth, 'f', 1) + "\n";
    response += "Max Azimuth: " + QString::number(config.limits.maxAzimuth, 'f', 1) + "\n";
    response += "Min Elevation: " + QString::number(config.limits.minElevation, 'f', 1) + "\n";
    response += "Max Elevation: " + QString::number(config.limits.maxElevation, 'f', 1) + "\n";
    response += "Min Polarization: " + QString::number(config.limits.minPolarization, 'f', 1) + "\n";
    response += "Max Polarization: " + QString::number(config.limits.maxPolarization, 'f', 1) + "\n";
    response += "Max Speed: " + QString::number(config.limits.maxSpeed, 'f', 1) + " deg/sec\n";

    // Режимы скоростей
    response += "Speed Modes Count: " + QString::number(config.speedModes.size()) + "\n";
    for (const SpeedMode& mode : config.speedModes) {
        response += QString("Speed Mode %1: %2 (%3 deg/sec)\n")
            .arg(mode.id)
            .arg(mode.name)
            .arg(mode.speed, 0, 'f', 1);
    }

    // Значения по умолчанию
    response += "Default Azimuth: " + QString::number(config.defaultAzimuth, 'f', 1) + "\n";
    response += "Default Elevation: " + QString::number(config.defaultElevation, 'f', 1) + "\n";
    response += "Default Polarization: " + QString::number(config.defaultPolarization, 'f', 1) + "\n";
    response += "Default Speed Mode: " + QString::number(config.defaultSpeedMode) + "\n";

    response += "=====================";

    return response.toUtf8();
}

QByteArray EmuManager::cmdConfig()
{
    if (!m_p_antenn) {
        return createErrorResponse("Antenna not available");
    }

    AntennaConfig config;
    QMetaObject::invokeMethod(m_p_antenn, [this, &config]() {
        config = m_p_antenn->getConfig();
        }, Qt::BlockingQueuedConnection);

    QString response;
    response += "=== ANTENNA CONFIG ===\n";

    // Лимиты
    response += "Min Azimuth: " + QString::number(config.limits.minAzimuth, 'f', 1) + "\n";
    response += "Max Azimuth: " + QString::number(config.limits.maxAzimuth, 'f', 1) + "\n";
    response += "Min Elevation: " + QString::number(config.limits.minElevation, 'f', 1) + "\n";
    response += "Max Elevation: " + QString::number(config.limits.maxElevation, 'f', 1) + "\n";
    response += "Min Polarization: " + QString::number(config.limits.minPolarization, 'f', 1) + "\n";
    response += "Max Polarization: " + QString::number(config.limits.maxPolarization, 'f', 1) + "\n";
    response += "Max Speed: " + QString::number(config.limits.maxSpeed, 'f', 1) + " deg/sec\n";

    // Режимы скоростей
    response += "Speed Modes Count: " + QString::number(config.speedModes.size()) + "\n";
    for (const SpeedMode& mode : config.speedModes) {
        response += QString("Speed Mode %1: %2 (%3 deg/sec)\n")
            .arg(mode.id)
            .arg(mode.name)
            .arg(mode.speed, 0, 'f', 1);
    }

    // Значения по умолчанию
    response += "Default Azimuth: " + QString::number(config.defaultAzimuth, 'f', 1) + "\n";
    response += "Default Elevation: " + QString::number(config.defaultElevation, 'f', 1) + "\n";
    response += "Default Polarization: " + QString::number(config.defaultPolarization, 'f', 1) + "\n";
    response += "Default Speed Mode: " + QString::number(config.defaultSpeedMode) + "\n";

    response += "=====================";

    return response.toUtf8();
}


bool EmuManager::isCommandValid(const ParsedCMD& cmd, int expectedParams) const
{
    if (cmd.name.isEmpty()) {
        return false;
    }
    return cmd.paramCount() == expectedParams;
}

QByteArray EmuManager::createErrorResponse(const QString& error) const
{
    return QString("ERROR: %1\n").arg(error).toUtf8();
}

QByteArray EmuManager::createSuccessResponse(const QString& message) const
{
    return QString("OK: %1\n").arg(message).toUtf8();
}

