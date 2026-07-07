#include "include/MyAntenn.h"
#include <cmath>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QTimerEvent>

const QString MyAntenn::DEFAULT_CONFIG_PATH = "config/antenna_config.ini";

MyAntenn::MyAntenn(QObject* parent)
    : IAntenn(parent)
    , m_azimuth(0.0)
    , m_elevation(0.0)
    , m_polarization(0.0)
    , m_targetAzimuth(0.0)
    , m_targetElevation(0.0)
    , m_targetPolarization(0.0)
    , m_currentSpeed(0.0)
    , m_speedMode(0)
    , m_minAzimuth(0.0)
    , m_maxAzimuth(360.0)
    , m_minElevation(-90.0)
    , m_maxElevation(90.0)
    , m_minPolarization(-180.0)
    , m_maxPolarization(180.0)
    , m_maxSpeed(50.0)
    , m_isMoving(false)
    , m_isRunning(false)
    , m_isCalibrated(false)
    , m_timerId(0)
    , m_timerActive(false)
    , m_settings(nullptr)
    , m_defaultAzimuth(0.0)
    , m_defaultElevation(0.0)
    , m_defaultPolarization(0.0)
    , m_defaultSpeedMode(0)
{
    // Загружаем конфигурацию
    if (!loadConfig(DEFAULT_CONFIG_PATH)) {
        loadDefaultConfig();
    }

    applyConfig();
    emit infoOccurred("MyAntenn instance created");
}

MyAntenn::~MyAntenn()
{
    stop();
    if (m_timerActive) {
        killTimer(m_timerId);
        m_timerActive = false;
    }
    if (m_settings) {
        delete m_settings;
        m_settings = nullptr;
    }
    emit infoOccurred("MyAntenn destroyed");
}

void MyAntenn::loadDefaultConfig()
{
    m_speedModes.clear();
    m_speedModes << 5.0 << 10.0 << 20.0 << 30.0 << 50.0;
    m_defaultAzimuth = 0.0;
    m_defaultElevation = 0.0;
    m_defaultPolarization = 0.0;
    m_configPath = DEFAULT_CONFIG_PATH;
    m_speedMode = 0;
    m_defaultSpeedMode = 0;
    m_currentSpeed = m_speedModes.value(0, 10.0);

    // Устанавливаем ограничения по умолчанию
    m_minAzimuth = 0.0;
    m_maxAzimuth = 360.0;
    m_minElevation = -90.0;
    m_maxElevation = 90.0;
    m_minPolarization = -180.0;
    m_maxPolarization = 180.0;
    m_maxSpeed = 50.0;

    if (!m_settings) {
        m_settings = new QSettings(m_configPath, QSettings::IniFormat, this);
    }
}

void MyAntenn::loadLimits()
{
    if (m_settings) {
        m_minAzimuth = m_settings->value("Limits/MinAzimuth", 0.0).toDouble();
        m_maxAzimuth = m_settings->value("Limits/MaxAzimuth", 360.0).toDouble();
        m_minElevation = m_settings->value("Limits/MinElevation", -90.0).toDouble();
        m_maxElevation = m_settings->value("Limits/MaxElevation", 90.0).toDouble();
        m_minPolarization = m_settings->value("Limits/MinPolarization", -180.0).toDouble();
        m_maxPolarization = m_settings->value("Limits/MaxPolarization", 180.0).toDouble();
        m_maxSpeed = m_settings->value("Limits/MaxSpeed", 50.0).toDouble();
    }
}

bool MyAntenn::loadConfig(const QString& configPath)
{
    if (!QFile::exists(configPath)) {
        emit warningOccurred("Config file not found: " + configPath);
        return false;
    }

    if (m_settings) {
        delete m_settings;
        m_settings = nullptr;
    }

    m_configPath = configPath;
    m_settings = new QSettings(configPath, QSettings::IniFormat, this);

    // Читаем настройки скоростей
    m_speedModes.clear();
    m_speedModeNames.clear();

    int modeCount = m_settings->value("SpeedModes/Count", 5).toInt();
    for (int i = 0; i < modeCount; ++i) {
        // Читаем скорость
        QString key = QString("SpeedModes/Mode%1").arg(i);
        double speed = m_settings->value(key, 10.0).toDouble();
        m_speedModes.append(speed);

        QString nameKey = QString("SpeedModes/Name%1").arg(i);
        QString name = m_settings->value(nameKey, QString("Mode %1").arg(i)).toString();
        m_speedModeNames.append(name);
    }

    if (m_speedModes.isEmpty()) {
        m_speedModes << 5.0 << 10.0 << 20.0 << 30.0 << 50.0;
        m_speedModeNames << "Slow" << "Normal" << "Fast" << "Very Fast" << "Extreme";
    }

    // Читаем настройки по умолчанию
    m_defaultAzimuth = m_settings->value("Default/Azimuth", 0.0).toDouble();
    m_defaultElevation = m_settings->value("Default/Elevation", 0.0).toDouble();
    m_defaultPolarization = m_settings->value("Default/Polarization", 0.0).toDouble();
    m_defaultSpeedMode = m_settings->value("Default/SpeedMode", 0).toInt();

    // Читаем ограничения
    loadLimits();

    emit infoOccurred("Config loaded from: " + configPath);
    return true;
}

bool MyAntenn::saveConfig(const QString& configPath)
{
    if (!m_settings) {
        m_settings = new QSettings(configPath, QSettings::IniFormat, this);
    }

    // Сохраняем режимы скоростей
    m_settings->setValue("SpeedModes/Count", m_speedModes.size());
    for (int i = 0; i < m_speedModes.size(); ++i) {
        m_settings->setValue(QString("SpeedModes/Mode%1").arg(i), m_speedModes[i]);
    }

    // Сохраняем значения по умолчанию
    m_settings->setValue("Default/Azimuth", m_defaultAzimuth);
    m_settings->setValue("Default/Elevation", m_defaultElevation);
    m_settings->setValue("Default/Polarization", m_defaultPolarization);
    m_settings->setValue("Default/SpeedMode", m_defaultSpeedMode);

    // Сохраняем ограничения
    m_settings->setValue("Limits/MinAzimuth", m_minAzimuth);
    m_settings->setValue("Limits/MaxAzimuth", m_maxAzimuth);
    m_settings->setValue("Limits/MinElevation", m_minElevation);
    m_settings->setValue("Limits/MaxElevation", m_maxElevation);
    m_settings->setValue("Limits/MinPolarization", m_minPolarization);
    m_settings->setValue("Limits/MaxPolarization", m_maxPolarization);
    m_settings->setValue("Limits/MaxSpeed", m_maxSpeed);

    m_settings->sync();

    emit infoOccurred("Config saved to: " + configPath);
    return true;
}

QString MyAntenn::getConfigPath() const
{
    return m_configPath;
}

void MyAntenn::applyConfig()
{
    QMutexLocker locker(&m_dataMutex);

    // Проверяем, что значения по умолчанию в пределах ограничений
    m_defaultAzimuth = qBound(m_minAzimuth, m_defaultAzimuth, m_maxAzimuth);
    m_defaultElevation = qBound(m_minElevation, m_defaultElevation, m_maxElevation);
    m_defaultPolarization = qBound(m_minPolarization, m_defaultPolarization, m_maxPolarization);

    // Устанавливаем текущие значения в default (без циклической нормализации)
    m_azimuth = m_defaultAzimuth;
    m_elevation = m_defaultElevation;
    m_polarization = m_defaultPolarization;
    m_targetAzimuth = m_defaultAzimuth;
    m_targetElevation = m_defaultElevation;
    m_targetPolarization = m_defaultPolarization;

    // Устанавливаем режим скорости
    if (m_defaultSpeedMode >= 0 && m_defaultSpeedMode < m_speedModes.size()) {
        m_speedMode = m_defaultSpeedMode;
        m_currentSpeed = m_speedModes[m_defaultSpeedMode];
    }
    else {
        m_speedMode = 0;
        m_currentSpeed = m_speedModes.value(0, 10.0);
    }

    // Ограничиваем скорость сверху
    m_currentSpeed = qBound(0.0, m_currentSpeed, m_maxSpeed);

    m_isMoving = false;
    m_isCalibrated = true;
}

bool MyAntenn::setAzimuth(double azimuth)
{
    QMutexLocker locker(&m_dataMutex);

    if (!m_isRunning) {
        emit warningOccurred("Cannot set azimuth: antenna is stopped");
        return false;
    }

    if (azimuth < m_minAzimuth || azimuth > m_maxAzimuth) {
        emit errorOccurred("Azimuth " + QString::number(azimuth) +
            " is out of range [" + QString::number(m_minAzimuth) +
            ", " + QString::number(m_maxAzimuth) + "]");
        return false;
    }

    m_targetAzimuth = azimuth;
    m_isMoving = true;

    if (!m_timerActive && m_isRunning) {
        m_timerId = startTimer(MOVEMENT_INTERVAL_MS);
        m_timerActive = true;
    }

    emit movementStarted();
    emit infoOccurred("Target azimuth set to: " + QString::number(m_targetAzimuth, 'f', 2));
    return true;
}

bool MyAntenn::setElevation(double elevation)
{
    QMutexLocker locker(&m_dataMutex);

    if (!m_isRunning) {
        emit warningOccurred("Cannot set elevation: antenna is stopped");
        return false;
    }

    // Проверяем, что угол места в пределах допустимых значений
    if (elevation < m_minElevation || elevation > m_maxElevation) {
        emit errorOccurred("Elevation " + QString::number(elevation) +
            " is out of range [" + QString::number(m_minElevation) +
            ", " + QString::number(m_maxElevation) + "]");
        return false;
    }

    m_targetElevation = elevation;
    m_isMoving = true;

    if (!m_timerActive && m_isRunning) {
        m_timerId = startTimer(MOVEMENT_INTERVAL_MS);
        m_timerActive = true;
    }

    emit movementStarted();
    emit infoOccurred("Target elevation set to: " + QString::number(m_targetElevation, 'f', 2));
    return true;
}

bool MyAntenn::setPolarization(double polarization)
{
    QMutexLocker locker(&m_dataMutex);

    if (!m_isRunning) {
        emit warningOccurred("Cannot set polarization: antenna is stopped");
        return false;
    }

    // Проверяем, что поляризация в пределах допустимых значений
    if (polarization < m_minPolarization || polarization > m_maxPolarization) {
        emit errorOccurred("Polarization " + QString::number(polarization) +
            " is out of range [" + QString::number(m_minPolarization) +
            ", " + QString::number(m_maxPolarization) + "]");
        return false;
    }

    m_targetPolarization = polarization;
    m_isMoving = true;

    if (!m_timerActive && m_isRunning) {
        m_timerId = startTimer(MOVEMENT_INTERVAL_MS);
        m_timerActive = true;
    }

    emit movementStarted();
    emit infoOccurred("Target polarization set to: " + QString::number(m_targetPolarization, 'f', 2));
    return true;
}

bool MyAntenn::setSpeedMode(int mode)
{
    QMutexLocker locker(&m_dataMutex);

    if (mode < 0 || mode >= m_speedModes.size()) {
        emit errorOccurred("Invalid speed mode: " + QString::number(mode) +
            ". Available modes: 0-" + QString::number(m_speedModes.size() - 1));
        return false;
    }

    m_speedMode = mode;
    m_currentSpeed = qMin(m_speedModes[mode], m_maxSpeed);

    emit infoOccurred("Speed mode set to: " + QString::number(mode) +
        " (" + QString::number(m_currentSpeed, 'f', 1) + " deg/sec)");
    return true;
}

void MyAntenn::calibrate()
{
    QMutexLocker locker(&m_dataMutex);

    m_azimuth = m_defaultAzimuth;
    m_elevation = m_defaultElevation;
    m_polarization = m_defaultPolarization;
    m_targetAzimuth = m_defaultAzimuth;
    m_targetElevation = m_defaultElevation;
    m_targetPolarization = m_defaultPolarization;
    m_isMoving = false;
    m_isCalibrated = true;

    if (m_timerActive) {
        killTimer(m_timerId);
        m_timerActive = false;
    }

    emit calibrated();
    emit infoOccurred("Antenna calibrated to default position");
}


void MyAntenn::processMovement()
{
    QMutexLocker locker(&m_dataMutex);

    if (!m_isMoving || !m_isRunning) {
        return;
    }

    double deltaTime = MOVEMENT_INTERVAL_MS / 1000.0;
    double maxDelta = m_currentSpeed * deltaTime;

    bool azimuthReached = false;
    bool elevationReached = false;
    bool polarizationReached = false;

    // ===== Движение по азимуту (БЕЗ оптимизации кратчайшего пути) =====
    double diffAzimuth = m_targetAzimuth - m_azimuth;

    if (std::abs(diffAzimuth) > 0.001) {
        double step = qBound(-maxDelta, diffAzimuth, maxDelta);
        m_azimuth += step;

        // Нормализация в пределах [minAzimuth, maxAzimuth]
        m_azimuth = qBound(m_minAzimuth, m_azimuth, m_maxAzimuth);
    }
    else {
        m_azimuth = m_targetAzimuth;
        azimuthReached = true;
    }

    // ===== Движение по углу места =====
    double diffElevation = m_targetElevation - m_elevation;
    if (std::abs(diffElevation) > 0.001) {
        double step = qBound(-maxDelta, diffElevation, maxDelta);
        m_elevation += step;
        m_elevation = qBound(m_minElevation, m_elevation, m_maxElevation);
    }
    else {
        m_elevation = m_targetElevation;
        elevationReached = true;
    }

    // ===== Движение по поляризации (БЕЗ оптимизации) =====
    double diffPolarization = m_targetPolarization - m_polarization;
    if (std::abs(diffPolarization) > 0.001) {
        double step = qBound(-maxDelta, diffPolarization, maxDelta);
        m_polarization += step;
        m_polarization = qBound(m_minPolarization, m_polarization, m_maxPolarization);
    }
    else {
        m_polarization = m_targetPolarization;
        polarizationReached = true;
    }

    // Проверка достижения цели
    if (azimuthReached && elevationReached && polarizationReached) {
        m_isMoving = false;
        if (m_timerActive) {
            killTimer(m_timerId);
            m_timerActive = false;
        }
        emit movementStopped();
        emit infoOccurred("Position reached: AZ=" + QString::number(m_azimuth, 'f', 2) +
            " EL=" + QString::number(m_elevation, 'f', 2) +
            " POL=" + QString::number(m_polarization, 'f', 2));
    }
}

void MyAntenn::timerEvent(QTimerEvent* event)
{
    Q_UNUSED(event)
        processMovement();
}

bool MyAntenn::start()
{
    if (m_isRunning.testAndSetOrdered(0, 1)) {
        m_startTime = QDateTime::currentDateTime();
        emit infoOccurred("MyAntenn started");
        return true;
    }
    else {
        emit warningOccurred("MyAntenn already running");
        return false;
    }
}

bool MyAntenn::stop()
{
    if (m_isRunning.testAndSetOrdered(1, 0)) {
        if (m_timerActive) {
            killTimer(m_timerId);
            m_timerActive = false;
        }
        m_isMoving = false;
        emit movementStopped();
        emit infoOccurred("MyAntenn stopped");
        return true;
    }
    else {
        emit warningOccurred("MyAntenn already stopped");
        return false;
    }
}

bool MyAntenn::isRunning() const
{
    return m_isRunning;
}

AntennaStatus MyAntenn::getStatus() const
{
    QMutexLocker locker(&m_dataMutex);

    AntennaStatus status;
    status.azimuth = m_azimuth;
    status.elevation = m_elevation;
    status.polarization = m_polarization;
    status.targetAzimuth = m_targetAzimuth;
    status.targetElevation = m_targetElevation;
    status.targetPolarization = m_targetPolarization;
    status.speed = m_currentSpeed;
    status.speedMode = m_speedMode;
    status.isMoving = m_isMoving;
    status.isCalibrated = m_isCalibrated;
    status.isRunning = m_isRunning;
    status.uptimeSeconds = m_startTime.secsTo(QDateTime::currentDateTime());

    return status;
}

AntennaConfig MyAntenn::getConfig() const
{
    QMutexLocker locker(&m_dataMutex);

    AntennaConfig config;

    // Лимиты
    config.limits.minAzimuth = m_minAzimuth;
    config.limits.maxAzimuth = m_maxAzimuth;
    config.limits.minElevation = m_minElevation;
    config.limits.maxElevation = m_maxElevation;
    config.limits.minPolarization = m_minPolarization;
    config.limits.maxPolarization = m_maxPolarization;
    config.limits.maxSpeed = m_maxSpeed;

    // Режимы скоростей (с именами)
    config.speedModes.clear();
    for (int i = 0; i < m_speedModes.size(); ++i) {
        QString name = (i < m_speedModeNames.size()) ?
            m_speedModeNames[i] :
            QString("Mode %1").arg(i);
        config.speedModes.append(SpeedMode(i, name, m_speedModes[i]));
    }

    // Значения по умолчанию
    config.defaultAzimuth = m_defaultAzimuth;
    config.defaultElevation = m_defaultElevation;
    config.defaultPolarization = m_defaultPolarization;
    config.defaultSpeedMode = m_defaultSpeedMode;

    return config;
}
