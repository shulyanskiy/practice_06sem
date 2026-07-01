// tcpantenna.cpp
#include "tcpantenna.h"
#include <QHostAddress>
#include <QDataStream>
#include <cmath>

TcpAntenna::TcpAntenna(QObject* parent)
    : IAntenn(parent)
    , m_socket(nullptr)
    , m_serverPort(0)
    , m_isConnected(false)
    , m_nextBlockSize(0)
    , m_azimuth(0.0)
    , m_elevation(0.0)
    , m_targetAzimuth(0.0)
    , m_targetElevation(0.0)
    , m_movementSpeed(DEFAULT_SPEED)
    , m_isMoving(false)
    , m_isCalibrated(false)
    , m_totalCommands(0)
    , m_successCommands(0)
    , m_failedCommands(0)
    , m_startTime(QDateTime::currentDateTime())
{
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected, this, &TcpAntenna::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &TcpAntenna::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpAntenna::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &TcpAntenna::onError);

    m_movementTimer = new QTimer(this);
    m_movementTimer->setInterval(MOVEMENT_INTERVAL_MS);
    connect(m_movementTimer, &QTimer::timeout, this, &TcpAntenna::onMovementTimer);

    emit infoOccurred("Antenna instance created");
}

TcpAntenna::~TcpAntenna()
{
    stop();
    disconnectFromServer();
    delete m_socket;
    delete m_movementTimer;
    emit infoOccurred("Antenna destroyed");
}

// ===== Основные методы =====

void TcpAntenna::setAzimuth(double azimuth)
{
    QMutexLocker locker(&m_dataMutex);
    m_targetAzimuth = qBound(MIN_AZIMUTH, azimuth, MAX_AZIMUTH);
    m_isMoving = true;

    if (!m_movementTimer->isActive()) {
        m_movementTimer->start();
    }

    emit infoOccurred("Target azimuth set to: " + QString::number(m_targetAzimuth, 'f', 2));
    emit azimuthChanged(m_targetAzimuth);
}

void TcpAntenna::setElevation(double elevation)
{
    QMutexLocker locker(&m_dataMutex);
    m_targetElevation = qBound(MIN_ELEVATION, elevation, MAX_ELEVATION);
    m_isMoving = true;

    if (!m_movementTimer->isActive()) {
        m_movementTimer->start();
    }

    emit infoOccurred("Target elevation set to: " + QString::number(m_targetElevation, 'f', 2));
    emit elevationChanged(m_targetElevation);
}

void TcpAntenna::setSpeed(double speed)
{
    QMutexLocker locker(&m_dataMutex);
    if (speed <= 0) {
        emit errorOccurred("Speed must be greater than 0");
        return;
    }
    m_movementSpeed = speed;
    emit speedChanged(m_movementSpeed);
    emit infoOccurred("Speed set to: " + QString::number(m_movementSpeed, 'f', 2));
}

// ===== Команды =====

QByteArray TcpAntenna::execCMD(const QByteArray& request)
{
    m_totalCommands++;

    try {
        ParsedCMD parsed = parsingToCMD(request);

        if (!parsed.isValid()) {
            m_failedCommands++;
            emit warningOccurred("Invalid command received: " + QString(request));
            return createErrorResponse("Empty or invalid command");
        }

        QString cmd = parsed.name;
        QList<QByteArray> params = parsed.params;

        emit infoOccurred("Executing command: " + cmd + " with " +
            QString::number(params.size()) + " parameters");

        QByteArray response;

        if (cmd == "SET_AZIMUTH" || cmd == "SA") {
            response = cmdSetAzimuth(params);
        }
        else if (cmd == "SET_ELEVATION" || cmd == "SE") {
            response = cmdSetElevation(params);
        }
        else if (cmd == "SET_SPEED" || cmd == "SS") {
            response = cmdSetSpeed(params);
        }
        else if (cmd == "MOVE_AZIMUTH" || cmd == "MA") {
            response = cmdMoveAzimuth(params);
        }
        else if (cmd == "MOVE_ELEVATION" || cmd == "ME") {
            response = cmdMoveElevation(params);
        }
        else if (cmd == "CALIBRATE" || cmd == "CAL") {
            response = cmdCalibrate();
        }
        else if (cmd == "STATUS" || cmd == "ST") {
            response = cmdStatus();
        }
        else if (cmd == "STOP" || cmd == "S") {
            response = cmdStop();
        }
        else if (cmd == "HELP" || cmd == "H" || cmd == "?") {
            response = cmdHelp();
        }
        else {
            m_failedCommands++;
            emit warningOccurred("Unknown command: " + cmd);
            response = createErrorResponse("Unknown command: " + cmd);
        }

        if (!response.contains("ERROR")) {
            m_successCommands++;
            emit commandExecuted(cmd, true);
            emit infoOccurred("Command executed successfully: " + cmd);
        }
        else {
            m_failedCommands++;
            emit commandExecuted(cmd, false);
            emit warningOccurred("Command failed: " + cmd);
        }

        return response;

    }
    catch (const std::exception& e) {
        m_failedCommands++;
        QString error = "Exception in execCMD: " + QString(e.what());
        emit errorOccurred(error);
        return createErrorResponse(error);
    }
}

IAntenn::ParsedCMD TcpAntenna::parsingToCMD(const QByteArray& data)
{
    ParsedCMD result;
    QByteArray trimmed = data.trimmed();

    if (trimmed.isEmpty()) {
        return result;
    }

    int spacePos = trimmed.indexOf(' ');
    if (spacePos == -1) {
        result.name = QString(trimmed).toUpper();
        return result;
    }

    result.name = QString(trimmed.left(spacePos)).toUpper();
    QByteArray paramParts = trimmed.mid(spacePos + 1);

    for (const QByteArray& param : paramParts.split(',')) {
        if (!param.trimmed().isEmpty()) {
            result.params.append(param.trimmed());
        }
    }

    return result;
}

QByteArray TcpAntenna::getStatus()
{
    return cmdStatus();
}

// ===== Реализация команд =====

QByteArray TcpAntenna::cmdSetAzimuth(const QList<QByteArray>& params)
{
    if (!isCommandValid(ParsedCMD{ "SET_AZIMUTH", params }, 1)) {
        return createErrorResponse("SET_AZIMUTH requires 1 parameter: angle (0-360)");
    }

    double azimuth;
    if (!parseDouble(params[0], azimuth)) {
        return createErrorResponse("Invalid azimuth value: " + params[0]);
    }

    if (azimuth < MIN_AZIMUTH || azimuth > MAX_AZIMUTH) {
        return createErrorResponse(QString("Azimuth must be between %1 and %2")
            .arg(MIN_AZIMUTH).arg(MAX_AZIMUTH));
    }

    setAzimuth(azimuth);
    return createSuccessResponse(QString("Azimuth set to %1").arg(azimuth, 0, 'f', 2));
}

QByteArray TcpAntenna::cmdSetElevation(const QList<QByteArray>& params)
{
    if (!isCommandValid(ParsedCMD{ "SET_ELEVATION", params }, 1)) {
        return createErrorResponse("SET_ELEVATION requires 1 parameter: angle (-90 to 90)");
    }

    double elevation;
    if (!parseDouble(params[0], elevation)) {
        return createErrorResponse("Invalid elevation value: " + params[0]);
    }

    if (elevation < MIN_ELEVATION || elevation > MAX_ELEVATION) {
        return createErrorResponse(QString("Elevation must be between %1 and %2")
            .arg(MIN_ELEVATION).arg(MAX_ELEVATION));
    }

    setElevation(elevation);
    return createSuccessResponse(QString("Elevation set to %1").arg(elevation, 0, 'f', 2));
}

QByteArray TcpAntenna::cmdSetSpeed(const QList<QByteArray>& params)
{
    if (!isCommandValid(ParsedCMD{ "SET_SPEED", params }, 1)) {
        return createErrorResponse("SET_SPEED requires 1 parameter: speed (degrees/sec)");
    }

    double speed;
    if (!parseDouble(params[0], speed)) {
        return createErrorResponse("Invalid speed value: " + params[0]);
    }

    if (speed <= 0) {
        return createErrorResponse("Speed must be greater than 0");
    }

    setSpeed(speed);
    return createSuccessResponse(QString("Speed set to %1 deg/sec").arg(speed, 0, 'f', 2));
}

QByteArray TcpAntenna::cmdMoveAzimuth(const QList<QByteArray>& params)
{
    if (!isCommandValid(ParsedCMD{ "MOVE_AZIMUTH", params }, 1)) {
        return createErrorResponse("MOVE_AZIMUTH requires 1 parameter: delta angle");
    }

    double delta;
    if (!parseDouble(params[0], delta)) {
        return createErrorResponse("Invalid delta value: " + params[0]);
    }

    QMutexLocker locker(&m_dataMutex);
    double newAzimuth = m_targetAzimuth + delta;

    // Нормализуем азимут (0-360)
    newAzimuth = fmod(newAzimuth, MAX_AZIMUTH);
    if (newAzimuth < 0) {
        newAzimuth += MAX_AZIMUTH;
    }

    locker.unlock();
    setAzimuth(newAzimuth);

    return createSuccessResponse(QString("Moving azimuth by %1 to %2")
        .arg(delta, 0, 'f', 2)
        .arg(newAzimuth, 0, 'f', 2));
}

QByteArray TcpAntenna::cmdMoveElevation(const QList<QByteArray>& params)
{
    if (!isCommandValid(ParsedCMD{ "MOVE_ELEVATION", params }, 1)) {
        return createErrorResponse("MOVE_ELEVATION requires 1 parameter: delta angle");
    }

    double delta;
    if (!parseDouble(params[0], delta)) {
        return createErrorResponse("Invalid delta value: " + params[0]);
    }

    QMutexLocker locker(&m_dataMutex);
    double newElevation = m_targetElevation + delta;
    newElevation = qBound(MIN_ELEVATION, newElevation, MAX_ELEVATION);

    locker.unlock();
    setElevation(newElevation);

    return createSuccessResponse(QString("Moving elevation by %1 to %2")
        .arg(delta, 0, 'f', 2)
        .arg(newElevation, 0, 'f', 2));
}

QByteArray TcpAntenna::cmdCalibrate()
{
    QMutexLocker locker(&m_dataMutex);
    m_isCalibrated = true;
    m_azimuth = 0.0;
    m_elevation = 0.0;
    m_targetAzimuth = 0.0;
    m_targetElevation = 0.0;
    m_isMoving = false;
    m_movementTimer->stop();

    emit infoOccurred("Antenna calibrated to position (0, 0)");
    emit positionReached(0.0, 0.0);

    return createSuccessResponse("Antenna calibrated to position (0, 0)");
}

QByteArray TcpAntenna::cmdStatus()
{
    QMutexLocker locker(&m_dataMutex);

    QString status = QString(
        "=== ANTENNA STATUS ===\n"
        "Azimuth: %1\n"
        "Elevation: %2\n"
        "Target Azimuth: %3\n"
        "Target Elevation: %4\n"
        "Speed: %5 deg/sec\n"
        "Moving: %6\n"
        "Calibrated: %7\n"
        "Connected: %8\n"
        "Commands: %9 (Success: %10, Failed: %11)\n"
        "Uptime: %12 seconds\n"
        "====================="
    )
        .arg(m_azimuth, 0, 'f', 2)
        .arg(m_elevation, 0, 'f', 2)
        .arg(m_targetAzimuth, 0, 'f', 2)
        .arg(m_targetElevation, 0, 'f', 2)
        .arg(m_movementSpeed, 0, 'f', 2)
        .arg(m_isMoving ? "YES" : "NO")
        .arg(m_isCalibrated ? "YES" : "NO")
        .arg(m_isConnected ? "YES" : "NO")
        .arg(m_totalCommands)
        .arg(m_successCommands)
        .arg(m_failedCommands)
        .arg(m_startTime.secsTo(QDateTime::currentDateTime()));

    return status.toUtf8();
}

QByteArray TcpAntenna::cmdStop()
{
    QMutexLocker locker(&m_dataMutex);

    m_isMoving = false;
    m_movementTimer->stop();

    emit infoOccurred("Movement stopped at AZ=" + QString::number(m_azimuth, 'f', 2) +
        " EL=" + QString::number(m_elevation, 'f', 2));

    return createSuccessResponse(QString("Stopped at AZ=%1 EL=%2")
        .arg(m_azimuth, 0, 'f', 2)
        .arg(m_elevation, 0, 'f', 2));
}

QByteArray TcpAntenna::cmdHelp()
{
    QString help =
        "=== AVAILABLE COMMANDS ===\n"
        "SET_AZIMUTH <angle>\n"
        "SET_ELEVATION <angle>\n"
        "SET_SPEED <speed> (deg/sec)\n"
        "MOVE_AZIMUTH <delta delta>\n"
        "MOVE_ELEVATION <delta delta>\n"
        "CALIBRATE (0, 0)\n"
        "STATUS\n"
        "STOP\n"
        "HELP\n"
        "==========================";

    return help.toUtf8();
}

// ===== Вспомогательные методы =====

bool TcpAntenna::parseDouble(const QByteArray& data, double& value) const
{
    bool ok;
    value = data.toDouble(&ok);
    return ok;
}

bool TcpAntenna::isCommandValid(const ParsedCMD& cmd, int expectedParams) const
{
    if (cmd.name.isEmpty()) {
        return false;
    }
    return cmd.paramCount() == expectedParams;
}

QByteArray TcpAntenna::createErrorResponse(const QString& error) const
{
    return QString("ERROR: %1\n").arg(error).toUtf8();
}

QByteArray TcpAntenna::createSuccessResponse(const QString& message) const
{
    return QString("OK: %1\n").arg(message).toUtf8();
}

QString TcpAntenna::formatStatus() const
{
    return QString("AZ=%1 EL=%2 TAZ=%3 TEL=%4 MOV=%5 SPD=%6")
        .arg(m_azimuth, 0, 'f', 2)
        .arg(m_elevation, 0, 'f', 2)
        .arg(m_targetAzimuth, 0, 'f', 2)
        .arg(m_targetElevation, 0, 'f', 2)
        .arg(m_isMoving ? "Y" : "N")
        .arg(m_movementSpeed, 0, 'f', 2);
}

// ===== Движение антенны =====

void TcpAntenna::processMovement()
{
    QMutexLocker locker(&m_dataMutex);

    if (!m_isMoving) {
        return;
    }

    double deltaTime = MOVEMENT_INTERVAL_MS / 1000.0;
    double maxDelta = m_movementSpeed * deltaTime;

    bool azimuthReached = false;
    bool elevationReached = false;

    // Движение по азимуту
    double diffAzimuth = m_targetAzimuth - m_azimuth;

    // Учитываем круговое движение
    if (std::abs(diffAzimuth) > MAX_AZIMUTH / 2) {
        if (diffAzimuth > 0) {
            diffAzimuth -= MAX_AZIMUTH;
        }
        else {
            diffAzimuth += MAX_AZIMUTH;
        }
    }

    if (std::abs(diffAzimuth) > 0.01) {
        double step = qBound(-maxDelta, diffAzimuth, maxDelta);
        m_azimuth += step;

        // Нормализация
        if (m_azimuth >= MAX_AZIMUTH) {
            m_azimuth -= MAX_AZIMUTH;
        }
        else if (m_azimuth < 0) {
            m_azimuth += MAX_AZIMUTH;
        }
    }
    else {
        azimuthReached = true;
    }

    // Движение по углу места
    double diffElevation = m_targetElevation - m_elevation;
    if (std::abs(diffElevation) > 0.01) {
        double step = qBound(-maxDelta, diffElevation, maxDelta);
        m_elevation += step;
        m_elevation = qBound(MIN_ELEVATION, m_elevation, MAX_ELEVATION);
    }
    else {
        elevationReached = true;
    }

    // Проверка достижения цели
    if (azimuthReached && elevationReached) {
        m_isMoving = false;
        m_movementTimer->stop();
        emit infoOccurred("Position reached: AZ=" + QString::number(m_azimuth, 'f', 2) +
            " EL=" + QString::number(m_elevation, 'f', 2));
        emit positionReached(m_azimuth, m_elevation);
    }

    // Отправка обновлений
    emit azimuthChanged(m_azimuth);
    emit elevationChanged(m_elevation);
}

void TcpAntenna::onMovementTimer()
{
    processMovement();
}

// ===== Слоты сокета =====

void TcpAntenna::onReadyRead()
{
    if (!m_socket) {
        return;
    }

    QDataStream in(m_socket);
    in.setVersion(DATA_STREAM_VERSION);

    while (true) {
        if (m_nextBlockSize == 0) {
            if (m_socket->bytesAvailable() < static_cast<qint64>(sizeof(quint16))) {
                break;
            }

            in >> m_nextBlockSize;

            if (m_nextBlockSize <= 0 || m_nextBlockSize > MAX_MESSAGE_SIZE) {
                emit warningOccurred("Invalid message size: " + QString::number(m_nextBlockSize));
                m_nextBlockSize = 0;
                m_readBuffer.clear();
                m_socket->readAll();
                break;
            }
        }

        if (m_socket->bytesAvailable() < static_cast<qint64>(m_nextBlockSize)) {
            break;
        }

        m_readBuffer.resize(m_nextBlockSize);
        qint64 bytesRead = in.readRawData(m_readBuffer.data(), m_nextBlockSize);

        if (bytesRead != static_cast<qint64>(m_nextBlockSize)) {
            emit warningOccurred("Failed to read full message. Expected: " +
                QString::number(m_nextBlockSize) + " Got: " +
                QString::number(bytesRead));
            m_nextBlockSize = 0;
            m_readBuffer.clear();
            break;
        }

        QByteArray request = m_readBuffer;
        emit infoOccurred("Received command: " + QString(request));

        QByteArray response = execCMD(request);
        sendData(response);

        m_nextBlockSize = 0;
        m_readBuffer.clear();
    }
}

void TcpAntenna::onConnected()
{
    m_isConnected = true;
    m_startTime = QDateTime::currentDateTime();
    emit connected();
    emit infoOccurred("Connected to server at " + m_serverAddress + ":" +
        QString::number(m_serverPort));
    emit statusChanged("Connected");
}

void TcpAntenna::onDisconnected()
{
    m_isConnected = false;
    emit disconnected();
    emit warningOccurred("Disconnected from server");
    emit statusChanged("Disconnected");
}

void TcpAntenna::onError(QAbstractSocket::SocketError error)
{
    QString errorMsg = m_socket ? m_socket->errorString() : "Unknown error";
    emit errorOccurred("Socket error: " + errorMsg + " (code: " + QString::number(error) + ")");
}

// ===== Сетевые методы =====

bool TcpAntenna::connectToServer(const QString& address, quint16 port)
{
    if (m_isConnected) {
        emit warningOccurred("Already connected to " + m_serverAddress + ":" +
            QString::number(m_serverPort));
        return false;
    }

    m_serverAddress = address;
    m_serverPort = port;

    emit infoOccurred("Connecting to " + address + ":" + QString::number(port));
    m_socket->connectToHost(address, port);

    return true;
}

void TcpAntenna::disconnectFromServer()
{
    if (m_isConnected) {
        m_socket->disconnectFromHost();
        m_isConnected = false;
        emit infoOccurred("Disconnected from server");
    }
    else {
        emit warningOccurred("Already disconnected");
    }
}

bool TcpAntenna::isConnected() const
{
    return m_isConnected && m_socket &&
        m_socket->state() == QAbstractSocket::ConnectedState;
}

void TcpAntenna::sendData(const QByteArray& data)
{
    if (!m_isConnected || !m_socket) {
        emit errorOccurred("Cannot send data: not connected");
        return;
    }

    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setVersion(DATA_STREAM_VERSION);
    out << quint16(data.size());
    out.writeRawData(data.data(), data.size());

    qint64 written = m_socket->write(packet);
    if (written == -1) {
        emit errorOccurred("Failed to send data: " + m_socket->errorString());
    }
    else {
        m_socket->flush();
        emit infoOccurred("Sent " + QString::number(data.size()) + " bytes");
    }
}

// ===== Запуск/остановка =====

void TcpAntenna::start()
{
    if (m_isRunning.testAndSetOrdered(0, 1)) {
        emit infoOccurred("Antenna started");
        emit statusChanged("Running");
    }
    else {
        emit warningOccurred("Antenna already running");
    }
}

void TcpAntenna::stop()
{
    if (m_isRunning.testAndSetOrdered(1, 0)) {
        m_movementTimer->stop();
        m_isMoving = false;
        emit infoOccurred("Antenna stopped");
        emit statusChanged("Stopped");
    }
    else {
        emit warningOccurred("Antenna already stopped");
    }
}