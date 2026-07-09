#include "AntennaClient.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QHostAddress>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QFont>
#include <QTimerEvent>

AntennaClient::AntennaClient(QWidget* parent)
    : QWidget(parent)
    , m_socket(nullptr)
    , m_host("127.0.0.1")
    , m_port(12345)
    , m_isConnected(false)
    , m_timerId(0)
    , m_timerActive(false)
    , m_settings(nullptr)
{
    setupUI();

    m_socket = new QTcpSocket(this);
    m_settings = new QSettings("AntennaEmulator", "AntennaClient", this);

    // Подключаем сигналы сокета
    connect(m_socket, &QTcpSocket::disconnected, this, &AntennaClient::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &AntennaClient::onSocketReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &AntennaClient::onSocketError);
    connect(m_socket, &QTcpSocket::connected, this, &AntennaClient::onSocketConnected);

    // Подключаем сигналы кнопок
    connect(m_connectButton, &QPushButton::clicked, this, &AntennaClient::onConnectButtonClicked);
    connect(m_disconnectButton, &QPushButton::clicked, this, &AntennaClient::onDisconnectButtonClicked);

    connect(m_calibrateButton, &QPushButton::clicked, this, &AntennaClient::onCalibrateButtonClicked);
    connect(m_startButton, &QPushButton::clicked, this, &AntennaClient::onStartButtonClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &AntennaClient::onStopButtonClicked);

    connect(m_speedModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &AntennaClient::onSpeedModeChanged);

    connect(m_azimuthControlSlider, &QSlider::valueChanged, this, &AntennaClient::onAzimuthSliderChanged);
    connect(m_elevationControlSlider, &QSlider::valueChanged, this, &AntennaClient::onElevationSliderChanged);
    connect(m_polarizationControlSlider, &QSlider::valueChanged, this, &AntennaClient::onPolarizationSliderChanged);

    connect(m_applyPositionButton, &QPushButton::clicked, this, &AntennaClient::onApplyPositionButtonClicked);

    connect(m_antennaTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &AntennaClient::onAntennaTypeSelected);

    // Загружаем настройки
    loadSettings();

    // Устанавливаем стиль
    setWindowTitle("Antenna Emulator Client");
    resize(900, 700);

    // Инициализация статуса
    updateAntennaStatusDisplay(0, 0, 0, 0, 0, false, false);
    updateUIState();
}

AntennaClient::~AntennaClient()
{
    saveSettings();
    if (m_timerActive) {
        killTimer(m_timerId);
        m_timerActive = false;
    }
    if (m_socket) {
        m_socket->disconnectFromHost();
        m_socket->deleteLater();
    }
}

void AntennaClient::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == m_timerId) {
        requestStatus();
    }
}

void AntennaClient::setupUI()
{
    // Главный вертикальный layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    // ===== 1. Верхняя панель: подключение =====
    QHBoxLayout* connectionLayout = new QHBoxLayout();

    m_hostEdit = new QLineEdit("127.0.0.1");
    m_hostEdit->setFixedWidth(120);
    m_portEdit = new QLineEdit("12345");
    m_portEdit->setFixedWidth(70);

    m_connectButton = new QPushButton("Connect");
    m_connectButton->setFixedWidth(100);
    m_disconnectButton = new QPushButton("Disconnect");
    m_disconnectButton->setFixedWidth(100);
    m_disconnectButton->setEnabled(false);

    m_connectionStatusLabel = new QLabel("DISCONNECTED");
    m_connectionStatusLabel->setStyleSheet("background-color: #f44336; color: white; padding: 5px; border-radius: 3px;");
    m_connectionStatusLabel->setFixedWidth(120);
    m_connectionStatusLabel->setAlignment(Qt::AlignCenter);

    connectionLayout->addWidget(new QLabel("Host:"));
    connectionLayout->addWidget(m_hostEdit);
    connectionLayout->addWidget(new QLabel("Port:"));
    connectionLayout->addWidget(m_portEdit);
    connectionLayout->addSpacing(10);
    connectionLayout->addWidget(m_connectButton);
    connectionLayout->addWidget(m_disconnectButton);
    connectionLayout->addSpacing(10);
    connectionLayout->addWidget(m_connectionStatusLabel);
    connectionLayout->addStretch();

    mainLayout->addLayout(connectionLayout);

    // ===== 2. Выбор типа антенны =====
    QHBoxLayout* antennaTypeLayout = new QHBoxLayout();
    antennaTypeLayout->addWidget(new QLabel("Antenna Type:"));
    m_antennaTypeCombo = new QComboBox();
    m_antennaTypeCombo->setFixedWidth(200);
    m_antennaTypeCombo->addItem("Default");
    antennaTypeLayout->addWidget(m_antennaTypeCombo);
    antennaTypeLayout->addStretch();
    mainLayout->addLayout(antennaTypeLayout);

    // ===== 3. Основная панель: управление и статус =====
    QHBoxLayout* mainPanelLayout = new QHBoxLayout();
    mainPanelLayout->setSpacing(15);

    // ===== 3.1 Левая панель: Управление антенной =====
    QGroupBox* controlGroup = new QGroupBox("Antenna Control");
    QVBoxLayout* controlLayout = new QVBoxLayout(controlGroup);
    controlLayout->setSpacing(8);

    // Слайдеры управления
    QFormLayout* slidersLayout = new QFormLayout();
    slidersLayout->setSpacing(5);

    // Азимут
    QHBoxLayout* azimuthControlLayout = new QHBoxLayout();
    m_azimuthControlSlider = new QSlider(Qt::Horizontal);
    m_azimuthControlSlider->setRange(0, 360);
    m_azimuthControlSlider->setValue(0);
    m_azimuthControlSlider->setFixedWidth(180);
    m_azimuthControlLabel = new QLabel("0 deg");
    m_azimuthControlLabel->setFixedWidth(45);
    azimuthControlLayout->addWidget(m_azimuthControlSlider);
    azimuthControlLayout->addWidget(m_azimuthControlLabel);
    slidersLayout->addRow("Azimuth:", azimuthControlLayout);

    // Угол места
    QHBoxLayout* elevationControlLayout = new QHBoxLayout();
    m_elevationControlSlider = new QSlider(Qt::Horizontal);
    m_elevationControlSlider->setRange(-90, 90);
    m_elevationControlSlider->setValue(0);
    m_elevationControlSlider->setFixedWidth(180);
    m_elevationControlLabel = new QLabel("0 deg");
    m_elevationControlLabel->setFixedWidth(45);
    elevationControlLayout->addWidget(m_elevationControlSlider);
    elevationControlLayout->addWidget(m_elevationControlLabel);
    slidersLayout->addRow("Elevation:", elevationControlLayout);

    // Поляризация
    QHBoxLayout* polarizationControlLayout = new QHBoxLayout();
    m_polarizationControlSlider = new QSlider(Qt::Horizontal);
    m_polarizationControlSlider->setRange(-180, 180);
    m_polarizationControlSlider->setValue(0);
    m_polarizationControlSlider->setFixedWidth(180);
    m_polarizationControlLabel = new QLabel("0 deg");
    m_polarizationControlLabel->setFixedWidth(45);
    polarizationControlLayout->addWidget(m_polarizationControlSlider);
    polarizationControlLayout->addWidget(m_polarizationControlLabel);
    slidersLayout->addRow("Polarization:", polarizationControlLayout);

    controlLayout->addLayout(slidersLayout);

    // Кнопка применения позиции
    QHBoxLayout* applyLayout = new QHBoxLayout();
    m_applyPositionButton = new QPushButton("Apply Position");
    m_applyPositionButton->setFixedWidth(150);
    m_applyPositionButton->setStyleSheet("font-weight: bold; background-color: #4CAF50; color: white;");
    applyLayout->addWidget(m_applyPositionButton);
    applyLayout->addStretch();
    controlLayout->addLayout(applyLayout);

    // Режим скорости
    QHBoxLayout* speedLayout = new QHBoxLayout();
    speedLayout->addWidget(new QLabel("Speed Mode:"));
    m_speedModeCombo = new QComboBox();
    m_speedModeCombo->setFixedWidth(150);
    speedLayout->addWidget(m_speedModeCombo);
    speedLayout->addStretch();
    controlLayout->addLayout(speedLayout);

    // Кнопки управления
    QHBoxLayout* controlButtonsLayout = new QHBoxLayout();
    m_calibrateButton = new QPushButton("Reset");
    m_startButton = new QPushButton("Start");
    m_stopButton = new QPushButton("Stop");
    m_calibrateButton->setFixedWidth(90);
    m_startButton->setFixedWidth(90);
    m_stopButton->setFixedWidth(90);
    controlButtonsLayout->addWidget(m_calibrateButton);
    controlButtonsLayout->addWidget(m_startButton);
    controlButtonsLayout->addWidget(m_stopButton);
    controlButtonsLayout->addStretch();
    controlLayout->addLayout(controlButtonsLayout);

    controlLayout->addStretch();
    mainPanelLayout->addWidget(controlGroup);

    // ===== 3.2 Правая панель: Статус антенны =====
    QGroupBox* statusGroup = new QGroupBox("Antenna Status");
    QGridLayout* statusLayout = new QGridLayout(statusGroup);
    statusLayout->setSpacing(8);
    statusLayout->setColumnStretch(1, 1);

    // Заголовки
    statusLayout->addWidget(new QLabel("<b>Parameter</b>"), 0, 0);
    statusLayout->addWidget(new QLabel("<b>Value</b>"), 0, 1);

    // Азимут
    QVBoxLayout* azimuthLayout = new QVBoxLayout();
    azimuthLayout->setSpacing(2);
    m_azimuthStatusLabel = new QLabel("0.00 deg");
    m_azimuthStatusLabel->setStyleSheet("font-weight: bold; color: #2196F3;");
    m_azimuthLimitsLabel = new QLabel("Range: 0 deg - 360 deg");
    m_azimuthLimitsLabel->setStyleSheet("color: #757575; font-size: 9pt;");
    azimuthLayout->addWidget(m_azimuthStatusLabel);
    azimuthLayout->addWidget(m_azimuthLimitsLabel);
    statusLayout->addWidget(new QLabel("Azimuth:"), 1, 0);
    statusLayout->addLayout(azimuthLayout, 1, 1);

    // Угол места
    QVBoxLayout* elevationLayout = new QVBoxLayout();
    elevationLayout->setSpacing(2);
    m_elevationStatusLabel = new QLabel("0.00 deg");
    m_elevationStatusLabel->setStyleSheet("font-weight: bold; color: #4CAF50;");
    m_elevationLimitsLabel = new QLabel("Range: -90 deg - 90 deg");
    m_elevationLimitsLabel->setStyleSheet("color: #757575; font-size: 9pt;");
    elevationLayout->addWidget(m_elevationStatusLabel);
    elevationLayout->addWidget(m_elevationLimitsLabel);
    statusLayout->addWidget(new QLabel("Elevation:"), 2, 0);
    statusLayout->addLayout(elevationLayout, 2, 1);

    // Поляризация
    QVBoxLayout* polarizationLayout = new QVBoxLayout();
    polarizationLayout->setSpacing(2);
    m_polarizationStatusLabel = new QLabel("0.00 deg");
    m_polarizationStatusLabel->setStyleSheet("font-weight: bold; color: #FF9800;");
    m_polarizationLimitsLabel = new QLabel("Range: -180 deg - 180 deg");
    m_polarizationLimitsLabel->setStyleSheet("color: #757575; font-size: 9pt;");
    polarizationLayout->addWidget(m_polarizationStatusLabel);
    polarizationLayout->addWidget(m_polarizationLimitsLabel);
    statusLayout->addWidget(new QLabel("Polarization:"), 3, 0);
    statusLayout->addLayout(polarizationLayout, 3, 1);

    // Разделитель
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    statusLayout->addWidget(line, 4, 0, 1, 2);

    // Скорость
    QVBoxLayout* speedLayout2 = new QVBoxLayout();
    speedLayout2->setSpacing(2);
    m_speedStatusLabel = new QLabel("0.00 deg/s");
    m_speedStatusLabel->setStyleSheet("font-weight: bold;");
    m_speedLimitsLabel = new QLabel("Max: 50.0 deg/s");
    m_speedLimitsLabel->setStyleSheet("color: #757575; font-size: 9pt;");
    speedLayout2->addWidget(m_speedStatusLabel);
    statusLayout->addWidget(new QLabel("Speed:"), 5, 0);
    statusLayout->addLayout(speedLayout2, 5, 1);

    // Режим скорости
    m_speedModeStatusLabel = new QLabel("0 (Slow)");
    m_speedModeStatusLabel->setStyleSheet("font-weight: bold;");
    statusLayout->addWidget(new QLabel("Speed Mode:"), 6, 0);
    statusLayout->addWidget(m_speedModeStatusLabel, 6, 1);

    // Движение
    m_movingStatusLabel = new QLabel("NO");
    m_movingStatusLabel->setStyleSheet("font-weight: bold; color: #757575;");
    statusLayout->addWidget(new QLabel("Moving:"), 7, 0);
    statusLayout->addWidget(m_movingStatusLabel, 7, 1);

    // Калибровка
    m_calibratedStatusLabel = new QLabel("YES");
    m_calibratedStatusLabel->setStyleSheet("font-weight: bold; color: #4CAF50;");
    statusLayout->addWidget(new QLabel("Calibrated:"), 8, 0);
    statusLayout->addWidget(m_calibratedStatusLabel, 8, 1);

    // Запущена
    m_runningStatusLabel = new QLabel("STOPPED");
    m_runningStatusLabel->setStyleSheet("font-weight: bold; color: #f44336;");
    statusLayout->addWidget(new QLabel("Running:"), 9, 0);
    statusLayout->addWidget(m_runningStatusLabel, 9, 1);

    // Время работы
    m_uptimeStatusLabel = new QLabel("0 seconds");
    m_uptimeStatusLabel->setStyleSheet("font-weight: bold;");
    statusLayout->addWidget(new QLabel("Uptime:"), 10, 0);
    statusLayout->addWidget(m_uptimeStatusLabel, 10, 1);

    statusLayout->setRowStretch(11, 1);
    mainPanelLayout->addWidget(statusGroup);

    mainLayout->addLayout(mainPanelLayout);

    // ===== 4. Статусная строка =====
    m_statusBar = new QStatusBar();
    m_statusBar->setMaximumHeight(25);
    m_connectionInfoLabel = new QLabel("Not connected");
    m_statusBar->addWidget(m_connectionInfoLabel);
    mainLayout->addWidget(m_statusBar);

    // Отключаем элементы управления до подключения
    updateUIState();
}

void AntennaClient::updateAntennaStatusDisplay(double azimuth, double elevation, double polarization,
    double speed, int speedMode, bool isMoving, bool isCalibrated)
{
    m_azimuthStatusLabel->setText(QString("%1 deg").arg(azimuth, 0, 'f', 2));
    m_elevationStatusLabel->setText(QString("%1 deg").arg(elevation, 0, 'f', 2));
    m_polarizationStatusLabel->setText(QString("%1 deg").arg(polarization, 0, 'f', 2));
    m_speedStatusLabel->setText(QString("%1 deg/s").arg(speed, 0, 'f', 2));

    // Режим скорости
    QString modeText = m_speedModeCombo->currentText();
    if (speedMode >= 0 && speedMode < m_speedModeCombo->count()) {
        modeText = m_speedModeCombo->itemText(speedMode);
    }
    m_speedModeStatusLabel->setText(QString("%1: %2").arg(speedMode).arg(modeText));

    // Движение
    m_movingStatusLabel->setText(isMoving ? "YES" : "NO");
    m_movingStatusLabel->setStyleSheet(isMoving ?
        "font-weight: bold; color: #FF9800;" :
        "font-weight: bold; color: #757575;");

    // Калибровка
    m_calibratedStatusLabel->setText(isCalibrated ? "YES" : "NO");
    m_calibratedStatusLabel->setStyleSheet(isCalibrated ?
        "font-weight: bold; color: #4CAF50;" :
        "font-weight: bold; color: #f44336;");
}

void AntennaClient::loadSettings()
{
    if (m_settings) {
        m_host = m_settings->value("Connection/Host", "127.0.0.1").toString();
        m_port = m_settings->value("Connection/Port", 12345).toUInt();

        m_hostEdit->setText(m_host);
        m_portEdit->setText(QString::number(m_port));
    }
}

void AntennaClient::saveSettings()
{
    if (m_settings) {
        m_settings->setValue("Connection/Host", m_hostEdit->text());
        m_settings->setValue("Connection/Port", m_portEdit->text().toUInt());
        m_settings->sync();
    }
}

void AntennaClient::onConnectButtonClicked()
{
    if (m_isConnected) {
        return;
    }

    m_host = m_hostEdit->text();
    m_port = m_portEdit->text().toUInt();

    if (m_host.isEmpty() || m_port == 0) {
        QMessageBox::warning(this, "Error", "Invalid host or port");
        return;
    }

    m_statusBar->showMessage("Connecting to " + m_host + ":" + QString::number(m_port) + "...", 3000);
    m_socket->connectToHost(m_host, m_port);
}

void AntennaClient::onDisconnectButtonClicked()
{
    if (m_isConnected) {
        // Останавливаем встроенный таймер
        if (m_timerActive) {
            killTimer(m_timerId);
            m_timerActive = false;
        }
        m_socket->disconnectFromHost();
    }
}

void AntennaClient::onSocketConnected()
{
    m_isConnected = true;
    updateConnectionStatus(true);
    updateUIState();

    m_connectionInfoLabel->setText("Connected to " + m_host + ":" + QString::number(m_port));
    m_statusBar->showMessage("Connected to server", 2000);

    // Запрашиваем список типов антенн
    sendCommand("GET_ANTENNA_TYPES");

    // Запускаем встроенный таймер для обновления статуса
    if (!m_timerActive) {
        m_timerId = startTimer(STATUS_UPDATE_INTERVAL_MS);
        m_timerActive = true;
    }
}

void AntennaClient::onSocketDisconnected()
{
    m_isConnected = false;

    // Останавливаем встроенный таймер
    if (m_timerActive) {
        killTimer(m_timerId);
        m_timerActive = false;
    }

    updateConnectionStatus(false);
    updateUIState();

    m_connectionInfoLabel->setText("Not connected");
    m_statusBar->showMessage("Disconnected from server", 2000);
}

void AntennaClient::onSocketError(QAbstractSocket::SocketError error)
{
    QString errorMsg = m_socket->errorString();
    m_statusBar->showMessage("Socket error: " + errorMsg, 5000);
    QMessageBox::warning(this, "Socket Error", errorMsg);
}

void AntennaClient::onSocketReadyRead()
{
    QDataStream in(m_socket);
    in.setVersion(DATA_STREAM_VERSION);

    while (m_socket->bytesAvailable() > 0) {
        try {
            if (m_buffer.nextBlockSize == 0) {
                if (m_socket->bytesAvailable() < static_cast<qint64>(sizeof(quint16))) {
                    break;
                }

                in >> m_buffer.nextBlockSize;

                if (m_buffer.nextBlockSize <= 0 || m_buffer.nextBlockSize > MAX_MESSAGE_SIZE) {
                    m_buffer.reset();
                    m_socket->readAll();
                    break;
                }
            }

            if (m_socket->bytesAvailable() < static_cast<qint64>(m_buffer.nextBlockSize)) {
                break;
            }

            m_buffer.rawData.resize(m_buffer.nextBlockSize);
            qint64 bytesRead = in.readRawData(m_buffer.rawData.data(), m_buffer.nextBlockSize);

            if (bytesRead != static_cast<qint64>(m_buffer.nextBlockSize)) {
                m_buffer.reset();
                m_socket->readAll();
                break;
            }

            processResponse(m_buffer.rawData);
            m_buffer.reset();

        }
        catch (const std::exception& e) {
            m_buffer.reset();
            m_socket->readAll();
            break;
        }
    }
}

void AntennaClient::sendData(const QByteArray& data)
{
    if (!m_isConnected || !m_socket) {
        QMessageBox::warning(this, "Error", "Not connected to server");
        return;
    }

    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setVersion(DATA_STREAM_VERSION);
    out << quint16(data.size());
    out.writeRawData(data.data(), data.size());

    qint64 written = m_socket->write(packet);
    if (written == -1) {
        m_statusBar->showMessage("Failed to send data", 2000);
    }
    else {
        m_socket->flush();
    }
}

void AntennaClient::sendCommand(const QString& command)
{
    sendData(command.toUtf8());
}

void AntennaClient::processResponse(const QByteArray& data)
{
    QString response = QString::fromUtf8(data);
    QString trimmed = response.trimmed();

    qDebug() << "AntennaClient::processResponse : response -> " << response;

    // Проверяем на наличие списка типов антенн
    if (response.contains("ANTENNA TYPES")) {
        parseAntennaTypesResponse(response);
        return;
    }

    // Проверяем на статус антенны
    if (response.contains("=== ANTENNA STATUS ===")) {
        parseAntennStatusResponse(response);
        return;
    }

    // Обработка ошибок
    if (response.contains("ERROR:")) {
        QString errorMsg = trimmed;
        if (errorMsg.startsWith("ERROR: ")) {
            errorMsg = errorMsg.mid(7);
        }
        m_statusBar->showMessage("Error: " + errorMsg, 3000);
        return;
    }

    // Успешные операции
    if (response.contains("OK:")) {
        m_statusBar->showMessage("Success: " + trimmed, 2000);
        return;
    }
}

void AntennaClient::parseAntennaTypesResponse(const QString& response)
{
    QRegularExpression regex;
    QRegularExpressionMatch match;
    qDebug() << "AntennaClient::parseAntennaTypesResponse : response -> " << response;
    // Ищем строку с типами
    regex.setPattern("Types:\\s*(\\d+)\\s*:\\s*(.+)");
    match = regex.match(response);

    if (match.hasMatch()) {
        int count = match.captured(1).toInt();
        QString typesStr = match.captured(2);

        qDebug() << "Raw types string:" << typesStr;
        qDebug() << "Count:" << count;

        // Разделяем по запятой
        QStringList types = typesStr.split(',', Qt::SkipEmptyParts);

        // Очищаем каждый тип от пробелов и спецсимволов
        for (int i = 0; i < types.size(); ++i) {
            types[i] = types[i].trimmed();
            // Удаляем возможные точки в конце (если это \n или \r)
            types[i].remove(QRegularExpression("[.\\r\\n]+$"));
        }

        qDebug() << "Parsed types:" << types;

        // Очищаем и заполняем комбобокс
        m_antennaTypeCombo->blockSignals(true);
        m_antennaTypeCombo->clear();

        if (types.isEmpty() || count == 0) {
            m_antennaTypeCombo->addItem("Default");
        }
        else {
            for (const QString& type : types) {
                if (!type.isEmpty()) {
                    m_antennaTypeCombo->addItem(type);
                }
            }
        }

        m_antennaTypeCombo->blockSignals(false);
    }
    else {
        qDebug() << "No match found for antenna types pattern";
        qDebug() << "Response:" << response;
    }
}

void AntennaClient::parseAntennStatusResponse(const QString& response)
{
    QRegularExpression regex;
    QRegularExpressionMatch match;

    QString antennaType;
    double azimuth = 0, elevation = 0, polarization = 0;
    double speed = 0;
    int speedMode = 0;
    bool isMoving = false;
    bool isCalibrated = false;
    bool isRunning = false;
    qint64 uptime = 0;

    regex.setPattern("Antenna Type:\\s*(.+)");
    match = regex.match(response);
    if (match.hasMatch()) antennaType = match.captured(1).trimmed();

    // Паттерн для чисел с плавающей точкой (включая отрицательные)
    QString numPattern = "-?\\d+\\.?\\d*";

    regex.setPattern("Azimuth:\\s*(" + numPattern + ")");
    match = regex.match(response);
    if (match.hasMatch()) azimuth = match.captured(1).toDouble();

    regex.setPattern("Elevation:\\s*(" + numPattern + ")");
    match = regex.match(response);
    if (match.hasMatch()) elevation = match.captured(1).toDouble();

    regex.setPattern("Polarization:\\s*(" + numPattern + ")");
    match = regex.match(response);
    if (match.hasMatch()) polarization = match.captured(1).toDouble();

    regex.setPattern("Speed:\\s*(" + numPattern + ")");
    match = regex.match(response);
    if (match.hasMatch()) speed = match.captured(1).toDouble();

    regex.setPattern("Speed Mode:\\s*(\\d+)");
    match = regex.match(response);
    if (match.hasMatch()) speedMode = match.captured(1).toInt();

    regex.setPattern("Moving:\\s*(YES|NO)");
    match = regex.match(response);
    if (match.hasMatch()) isMoving = (match.captured(1) == "YES");

    regex.setPattern("Calibrated:\\s*(YES|NO)");
    match = regex.match(response);
    if (match.hasMatch()) isCalibrated = (match.captured(1) == "YES");

    regex.setPattern("Running:\\s*(YES|NO)");
    match = regex.match(response);
    if (match.hasMatch()) isRunning = (match.captured(1) == "YES");

    regex.setPattern("Uptime:\\s*(\\d+)");
    match = regex.match(response);
    if (match.hasMatch()) uptime = match.captured(1).toLongLong();

    // Парсим лимиты
    double minAzimuth = 0, maxAzimuth = 360;
    double minElevation = -90, maxElevation = 90;
    double minPolarization = -180, maxPolarization = 180;
    double maxSpeed = 50;

    regex.setPattern("Min Azimuth:\\s*([\\d.]+)");
    match = regex.match(response);
    if (match.hasMatch()) minAzimuth = match.captured(1).toDouble();

    regex.setPattern("Max Azimuth:\\s*([\\d.]+)");
    match = regex.match(response);
    if (match.hasMatch()) maxAzimuth = match.captured(1).toDouble();

    regex.setPattern("Min Elevation:\\s*([\\d.]+)");
    match = regex.match(response);
    if (match.hasMatch()) minElevation = match.captured(1).toDouble();

    regex.setPattern("Max Elevation:\\s*([\\d.]+)");
    match = regex.match(response);
    if (match.hasMatch()) maxElevation = match.captured(1).toDouble();

    regex.setPattern("Min Polarization:\\s*([\\d.]+)");
    match = regex.match(response);
    if (match.hasMatch()) minPolarization = match.captured(1).toDouble();

    regex.setPattern("Max Polarization:\\s*([\\d.]+)");
    match = regex.match(response);
    if (match.hasMatch()) maxPolarization = match.captured(1).toDouble();

    regex.setPattern("Max Speed:\\s*([\\d.]+)");
    match = regex.match(response);
    if (match.hasMatch()) maxSpeed = match.captured(1).toDouble();

    // Парсим режимы скоростей
    int speedModesCount = 0;
    regex.setPattern("Speed Modes Count:\\s*(\\d+)");
    match = regex.match(response);
    if (match.hasMatch()) {
        speedModesCount = match.captured(1).toInt();
    }

    // Обновляем комбобокс скоростей

    m_speedModeCombo->currentText();
    regex.setPattern("\\s*(" + numPattern + ")");
    match = regex.match(m_speedModeCombo->currentText());
    if (match.hasMatch()) {
        double tmp_speed = match.captured(1).toDouble();

        if (tmp_speed != speed) {
            m_speedModeCombo->blockSignals(true);
            m_speedModeCombo->clear();

            for (int i = 0; i < speedModesCount; ++i) {
                QString pattern = QString("Speed Mode %1:\\s*([^\\s]+)\\s*\\(([\\d.]+)\\s*deg/sec\\)").arg(i);
                regex.setPattern(pattern);
                match = regex.match(response);

                if (match.hasMatch()) {
                    QString name = match.captured(1);
                    double modeSpeed = match.captured(2).toDouble();
                    m_speedModeCombo->addItem(QString("%1 (%2 deg/s)").arg(name).arg(modeSpeed, 0, 'f', 1));
                }
            }

            if (speedMode >= 0 && speedMode < m_speedModeCombo->count()) {
                m_speedModeCombo->setCurrentIndex(speedMode);
            }
            m_speedModeCombo->blockSignals(false);
        }
    }
    else {
        m_speedModeCombo->blockSignals(true);
        m_speedModeCombo->clear();

        for (int i = 0; i < speedModesCount; ++i) {
            QString pattern = QString("Speed Mode %1:\\s*([^\\s]+)\\s*\\(([\\d.]+)\\s*deg/sec\\)").arg(i);
            regex.setPattern(pattern);
            match = regex.match(response);

            if (match.hasMatch()) {
                QString name = match.captured(1);
                double modeSpeed = match.captured(2).toDouble();
                m_speedModeCombo->addItem(QString("%1 (%2 deg/s)").arg(name).arg(modeSpeed, 0, 'f', 1));
            }
        }

        if (speedMode >= 0 && speedMode < m_speedModeCombo->count()) {
            m_speedModeCombo->setCurrentIndex(speedMode);
        }
        m_speedModeCombo->blockSignals(false);
    }



    // Обновляем слайдеры
    m_azimuthControlSlider->setRange(
        static_cast<int>(minAzimuth),
        static_cast<int>(maxAzimuth)
    );
    m_azimuthControlSlider->setToolTip(QString("Range: %1 deg - %2 deg")
        .arg(minAzimuth, 0, 'f', 1)
        .arg(maxAzimuth, 0, 'f', 1));

    m_elevationControlSlider->setRange(
        static_cast<int>(minElevation),
        static_cast<int>(maxElevation)
    );
    m_elevationControlSlider->setToolTip(QString("Range: %1 deg - %2 deg")
        .arg(minElevation, 0, 'f', 1)
        .arg(maxElevation, 0, 'f', 1));

    m_polarizationControlSlider->setRange(
        static_cast<int>(minPolarization),
        static_cast<int>(maxPolarization)
    );
    m_polarizationControlSlider->setToolTip(QString("Range: %1 deg - %2 deg")
        .arg(minPolarization, 0, 'f', 1)
        .arg(maxPolarization, 0, 'f', 1));

    // Обновляем лимиты
    m_azimuthLimitsLabel->setText(QString("Range: %1 deg - %2 deg")
        .arg(minAzimuth, 0, 'f', 1)
        .arg(maxAzimuth, 0, 'f', 1));

    m_elevationLimitsLabel->setText(QString("Range: %1 deg - %2 deg")
        .arg(minElevation, 0, 'f', 1)
        .arg(maxElevation, 0, 'f', 1));

    m_polarizationLimitsLabel->setText(QString("Range: %1 deg - %2 deg")
        .arg(minPolarization, 0, 'f', 1)
        .arg(maxPolarization, 0, 'f', 1));

    m_speedLimitsLabel->setText(QString("Max: %1 deg/s")
        .arg(maxSpeed, 0, 'f', 1));

    // Обновляем статус
    m_runningStatusLabel->setText(isRunning ? "RUNNING" : "STOPPED");
    m_runningStatusLabel->setStyleSheet(isRunning ?
        "font-weight: bold; color: #4CAF50;" :
        "font-weight: bold; color: #f44336;");

    m_uptimeStatusLabel->setText(QString("%1 seconds").arg(uptime));

    qDebug() << azimuth;
    qDebug() << elevation;
    qDebug() << polarization;
    qDebug() << Qt::endl;
    qDebug() << minAzimuth;
    qDebug() << minElevation;
    qDebug() << minPolarization;
    qDebug() << Qt::endl;

    updateAntennaTypeCombo(antennaType);

    updateAntennaStatusDisplay(azimuth, elevation, polarization, speed, speedMode, isMoving, isCalibrated);
}

void AntennaClient::updateAntennaTypeCombo(const QString& currentType)
{
    m_antennaTypeCombo->blockSignals(true);

    // Если тип не пустой - выбираем его
    if (!currentType.isEmpty() && m_antennaTypeCombo->currentText() != currentType) {
        int index = m_antennaTypeCombo->findText(currentType);
        if (index >= 0) {
            m_antennaTypeCombo->setCurrentIndex(index);
        }
    }

    m_antennaTypeCombo->blockSignals(false);
}

void AntennaClient::updateConnectionStatus(bool connected)
{
    QString style = connected ?
        "background-color: #4CAF50; color: white; padding: 5px; border-radius: 3px;" :
        "background-color: #f44336; color: white; padding: 5px; border-radius: 3px;";
    m_connectionStatusLabel->setStyleSheet(style);
    m_connectionStatusLabel->setText(connected ? "CONNECTED" : "DISCONNECTED");
}

void AntennaClient::updateUIState()
{
    bool connected = m_isConnected;
    m_connectButton->setEnabled(!connected);
    m_disconnectButton->setEnabled(connected);
    m_hostEdit->setEnabled(!connected);
    m_portEdit->setEnabled(!connected);

    m_applyPositionButton->setEnabled(connected);
    m_calibrateButton->setEnabled(connected);
    m_startButton->setEnabled(connected);
    m_stopButton->setEnabled(connected);
    m_speedModeCombo->setEnabled(connected);
    m_azimuthControlSlider->setEnabled(connected);
    m_elevationControlSlider->setEnabled(connected);
    m_polarizationControlSlider->setEnabled(connected);
    m_antennaTypeCombo->setEnabled(connected);
}

void AntennaClient::requestStatus()
{
    if (m_isConnected) {
        sendCommand("ANTENN_STATUS");
    }
}

// ===== Слоты управления =====

void AntennaClient::onApplyPositionButtonClicked()
{
    int azimuth = m_azimuthControlSlider->value();
    int elevation = m_elevationControlSlider->value();
    int polarization = m_polarizationControlSlider->value();

    sendCommand(QString("SET_POSITION %1 %2 %3")
        .arg(azimuth)
        .arg(elevation)
        .arg(polarization));

    m_statusBar->showMessage(QString("Position set: AZ=%1 EL=%2 POL=%3")
        .arg(azimuth)
        .arg(elevation)
        .arg(polarization), 2000);
}

void AntennaClient::onSpeedModeChanged(int index)
{
    if (m_isConnected && index >= 0) {
        sendCommand("SET_SPEED_MODE " + QString::number(index));
    }
}

void AntennaClient::onCalibrateButtonClicked()
{
    sendCommand("RESET");
    m_statusBar->showMessage("Reset command sent", 2000);
}

void AntennaClient::onStartButtonClicked()
{
    sendCommand("START");
    m_statusBar->showMessage("Start command sent", 2000);
}

void AntennaClient::onStopButtonClicked()
{
    sendCommand("STOP");
    m_statusBar->showMessage("Stop command sent", 2000);
}

void AntennaClient::onAzimuthSliderChanged(int value)
{
    m_azimuthControlLabel->setText(QString("%1 deg").arg(value));
}

void AntennaClient::onElevationSliderChanged(int value)
{
    m_elevationControlLabel->setText(QString("%1 deg").arg(value));
}

void AntennaClient::onPolarizationSliderChanged(int value)
{
    m_polarizationControlLabel->setText(QString("%1 deg").arg(value));
}

void AntennaClient::onAntennaTypeSelected(int index)
{
    if (m_isConnected && index >= 0) {
        QString type = m_antennaTypeCombo->currentText();
        sendCommand("SELECT_ANTENNA_TYPE " + type);
        m_statusBar->showMessage("Switching to antenna type: " + type, 2000);
    }
}
