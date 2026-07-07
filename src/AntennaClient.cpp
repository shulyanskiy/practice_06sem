#include "include/AntennaClient.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QHostAddress>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollBar>
#include <QFont>
#include <QDateTime>
#include <QTableWidget>
#include <QHeaderView>

AntennaClient::AntennaClient(QWidget* parent)
    : QWidget(parent)
    , m_socket(nullptr)
    , m_host("127.0.0.1")
    , m_port(12345)
    , m_isConnected(false)
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
    connect(m_sendButton, &QPushButton::clicked, this, &AntennaClient::onSendCommandButtonClicked);
    connect(m_statusButton, &QPushButton::clicked, this, &AntennaClient::onRequestStatusButtonClicked);
    connect(m_antennStatusButton, &QPushButton::clicked, this, &AntennaClient::onRequestAntennStatusButtonClicked);
    connect(m_calibrateButton, &QPushButton::clicked, this, &AntennaClient::onCalibrateButtonClicked);
    connect(m_startButton, &QPushButton::clicked, this, &AntennaClient::onStartButtonClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &AntennaClient::onStopButtonClicked);
    connect(m_speedModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &AntennaClient::onSpeedModeChanged);
    connect(m_azimuthControlSlider, &QSlider::valueChanged, this, &AntennaClient::onAzimuthSliderChanged);
    connect(m_elevationControlSlider, &QSlider::valueChanged, this, &AntennaClient::onElevationSliderChanged);
    connect(m_polarizationControlSlider, &QSlider::valueChanged, this, &AntennaClient::onPolarizationSliderChanged);
    connect(m_applyAzimuthButton, &QPushButton::clicked, this, &AntennaClient::onApplyAzimuthButtonClicked);
    connect(m_applyElevationButton, &QPushButton::clicked, this, &AntennaClient::onApplyElevationButtonClicked);
    connect(m_applyPolarizationButton, &QPushButton::clicked, this, &AntennaClient::onApplyPolarizationButtonClicked);
    connect(m_loadConfigButton, &QPushButton::clicked, this, &AntennaClient::onLoadConfigButtonClicked);
    connect(m_saveConfigButton, &QPushButton::clicked, this, &AntennaClient::onSaveConfigButtonClicked);

    // За degужаем настройки
    loadSettings();

    // Инициализация комбобокса
    m_speedModeCombo->addItems({ "no data"});

    // Устанавливаем стиль
    setWindowTitle("Antenna Emulator Client");
    resize(1000, 750);

    // Инициализация статуса
    updateAntennaStatusDisplay(0, 0, 0, 0, 0, false, false);
    updateUIState();
    appendLog("Application started", "blue");
}

AntennaClient::~AntennaClient()
{
    saveSettings();
    if (m_socket) {
        m_socket->disconnectFromHost();
        m_socket->deleteLater();
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

    // ===== 2. Основная панель: управление и статус =====
    QHBoxLayout* mainPanelLayout = new QHBoxLayout();
    mainPanelLayout->setSpacing(15);

    // ===== 2.1 Левая панель: Управление антенной =====
    QGroupBox* controlGroup = new QGroupBox("Antenna Control");
    QVBoxLayout* controlLayout = new QVBoxLayout(controlGroup);
    controlLayout->setSpacing(8);

    // Слайдеры управления
    QFormLayout* slidersLayout = new QFormLayout();
    slidersLayout->setSpacing(5);

    // Азимут (управление)
    QHBoxLayout* azimuthControlLayout = new QHBoxLayout();
    m_azimuthControlSlider = new QSlider(Qt::Horizontal);
    m_azimuthControlSlider->setRange(0, 360);
    m_azimuthControlSlider->setValue(0);
    m_azimuthControlSlider->setFixedWidth(180);
    m_azimuthControlLabel = new QLabel("0 deg");
    m_azimuthControlLabel->setFixedWidth(45);
    m_applyAzimuthButton = new QPushButton("Apply");
    m_applyAzimuthButton->setFixedWidth(60);
    azimuthControlLayout->addWidget(m_azimuthControlSlider);
    azimuthControlLayout->addWidget(m_azimuthControlLabel);
    azimuthControlLayout->addWidget(m_applyAzimuthButton);
    slidersLayout->addRow("Azimuth:", azimuthControlLayout);

    // Угол места (управление)
    QHBoxLayout* elevationControlLayout = new QHBoxLayout();
    m_elevationControlSlider = new QSlider(Qt::Horizontal);
    m_elevationControlSlider->setRange(-90, 90);
    m_elevationControlSlider->setValue(0);
    m_elevationControlSlider->setFixedWidth(180);
    m_elevationControlLabel = new QLabel("0 deg");
    m_elevationControlLabel->setFixedWidth(45);
    m_applyElevationButton = new QPushButton("Apply");
    m_applyElevationButton->setFixedWidth(60);
    elevationControlLayout->addWidget(m_elevationControlSlider);
    elevationControlLayout->addWidget(m_elevationControlLabel);
    elevationControlLayout->addWidget(m_applyElevationButton);
    slidersLayout->addRow("Elevation:", elevationControlLayout);

    // Поляризация (управление)
    QHBoxLayout* polarizationControlLayout = new QHBoxLayout();
    m_polarizationControlSlider = new QSlider(Qt::Horizontal);
    m_polarizationControlSlider->setRange(-180, 180);
    m_polarizationControlSlider->setValue(0);
    m_polarizationControlSlider->setFixedWidth(180);
    m_polarizationControlLabel = new QLabel("0 deg");
    m_polarizationControlLabel->setFixedWidth(45);
    m_applyPolarizationButton = new QPushButton("Apply");
    m_applyPolarizationButton->setFixedWidth(60);
    polarizationControlLayout->addWidget(m_polarizationControlSlider);
    polarizationControlLayout->addWidget(m_polarizationControlLabel);
    polarizationControlLayout->addWidget(m_applyPolarizationButton);
    slidersLayout->addRow("Polarization:", polarizationControlLayout);

    controlLayout->addLayout(slidersLayout);

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
    m_calibrateButton = new QPushButton("Calibrate");
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

    // Команды
    QHBoxLayout* commandLayout = new QHBoxLayout();
    m_commandEdit = new QLineEdit();
    m_commandEdit->setPlaceholderText("Enter custom command...");
    m_sendButton = new QPushButton("Send");
    m_sendButton->setFixedWidth(80);
    commandLayout->addWidget(m_commandEdit);
    commandLayout->addWidget(m_sendButton);
    controlLayout->addLayout(commandLayout);

    // Кнопки запросов статуса
    QHBoxLayout* requestLayout = new QHBoxLayout();
    m_statusButton = new QPushButton("Manager Status");
    m_antennStatusButton = new QPushButton("Request Antenna Status");
    m_statusButton->setFixedWidth(130);
    m_antennStatusButton->setFixedWidth(160);
    requestLayout->addWidget(m_statusButton);
    requestLayout->addWidget(m_antennStatusButton);
    requestLayout->addStretch();
    controlLayout->addLayout(requestLayout);

    // Конфигурация
    QHBoxLayout* configLayout = new QHBoxLayout();
    m_configPathEdit = new QLineEdit("config/antenna_config.ini");
    m_loadConfigButton = new QPushButton("Load");
    m_saveConfigButton = new QPushButton("Save");
    m_loadConfigButton->setFixedWidth(60);
    m_saveConfigButton->setFixedWidth(60);
    configLayout->addWidget(new QLabel("Config:"));
    configLayout->addWidget(m_configPathEdit);
    configLayout->addWidget(m_loadConfigButton);
    configLayout->addWidget(m_saveConfigButton);
    controlLayout->addLayout(configLayout);

    controlLayout->addStretch();
    mainPanelLayout->addWidget(controlGroup);

    // ===== 2.2 Правая панель: Статус антенны (только для чтения) =====
    QGroupBox* statusGroup = new QGroupBox("Antenna Status (Read-Only)");
    QGridLayout* statusLayout = new QGridLayout(statusGroup);
    statusLayout->setSpacing(8);
    statusLayout->setColumnStretch(1, 1);

    // Заголовки
    statusLayout->addWidget(new QLabel("<b>Parameter</b>"), 0, 0);
    statusLayout->addWidget(new QLabel("<b>Value</b>"), 0, 1);

    // ===== АЗИМУТ =====
    QVBoxLayout* azimuthLayout = new QVBoxLayout();
    azimuthLayout->setSpacing(2);

    m_azimuthStatusLabel = new QLabel("0.00 deg");
    m_azimuthStatusLabel->setStyleSheet("font-weight: bold; color: #2196F3;");

    m_azimuthLimitsLabel = new QLabel("Range: 0  deg - 360 deg");
    m_azimuthLimitsLabel->setStyleSheet("color: #757575; font-size: 9pt;");

    azimuthLayout->addWidget(m_azimuthStatusLabel);
    azimuthLayout->addWidget(m_azimuthLimitsLabel);

    statusLayout->addWidget(new QLabel("Azimuth:"), 1, 0);
    statusLayout->addLayout(azimuthLayout, 1, 1);

    // ===== УГОЛ МЕСТА =====
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

    // ===== ПОЛЯРИЗАЦИЯ =====
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

    // ===== РАЗДЕЛИТЕЛЬ =====
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    statusLayout->addWidget(line, 4, 0, 1, 2);

    // ===== СКОРОСТЬ =====
    QVBoxLayout* speedLayout2 = new QVBoxLayout();
    speedLayout2->setSpacing(2);

    m_speedStatusLabel = new QLabel("0.00  deg/s");
    m_speedStatusLabel->setStyleSheet("font-weight: bold;");

    m_speedLimitsLabel = new QLabel("Max: 50.0  deg/s");
    m_speedLimitsLabel->setStyleSheet("color: #757575; font-size: 9pt;");

    speedLayout2->addWidget(m_speedStatusLabel);
    // speedLayout2->addWidget(m_speedLimitsLabel);

    statusLayout->addWidget(new QLabel("Speed:"), 5, 0);
    statusLayout->addLayout(speedLayout2, 5, 1);

    // ===== РЕЖИМ СКОРОСТИ =====
    m_speedModeStatusLabel = new QLabel("0 (Slow)");
    m_speedModeStatusLabel->setStyleSheet("font-weight: bold;");
    statusLayout->addWidget(new QLabel("Speed Mode:"), 6, 0);
    statusLayout->addWidget(m_speedModeStatusLabel, 6, 1);

    // ===== ДВИЖЕНИЕ =====
    m_movingStatusLabel = new QLabel("NO");
    m_movingStatusLabel->setStyleSheet("font-weight: bold; color: #757575;");
    statusLayout->addWidget(new QLabel("Moving:"), 7, 0);
    statusLayout->addWidget(m_movingStatusLabel, 7, 1);

    // ===== КАЛИБРОВКА =====
    m_calibratedStatusLabel = new QLabel("YES");
    m_calibratedStatusLabel->setStyleSheet("font-weight: bold; color: #4CAF50;");
    statusLayout->addWidget(new QLabel("Calibrated:"), 8, 0);
    statusLayout->addWidget(m_calibratedStatusLabel, 8, 1);

    // ===== ЗАПУЩЕНА =====
    m_runningStatusLabel = new QLabel("STOPPED");
    m_runningStatusLabel->setStyleSheet("font-weight: bold; color: #f44336;");
    statusLayout->addWidget(new QLabel("Running:"), 9, 0);
    statusLayout->addWidget(m_runningStatusLabel, 9, 1);

    // ===== ВРЕМЯ РАБОТЫ =====
    m_uptimeStatusLabel = new QLabel("0 seconds");
    m_uptimeStatusLabel->setStyleSheet("font-weight: bold;");
    statusLayout->addWidget(new QLabel("Uptime:"), 10, 0);
    statusLayout->addWidget(m_uptimeStatusLabel, 10, 1);

    // ===== БЫСТРЫЙ СТАТУС =====
    QHBoxLayout* quickStatusLayout = new QHBoxLayout();
    m_clientsLabel = new QLabel("Clients: 0");
    m_antennaRunningLabel = new QLabel("Antenna: STOPPED");
    quickStatusLayout->addWidget(m_clientsLabel);
    quickStatusLayout->addSpacing(20);
    quickStatusLayout->addWidget(m_antennaRunningLabel);
    quickStatusLayout->addStretch();
    statusLayout->addLayout(quickStatusLayout, 11, 0, 1, 2);

    statusLayout->setRowStretch(12, 1);
    mainPanelLayout->addWidget(statusGroup);

    mainLayout->addLayout(mainPanelLayout);

    // ===== 3. Нижняя панель: лог =====
    QGroupBox* logGroup = new QGroupBox("Log");
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);

    m_logTextEdit = new QTextEdit();
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setFont(QFont("Consolas", 9));
    m_logTextEdit->setMinimumHeight(200);
    logLayout->addWidget(m_logTextEdit);

    mainLayout->addWidget(logGroup);

    // Status bar
    m_statusBar = new QStatusBar();
    m_statusBar->setMaximumHeight(25);
    mainLayout->addWidget(m_statusBar);
}

void AntennaClient::updateAntennaStatusDisplay(double azimuth, double elevation, double polarization,
    double speed, int speedMode, bool isMoving, bool isCalibrated)
{
    // Обновляем метки статуса (только для чтения)
    m_azimuthStatusLabel->setText(QString("%1 deg").arg(azimuth, 0, 'f', 2));
    m_elevationStatusLabel->setText(QString("%1 deg").arg(elevation, 0, 'f', 2));
    m_polarizationStatusLabel->setText(QString("%1 deg").arg(polarization, 0, 'f', 2));
    m_speedStatusLabel->setText(QString("%1  deg/s").arg(speed, 0, 'f', 2));

    // Режим скорости
    QStringList modeNames = { "Slow", "Normal", "Fast", "Very Fast", "Extreme" };
    QString modeText = (speedMode >= 0 && speedMode < modeNames.size()) ?
        QString("%1 (%2)").arg(speedMode).arg(modeNames[speedMode]) :
        QString::number(speedMode);
    m_speedModeStatusLabel->setText(modeText);

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

void AntennaClient::appendLog(const QString& message, const QString& color)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");

    QString escapedMessage = message;
    escapedMessage.replace("&", "&amp;");
    escapedMessage.replace("<", "&lt;");
    escapedMessage.replace(">", "&gt;");
    escapedMessage.replace("\"", "&quot;");
    escapedMessage.replace("\n", "<br>");

    QString formattedMsg = QString("[%1] %2").arg(timestamp, escapedMessage);

    if (!color.isEmpty()) {
        formattedMsg = QString("<font color=\"%1\">%2</font>").arg(color, formattedMsg);
    }

    m_logTextEdit->append(formattedMsg);

    QScrollBar* scrollbar = m_logTextEdit->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

void AntennaClient::loadSettings()
{
    if (m_settings) {
        m_host = m_settings->value("Connection/Host", "127.0.0.1").toString();
        m_port = m_settings->value("Connection/Port", 12345).toUInt();

        m_hostEdit->setText(m_host);
        m_portEdit->setText(QString::number(m_port));

        QString configPath = m_settings->value("Config/Path", "config/antenna_config.ini").toString();
        m_configPathEdit->setText(configPath);
    }
}

void AntennaClient::saveSettings()
{
    if (m_settings) {
        m_settings->setValue("Connection/Host", m_hostEdit->text());
        m_settings->setValue("Connection/Port", m_portEdit->text().toUInt());
        m_settings->setValue("Config/Path", m_configPathEdit->text());
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

    appendLog("Connecting to " + m_host + ":" + QString::number(m_port) + "...", "blue");
    m_socket->connectToHost(m_host, m_port);
}

void AntennaClient::onDisconnectButtonClicked()
{
    if (m_isConnected) {
        m_socket->disconnectFromHost();
    }
}

void AntennaClient::onSocketConnected()
{
    m_isConnected = true;
    updateConnectionStatus(true);
    appendLog("Connected to " + m_host + ":" + QString::number(m_port), "green");
    updateUIState();
    m_statusBar->showMessage("Connected", 2000);
}

void AntennaClient::onSocketDisconnected()
{
    m_isConnected = false;
    updateConnectionStatus(false);
    appendLog("Disconnected", "red");
    updateUIState();
    m_statusBar->showMessage("Disconnected", 2000);
}

void AntennaClient::onSocketError(QAbstractSocket::SocketError error)
{
    QString errorMsg = m_socket->errorString();
    appendLog("Socket error: " + errorMsg, "red");
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
                    // appendLog("Invalid block size: " + QString::number(m_buffer.nextBlockSize), "red");
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
                appendLog("Failed to read full message", "red");
                m_buffer.reset();
                m_socket->readAll();
                break;
            }

            processResponse(m_buffer.rawData);
            m_buffer.reset();

        }
        catch (const std::exception& e) {
            appendLog("Exception: " + QString(e.what()), "red");
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
        appendLog("Failed to send data", "red");
    }
    else {
        m_socket->flush();
    }
}

void AntennaClient::sendCommand(const QString& command)
{
    sendData(command.toUtf8());
    appendLog("-> " + command, "purple");
}

void AntennaClient::processResponse(const QByteArray& data)
{
    QString response = QString::fromUtf8(data);
    QString trimmed = response.trimmed();

    appendLog("<- " + trimmed, "darkgreen");

    if (response.contains("=== MANAGER STATUS ===")) {
        parseStatusResponse(response);
    }
    else if (response.contains("=== ANTENNA STATUS ===")) {
        parseAntennStatusResponse(response);
    }

    if (response.contains("ERROR:")) {
        QString errorMsg = trimmed;
        if (errorMsg.startsWith("ERROR: ")) {
            errorMsg = errorMsg.mid(7);
        }
        appendLog("Error: " + errorMsg, "red");
        m_statusBar->showMessage("Error: " + errorMsg, 3000);
    }
    else if (response.contains("OK:")) {
        appendLog("Success", "green");
        m_statusBar->showMessage("Success: " + trimmed, 2000);

        if (trimmed.contains("Config loaded")) {
            appendLog("Config loaded, requesting updated status...", "blue");
            sendCommand("ANTENN_STATUS");
        }
    }
}

void AntennaClient::parseStatusResponse(const QString& response)
{
    QRegularExpression regex;
    QRegularExpressionMatch match;

    // Парсим количество клиентов
    regex.setPattern("Clients:\\s*(\\d+)");
    match = regex.match(response);
    if (match.hasMatch()) {
        m_clientsLabel->setText("Clients: " + match.captured(1));
    }

    // Парсим статус антенны
    regex.setPattern("Antenna running:\\s*(YES|NO)");
    match = regex.match(response);
    if (match.hasMatch()) {
        QString status = match.captured(1);
        bool isRunning = (status == "YES");

        m_antennaRunningLabel->setText("Antenna: " + status);
        m_antennaRunningLabel->setStyleSheet(isRunning ? "color: green;" : "color: red;");

        // Обновляем статус Running в панели статуса
        m_runningStatusLabel->setText(isRunning ? "RUNNING" : "STOPPED");
        m_runningStatusLabel->setStyleSheet(isRunning ?
            "font-weight: bold; color: #4CAF50;" :
            "font-weight: bold; color: #f44336;");
    }

    // Парсим состояние Running (если есть)
    regex.setPattern("Running:\\s*(YES|NO)");
    match = regex.match(response);
    if (match.hasMatch()) {
        // Дублирующая информация, но для надежности
        bool isRunning = (match.captured(1) == "YES");
        m_runningStatusLabel->setText(isRunning ? "RUNNING" : "STOPPED");
        m_runningStatusLabel->setStyleSheet(isRunning ?
            "font-weight: bold; color: #4CAF50;" :
            "font-weight: bold; color: #f44336;");
    }

    // Парсим порт
    regex.setPattern("Port:\\s*(\\d+)");
    match = regex.match(response);
    if (match.hasMatch()) {
        // Можно использовать для отображения, но не обязательно
    }
}

void AntennaClient::parseAntennStatusResponse(const QString& response)
{
    QRegularExpression regex;
    QRegularExpressionMatch match;

    // Парсим статус
    double azimuth = 0, elevation = 0, polarization = 0;
    double speed = 0;
    int speedMode = 0;
    bool isMoving = false;
    bool isCalibrated = false;
    bool isRunning = false;
    qint64 uptime = 0;

    regex.setPattern("Azimuth:\\s*([\\d.]+)");
    match = regex.match(response);
    if (match.hasMatch()) azimuth = match.captured(1).toDouble();

    regex.setPattern("Elevation:\\s*([\\d.]+)");
    match = regex.match(response);
    if (match.hasMatch()) elevation = match.captured(1).toDouble();

    regex.setPattern("Polarization:\\s*([\\d.]+)");
    match = regex.match(response);
    if (match.hasMatch()) polarization = match.captured(1).toDouble();

    regex.setPattern("Speed:\\s*([\\d.]+)");
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


    m_speedModeCombo->blockSignals(true);
    m_speedModeCombo->clear();

    for (int i = 0; i < speedModesCount; ++i) {
        QString pattern = QString("Speed Mode %1:\\s*([^\\s]+)\\s*\\(([\\d.]+)\\s* deg/sec\\)").arg(i);
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


    // Обновляем UI

    // 1 Статус
    m_azimuthStatusLabel->setText(QString("%1 deg").arg(azimuth, 0, 'f', 2));
    m_elevationStatusLabel->setText(QString("%1 deg").arg(elevation, 0, 'f', 2));
    m_polarizationStatusLabel->setText(QString("%1 deg").arg(polarization, 0, 'f', 2));
    m_speedStatusLabel->setText(QString("%1  deg/s").arg(speed, 0, 'f', 2));
    m_uptimeStatusLabel->setText(QString("%1 seconds").arg(uptime));

    // 2 Режим скорости (выбираем текущий)
    if (speedMode >= 0 && speedMode < m_speedModeCombo->count()) {
        m_speedModeCombo->setCurrentIndex(speedMode);
    }

    // 3 Статусы
    m_movingStatusLabel->setText(isMoving ? "YES" : "NO");
    m_movingStatusLabel->setStyleSheet(isMoving ?
        "font-weight: bold; color: #FF9800;" :
        "font-weight: bold; color: #757575;");

    m_calibratedStatusLabel->setText(isCalibrated ? "YES" : "NO");
    m_calibratedStatusLabel->setStyleSheet(isCalibrated ?
        "font-weight: bold; color: #4CAF50;" :
        "font-weight: bold; color: #f44336;");

    m_runningStatusLabel->setText(isRunning ? "RUNNING" : "STOPPED");
    m_runningStatusLabel->setStyleSheet(isRunning ?
        "font-weight: bold; color: #4CAF50;" :
        "font-weight: bold; color: #f44336;");

    // 4 Лимиты (метки)
    m_azimuthLimitsLabel->setText(QString("Range: %1 deg - %2 deg")
        .arg(minAzimuth, 0, 'f', 1)
        .arg(maxAzimuth, 0, 'f', 1));

    m_elevationLimitsLabel->setText(QString("Range: %1 deg - %2 deg")
        .arg(minElevation, 0, 'f', 1)
        .arg(maxElevation, 0, 'f', 1));

    m_polarizationLimitsLabel->setText(QString("Range: %1 deg - %2 deg")
        .arg(minPolarization, 0, 'f', 1)
        .arg(maxPolarization, 0, 'f', 1));

    m_speedLimitsLabel->setText(QString("Max: %1  deg/s")
        .arg(maxSpeed, 0, 'f', 1));

    // 5 Лимиты (слайдеры)
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

    m_sendButton->setEnabled(connected);
    m_statusButton->setEnabled(connected);
    m_antennStatusButton->setEnabled(connected);
    m_calibrateButton->setEnabled(connected);
    m_startButton->setEnabled(connected);
    m_stopButton->setEnabled(connected);
    m_applyAzimuthButton->setEnabled(connected);
    m_applyElevationButton->setEnabled(connected);
    m_applyPolarizationButton->setEnabled(connected);
    m_loadConfigButton->setEnabled(connected);
    m_saveConfigButton->setEnabled(connected);
    m_speedModeCombo->setEnabled(connected);
    m_azimuthControlSlider->setEnabled(connected);
    m_elevationControlSlider->setEnabled(connected);
    m_polarizationControlSlider->setEnabled(connected);
    m_commandEdit->setEnabled(connected);
}

void AntennaClient::onSendCommandButtonClicked()
{
    QString command = m_commandEdit->text().trimmed();
    if (command.isEmpty()) {
        return;
    }
    sendCommand(command);
    m_commandEdit->clear();
}

void AntennaClient::onRequestStatusButtonClicked()
{
    sendCommand("STATUS");
}

void AntennaClient::onRequestAntennStatusButtonClicked()
{
    sendCommand("ANTENN_STATUS");
}

void AntennaClient::onCalibrateButtonClicked()
{
    sendCommand("CALIBRATE");
}

void AntennaClient::onStartButtonClicked()
{
    sendCommand("START");
    appendLog("Start command sent", "blue");
}

void AntennaClient::onStopButtonClicked()
{
    sendCommand("STOP");
    appendLog("Stop command sent", "blue");
}

void AntennaClient::onSpeedModeChanged(int index)
{
    if (m_isConnected) {
        sendCommand("SET_SPEED_MODE " + QString::number(index));
    }
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

void AntennaClient::onApplyAzimuthButtonClicked()
{
    int value = m_azimuthControlSlider->value();
    sendCommand("SET_AZIMUTH " + QString::number(value));
}

void AntennaClient::onApplyElevationButtonClicked()
{
    int value = m_elevationControlSlider->value();
    sendCommand("SET_ELEVATION " + QString::number(value));
}

void AntennaClient::onApplyPolarizationButtonClicked()
{
    int value = m_polarizationControlSlider->value();
    sendCommand("SET_POLARIZATION " + QString::number(value));
}

void AntennaClient::onLoadConfigButtonClicked()
{
    QString path = QFileDialog::getOpenFileName(this,
        "Load Antenna Config",
        m_configPathEdit->text(),
        "Config Files (*.ini *.conf);;All Files (*)");

    if (!path.isEmpty()) {
        m_configPathEdit->setText(path);
        sendCommand("CONFIG LOAD " + path);
        saveSettings();
    }
}

void AntennaClient::onSaveConfigButtonClicked()
{
    QString path = QFileDialog::getSaveFileName(this,
        "Save Antenna Config",
        m_configPathEdit->text(),
        "Config Files (*.ini);;All Files (*)");

    if (!path.isEmpty()) {
        m_configPathEdit->setText(path);
        sendCommand("CONFIG SAVE " + path);
        saveSettings();
    }
}






