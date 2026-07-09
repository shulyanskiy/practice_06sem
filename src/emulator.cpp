#include "include/emulator.h"
#include "include/AntennaFactory.h"
#include <QHostAddress>
#include <QDateTime>
#include <QMetaObject>
#include <QThread>
#include <QFile>
#include <QDir>

const QStringList EmuManager::MANAGER_COMMANDS = {
    "HELP", "H", "?",
    "STATUS", "ST",
    "DISCONNECT", "DC",
    "ANTENN_STATUS", "AS",
    "GET_ANTENNA_TYPES", "GAT",
    "SELECT_ANTENNA_TYPE", "SAT"
};


void EmuManager::loadAntennaTypes()
{
    qDebug() << "Current working directory:" << QDir::currentPath();
    QSettings typeSettings("config/antenna_types.ini", QSettings::IniFormat);
    int count = typeSettings.value("Types/Count", 0).toInt();
    for (int i = 0; i < count; ++i) {
        QString type = typeSettings.value(QString("Types/Type%1").arg(i)).toString();
        QString configPath = typeSettings.value(QString("Types/Config%1").arg(i)).toString();
        qDebug() << type << configPath;

        if (!type.isEmpty() && !configPath.isEmpty()) {
            if (QFile::exists(configPath)) {
                AntennaFactory::instance().registerAntennaTypeWithDefaultCreator(type, configPath);
                infoOccurred("Registered antenna type: " + type + " (config: " + configPath + ")");
                
            }
            else {
                warningOccurred("Config not found for type: " + type + " (" + configPath + ")");
            }
        }
    }
}

EmuManager::EmuManager(QObject* parent)
    : QObject(parent)
    , m_p_antenn(nullptr)
    , m_server(nullptr)
    , m_serverPort(0)
    , m_isRunning(false)
    , m_clientSocket(nullptr)
    , m_hasClient(false)
    , m_antennaThread(nullptr)
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection,
        this, &EmuManager::onClientConnected);

    // Загружаем типы антенн из конфига
    loadAntennaTypes();

    // Проверяем, есть ли зарегистрированные типы
    QStringList types = AntennaFactory::instance().getAvailableTypes();

    if (types.isEmpty()) {
        errorOccurred("No antenna types registered! Please add types via UI or check config files.");
        m_p_antenn = nullptr;
    }
    else {
        // Создаем антенну в отдельном потоке
        createAntennaInThread(types.first());
    }

    infoOccurred("EmuManager created");
}

EmuManager::~EmuManager()
{
    stop();

    // Останавливаем поток антенны
    if (m_antennaThread) {
        m_antennaThread->quit();
        m_antennaThread->wait();
        delete m_antennaThread;
        m_antennaThread = nullptr;
    }

    infoOccurred("EmuManager destroyed");
}


void EmuManager::createAntennaInThread(const QString& type)
{
    // 3. Создаем новую антенну
    IAntenn* newAntenn = AntennaFactory::instance().createAntenna(type);
    if (!newAntenn) {
        errorOccurred("Failed to create antenna of type: " + type);
        delete m_antennaThread;
        m_antennaThread = nullptr;
        return;
    }

    // 1. Останавливаем и удаляем старые ресурсы (если есть)
    if (m_antennaThread) {
        // Отключаем сигналы от старого потока
        disconnect(m_antennaThread, nullptr, this, nullptr);

        m_antennaThread->quit();
        m_antennaThread->wait();
        delete m_antennaThread;
        m_antennaThread = nullptr;
    }

    if (m_p_antenn) {
        // Останавливаем старую антенну
        QMetaObject::invokeMethod(m_p_antenn, [this]() {
            m_p_antenn->stop();
            }, Qt::BlockingQueuedConnection);

        // Отключаем сигналы
        disconnect(m_p_antenn, nullptr, this, nullptr);
        m_p_antenn->deleteLater();
        m_p_antenn = nullptr;
    }

    // 2. Создаем новый поток
    m_antennaThread = new QThread(this);


    // 4. Перемещаем в поток
    newAntenn->moveToThread(m_antennaThread);

    // 5. Настраиваем сигналы
    connect(m_antennaThread, &QThread::started, [newAntenn]() {
        newAntenn->start();
        });

    connect(m_antennaThread, &QThread::finished, [this, newAntenn]() {
        // Отключаем сигналы перед удалением
        disconnect(newAntenn, nullptr, this, nullptr);
        newAntenn->stop();
        newAntenn->deleteLater();

        // Обнуляем указатель, если это текущая антенна
        if (m_p_antenn == newAntenn) {
            m_p_antenn = nullptr;
        }
        });

    // 6. Сохраняем новую антенну
    m_p_antenn = newAntenn;

    // 7. Подключаем сигналы антенны
    connect(newAntenn, &IAntenn::infoOccurred, this, &EmuManager::infoOccurred);
    connect(newAntenn, &IAntenn::warningOccurred, this, &EmuManager::warningOccurred);
    connect(newAntenn, &IAntenn::errorOccurred, this, &EmuManager::errorOccurred);

    // 8. Запускаем поток
    m_antennaThread->start();

    infoOccurred("Antenna created in separate thread: " + type);
}

void EmuManager::setAntenna(IAntenn* antenn)
{
    if (m_p_antenn && antenn != m_p_antenn) {
        // Отключаем сигналы
        disconnect(m_p_antenn, nullptr, this, nullptr);
        m_p_antenn->deleteLater();
    }
    m_p_antenn = antenn;

    if (m_p_antenn) {
        // Подключаем сигналы антенны
        connect(m_p_antenn, &IAntenn::infoOccurred, this, &EmuManager::infoOccurred);
        connect(m_p_antenn, &IAntenn::warningOccurred, this, &EmuManager::warningOccurred);
        connect(m_p_antenn, &IAntenn::errorOccurred, this, &EmuManager::errorOccurred);

        infoOccurred("Antenna set: " + m_p_antenn->getAntennaType());
    }
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

QStringList EmuManager::getAntennaTypes() const
{
    return AntennaFactory::instance().getAvailableTypes();
}

bool EmuManager::selectAntennaType(const QString& type)
{
    QStringList types = getAntennaTypes();
    if (!types.contains(type)) {
        errorOccurred("Unknown antenna type: " + type);
        return false;
    }

    if (!m_isSwitching.testAndSetOrdered(0, 1)) {
        warningOccurred("Antenna type switch already in progress");
        return false;
    }

    // Просто создаем новую антенну
    // Старая удаляется внутри createAntennaInThread()
    createAntennaInThread(type);

    m_isSwitching.testAndSetOrdered(1, 0);

    infoOccurred("Switched to antenna type: " + type);
    return true;
}

void EmuManager::sendData(QTcpSocket* socket, const QByteArray& data)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    // Логируем отправляемые данные (сырые байты)
    emit logRawData("SEND", data);

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
        "=== EMULATOR ANTENNA SERVER ===\n"
        "Type HELP for available commands\n"
        "================================\n";

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

            // Логируем полученные данные (сырые байты)
            emit logRawData("RECV", buffer.rawData);
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
        qDebug()  << "=== EmuManager::execCMD : ThreadId -> " << QThread::currentThreadId()
            << Qt::endl << "=== \tRequest : " << request;

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
    else if (cmd == "GET_ANTENNA_TYPES" || cmd == "GAT") {
        response = cmdGetAntennaTypes();
    }
    else if (cmd == "SELECT_ANTENNA_TYPE" && parsed.paramCount() >= 1) {
        response = cmdSelectAntennaType(QString(parsed.params[0]));
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

    // SET_POSITION - установка всех трех координат
    if (cmd == "SET_POSITION") {
        if (parsed.paramCount() < 3) {
            return createErrorResponse("SET_POSITION requires azimuth, elevation, polarization");
        }

        double azimuth = parsed.params[0].toDouble();
        double elevation = parsed.params[1].toDouble();
        double polarization = parsed.params[2].toDouble();

        QMetaObject::invokeMethod(m_p_antenn, [this, azimuth, elevation, polarization]() {
            m_p_antenn->setPosition(azimuth, elevation, polarization);
            }, Qt::BlockingQueuedConnection);

        return createSuccessResponse(QString("Position set: AZ=%1 EL=%2 POL=%3")
            .arg(azimuth, 0, 'f', 2)
            .arg(elevation, 0, 'f', 2)
            .arg(polarization, 0, 'f', 2));
    }

    // SET_AZIMUTH
    if (cmd == "SET_AZIMUTH") {
        if (parsed.paramCount() < 1) {
            return createErrorResponse("SET_AZIMUTH requires angle parameter");
        }
        double angle = parsed.params[0].toDouble();

        QMetaObject::invokeMethod(m_p_antenn, [this, angle]() {
            m_p_antenn->setAzimuth(angle);
            }, Qt::BlockingQueuedConnection);

        return createSuccessResponse("Azimuth set to " + QString::number(angle, 'f', 2));
    }

    // SET_ELEVATION
    if (cmd == "SET_ELEVATION") {
        if (parsed.paramCount() < 1) {
            return createErrorResponse("SET_ELEVATION requires angle parameter");
        }
        double angle = parsed.params[0].toDouble();

        QMetaObject::invokeMethod(m_p_antenn, [this, angle]() {
            m_p_antenn->setElevation(angle);
            }, Qt::BlockingQueuedConnection);

        return createSuccessResponse("Elevation set to " + QString::number(angle, 'f', 2));
    }

    // SET_POLARIZATION
    if (cmd == "SET_POLARIZATION") {
        if (parsed.paramCount() < 1) {
            return createErrorResponse("SET_POLARIZATION requires angle parameter");
        }
        double angle = parsed.params[0].toDouble();

        QMetaObject::invokeMethod(m_p_antenn, [this, angle]() {
            m_p_antenn->setPolarization(angle);
            }, Qt::BlockingQueuedConnection);

        return createSuccessResponse("Polarization set to " + QString::number(angle, 'f', 2));
    }

    // SET_SPEED_MODE
    if (cmd == "SET_SPEED_MODE") {
        if (parsed.paramCount() < 1) {
            return createErrorResponse("SET_SPEED_MODE requires mode parameter");
        }
        int mode = parsed.params[0].toInt();

        QMetaObject::invokeMethod(m_p_antenn, [this, mode]() {
            m_p_antenn->setSpeedMode(mode);
            }, Qt::BlockingQueuedConnection);

        return createSuccessResponse("Speed mode set to " + QString::number(mode));
    }

    // RESET (вместо CALIBRATE)
    if (cmd == "RESET") {
        QMetaObject::invokeMethod(m_p_antenn, [this]() {
            m_p_antenn->reset();
            }, Qt::BlockingQueuedConnection);
        return createSuccessResponse("Antenna reset completed");
    }

    // START
    if (cmd == "START") {
        QMetaObject::invokeMethod(m_p_antenn, [this]() {
            m_p_antenn->start();
            }, Qt::BlockingQueuedConnection);
        return createSuccessResponse("Antenna started");
    }

    // STOP
    if (cmd == "STOP") {
        QMetaObject::invokeMethod(m_p_antenn, [this]() {
            m_p_antenn->stop();
            }, Qt::BlockingQueuedConnection);
        return createSuccessResponse("Antenna stopped");
    }

    return createErrorResponse("Unknown antenna command: " + cmd);
}

QByteArray EmuManager::cmdHelp()
{
    QString help =
        "=== EMULATOR MANAGER COMMANDS ===\n"
        "HELP (H, ?)                    - Show this help\n"
        "STATUS (ST)                    - Show manager status\n"
        "DISCONNECT (DC)                - Disconnect all clients\n"
        "ANTENN_STATUS (AS)             - Show antenna status\n"
        "GET_ANTENNA_TYPES (GAT)        - Get available antenna types\n"
        "SELECT_ANTENNA_TYPE <type>     - Switch to antenna type\n"
        "\n"
        "=== ANTENNA COMMANDS ===\n"
        "SET_POSITION <az> <el> <pol>   - Set all angles (degrees)\n"
        "SET_AZIMUTH <angle>            - Set azimuth angle (degrees)\n"
        "SET_ELEVATION <angle>          - Set elevation angle (degrees)\n"
        "SET_POLARIZATION <angle>       - Set polarization angle (degrees)\n"
        "SET_SPEED_MODE <mode>          - Set speed mode (0, 1, 2, ...)\n"
        "RESET                          - Reset antenna to default\n"
        "START                          - Start antenna\n"
        "STOP                           - Stop antenna\n"
        "STATUS                         - Get antenna status\n"
        "CONFIG LOAD <path>             - Load config from file\n"
        "CONFIG SAVE <path>             - Save config to file\n"
        "=================================";

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
        "Antenna type: %5\n"
        "Antenna running: %6\n"
        "====================="
    )
        .arg(m_isRunning ? "YES" : "NO")
        .arg(m_serverPort)
        .arg(hasClient() ? "CONNECTED" : "DISCONNECTED")
        .arg(m_p_antenn ? "AVAILABLE" : "NOT AVAILABLE")
        .arg(m_p_antenn ? m_p_antenn->getAntennaType() : "NONE")
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

    QMetaObject::invokeMethod(m_p_antenn, [this, &status, &config]() {
        status = m_p_antenn->getStatus();
        config = m_p_antenn->getConfig();
        }, Qt::BlockingQueuedConnection);

    qDebug() << QString::number(m_p_antenn->getStatus().speed, 'f', 2) << Qt::endl;

    QString response;
    response += "=== ANTENNA STATUS ===\n";
    response += "Antenna Type: " + status.type + "\n";
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

    response += "\n=== ANTENNA CONFIG ===\n";
    response += "Min Azimuth: " + QString::number(config.limits.minAzimuth, 'f', 1) + "\n";
    response += "Max Azimuth: " + QString::number(config.limits.maxAzimuth, 'f', 1) + "\n";
    response += "Min Elevation: " + QString::number(config.limits.minElevation, 'f', 1) + "\n";
    response += "Max Elevation: " + QString::number(config.limits.maxElevation, 'f', 1) + "\n";
    response += "Min Polarization: " + QString::number(config.limits.minPolarization, 'f', 1) + "\n";
    response += "Max Polarization: " + QString::number(config.limits.maxPolarization, 'f', 1) + "\n";
    response += "Max Speed: " + QString::number(config.limits.maxSpeed, 'f', 1) + " deg/sec\n";

    response += "Speed Modes Count: " + QString::number(config.speedModes.size()) + "\n";
    for (const SpeedMode& mode : config.speedModes) {
        response += QString("Speed Mode %1: %2 (%3 deg/sec)\n")
            .arg(mode.id)
            .arg(mode.name)
            .arg(mode.speed, 0, 'f', 1);
    }

    response += "Default Azimuth: " + QString::number(config.defaultAzimuth, 'f', 1) + "\n";
    response += "Default Elevation: " + QString::number(config.defaultElevation, 'f', 1) + "\n";
    response += "Default Polarization: " + QString::number(config.defaultPolarization, 'f', 1) + "\n";
    response += "Default Speed Mode: " + QString::number(config.defaultSpeedMode) + "\n";
    response += "Antenna Type: " + m_p_antenn->getAntennaType() + "\n";
    response += "=====================";

    return response.toUtf8();
}

QByteArray EmuManager::cmdGetAntennaTypes()
{
    QStringList types = getAntennaTypes();

    QString response = "=== ANTENNA TYPES ===\n";
    response += "Types: " + QString::number(types.size()) + ": ";
    response += types.join(", ");
    response += "\n=====================";

    return response.toUtf8();
}

QByteArray EmuManager::cmdSelectAntennaType(const QString& type)
{
    if (selectAntennaType(type)) {
        return createSuccessResponse("Switched to antenna type: " + type);
    }
    else {
        return createErrorResponse("Failed to switch to antenna type: " + type);
    }
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
