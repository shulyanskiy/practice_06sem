#include "include/ServerWidget.h"
#include "include/emulator.h"
#include "include/AntennaFactory.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QScrollBar>
#include <QTimerEvent>
#include <QThread>

ServerWidget::ServerWidget(QWidget* parent)
    : QWidget(parent)
    , m_manager(nullptr)
    , m_isServerRunning(false)
    , m_statusTimerId(0)
    , m_statusTimerActive(false)
    , m_updateProgress(false)
{
    setupUI();

    // Создаем менеджера в основном потоке
    m_manager = new EmuManager(this);


    // Подключаем сигналы менеджера к слотам виджета
    connect(m_manager, &EmuManager::serverStarted, this, &ServerWidget::onServerStarted);
    connect(m_manager, &EmuManager::serverStopped, this, &ServerWidget::onServerStopped);
    connect(m_manager, &EmuManager::clientConnected, this, &ServerWidget::onClientConnected);
    connect(m_manager, &EmuManager::clientDisconnected, this, &ServerWidget::onClientDisconnected);
    connect(m_manager, &EmuManager::infoOccurred, this, &ServerWidget::onInfo);
    connect(m_manager, &EmuManager::warningOccurred, this, &ServerWidget::onWarning);
    connect(m_manager, &EmuManager::errorOccurred, this, &ServerWidget::onError);
    connect(m_manager, &EmuManager::logRawData, this, &ServerWidget::onLogRawData);

    // Загружаем типы антенн
    loadAntennaTypes();

    updateUIState();

    appendLog("Server application started", "blue");
}

ServerWidget::~ServerWidget()
{
    if (m_statusTimerActive) {
        killTimer(m_statusTimerId);
        m_statusTimerActive = false;
    }
    if (m_manager) {
        disconnect(m_manager, nullptr, this, nullptr);
        m_manager->stop();
        m_manager->deleteLater();
    }
}

void ServerWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    // ===== Верхняя панель: управление сервером =====
    QGroupBox* serverGroup = new QGroupBox("Server Control");
    QHBoxLayout* serverLayout = new QHBoxLayout(serverGroup);

    serverLayout->addWidget(new QLabel("Port:"));
    m_portEdit = new QLineEdit("12345");
    m_portEdit->setFixedWidth(70);
    serverLayout->addWidget(m_portEdit);

    m_startServerButton = new QPushButton("Start Server");
    m_startServerButton->setFixedWidth(120);
    m_stopServerButton = new QPushButton("Stop Server");
    m_stopServerButton->setFixedWidth(120);
    m_stopServerButton->setEnabled(false);

    serverLayout->addWidget(m_startServerButton);
    serverLayout->addWidget(m_stopServerButton);
    serverLayout->addSpacing(20);

    m_serverStatusLabel = new QLabel("STOPPED");
    m_serverStatusLabel->setStyleSheet("background-color: #f44336; color: white; padding: 5px; border-radius: 3px;");
    m_serverStatusLabel->setFixedWidth(100);
    m_serverStatusLabel->setAlignment(Qt::AlignCenter);
    serverLayout->addWidget(m_serverStatusLabel);

    serverLayout->addStretch();

    mainLayout->addWidget(serverGroup);

    // ===== Типы антенн =====
    QHBoxLayout* antennaTypeLayout = new QHBoxLayout();
    antennaTypeLayout->addWidget(new QLabel("Antenna Type:"));
    m_antennaTypeCombo = new QComboBox();
    m_antennaTypeCombo->setFixedWidth(200);
    antennaTypeLayout->addWidget(m_antennaTypeCombo);
    antennaTypeLayout->addStretch();
    mainLayout->addLayout(antennaTypeLayout);

    // ===== Основная панель =====
    QHBoxLayout* mainPanelLayout = new QHBoxLayout();
    mainPanelLayout->setSpacing(15);

    // ===== Левая панель: управление антенной =====
    QGroupBox* controlGroup = new QGroupBox("Antenna Control");
    QVBoxLayout* controlLayout = new QVBoxLayout(controlGroup);

    // Углы
    QFormLayout* anglesLayout = new QFormLayout();
    QHBoxLayout* azimuthLayout = new QHBoxLayout();
    m_azimuthSpin = new QSpinBox();
    m_azimuthSpin->setRange(0, 360);
    m_azimuthSpin->setValue(0);
    m_azimuthSpin->setSuffix(" deg");
    azimuthLayout->addWidget(m_azimuthSpin);
    anglesLayout->addRow("Azimuth:", azimuthLayout);

    QHBoxLayout* elevationLayout = new QHBoxLayout();
    m_elevationSpin = new QSpinBox();
    m_elevationSpin->setRange(-90, 90);
    m_elevationSpin->setValue(0);
    m_elevationSpin->setSuffix(" deg");
    elevationLayout->addWidget(m_elevationSpin);
    anglesLayout->addRow("Elevation:", elevationLayout);

    QHBoxLayout* polarizationLayout = new QHBoxLayout();
    m_polarizationSpin = new QSpinBox();
    m_polarizationSpin->setRange(-180, 180);
    m_polarizationSpin->setValue(0);
    m_polarizationSpin->setSuffix(" deg");
    polarizationLayout->addWidget(m_polarizationSpin);
    anglesLayout->addRow("Polarization:", polarizationLayout);

    controlLayout->addLayout(anglesLayout);

    // Кнопка установки позиции
    m_setPositionButton = new QPushButton("Set Position");
    m_setPositionButton->setStyleSheet("font-weight: bold; background-color: #4CAF50; color: white;");
    m_setPositionButton->setFixedHeight(35);
    controlLayout->addWidget(m_setPositionButton);

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
    m_resetButton = new QPushButton("Reset");
    m_startAntennaButton = new QPushButton("Start");
    m_stopAntennaButton = new QPushButton("Stop");
    m_resetButton->setFixedWidth(80);
    m_startAntennaButton->setFixedWidth(80);
    m_stopAntennaButton->setFixedWidth(80);
    controlButtonsLayout->addWidget(m_resetButton);
    controlButtonsLayout->addWidget(m_startAntennaButton);
    controlButtonsLayout->addWidget(m_stopAntennaButton);
    controlButtonsLayout->addStretch();
    controlLayout->addLayout(controlButtonsLayout);

    // Конфигурация
    QGroupBox* configGroup = new QGroupBox("Configuration");
    QVBoxLayout* configLayout = new QVBoxLayout(configGroup);

    QHBoxLayout* configPathLayout = new QHBoxLayout();
    m_configPathEdit = new QLineEdit("config/antenna_config.ini");
    m_loadConfigButton = new QPushButton("Load");
    m_saveConfigButton = new QPushButton("Save");
    m_loadConfigButton->setFixedWidth(60);
    m_saveConfigButton->setFixedWidth(60);
    configPathLayout->addWidget(m_configPathEdit);
    configPathLayout->addWidget(m_loadConfigButton);
    configPathLayout->addWidget(m_saveConfigButton);
    configLayout->addLayout(configPathLayout);

    QHBoxLayout* addTypeLayout = new QHBoxLayout();
    addTypeLayout->addWidget(new QLabel("New Type:"));
    m_newAntennaTypeEdit = new QLineEdit();
    m_newAntennaTypeEdit->setPlaceholderText("Type name");
    addTypeLayout->addWidget(m_newAntennaTypeEdit);
    addTypeLayout->addWidget(new QLabel("Config:"));
    m_newAntennaConfigEdit = new QLineEdit();
    m_newAntennaConfigEdit->setPlaceholderText("config path (optional)");
    addTypeLayout->addWidget(m_newAntennaConfigEdit);
    m_addAntennaTypeButton = new QPushButton("Add Type");
    m_addAntennaTypeButton->setFixedWidth(80);
    addTypeLayout->addWidget(m_addAntennaTypeButton);
    configLayout->addLayout(addTypeLayout);

    controlLayout->addWidget(configGroup);
    controlLayout->addStretch();

    mainPanelLayout->addWidget(controlGroup);

    // ===== Правая панель: статус и логи =====
    QWidget* rightPanel = new QWidget();
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setSpacing(10);

    // Статус антенны
    QGroupBox* statusGroup = new QGroupBox("Antenna Status");
    QGridLayout* statusLayout = new QGridLayout(statusGroup);
    statusLayout->setSpacing(5);

    statusLayout->addWidget(new QLabel("Azimuth:"), 0, 0);
    m_azimuthStatusLabel = new QLabel("0.00 deg");
    m_azimuthStatusLabel->setStyleSheet("font-weight: bold; color: #2196F3;");
    statusLayout->addWidget(m_azimuthStatusLabel, 0, 1);

    statusLayout->addWidget(new QLabel("Elevation:"), 1, 0);
    m_elevationStatusLabel = new QLabel("0.00 deg");
    m_elevationStatusLabel->setStyleSheet("font-weight: bold; color: #4CAF50;");
    statusLayout->addWidget(m_elevationStatusLabel, 1, 1);

    statusLayout->addWidget(new QLabel("Polarization:"), 2, 0);
    m_polarizationStatusLabel = new QLabel("0.00 deg");
    m_polarizationStatusLabel->setStyleSheet("font-weight: bold; color: #FF9800;");
    statusLayout->addWidget(m_polarizationStatusLabel, 2, 1);

    statusLayout->addWidget(new QLabel("Speed:"), 3, 0);
    m_speedStatusLabel = new QLabel("0.00 deg/s");
    m_speedStatusLabel->setStyleSheet("font-weight: bold;");
    statusLayout->addWidget(m_speedStatusLabel, 3, 1);

    statusLayout->addWidget(new QLabel("Moving:"), 4, 0);
    m_movingStatusLabel = new QLabel("NO");
    m_movingStatusLabel->setStyleSheet("font-weight: bold; color: #757575;");
    statusLayout->addWidget(m_movingStatusLabel, 4, 1);

    statusLayout->addWidget(new QLabel("Running:"), 5, 0);
    m_runningStatusLabel = new QLabel("STOPPED");
    m_runningStatusLabel->setStyleSheet("font-weight: bold; color: #f44336;");
    statusLayout->addWidget(m_runningStatusLabel, 5, 1);

    statusLayout->addWidget(new QLabel("Uptime:"), 6, 0);
    m_uptimeStatusLabel = new QLabel("0 sec");
    m_uptimeStatusLabel->setStyleSheet("font-weight: bold;");
    statusLayout->addWidget(m_uptimeStatusLabel, 6, 1);


    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    statusLayout->addWidget(line, 7, 0, 1, 2);

    statusLayout->addWidget(new QLabel("<b>Limits:</b>"), 8, 0, 1, 2);
    statusLayout->addWidget(new QLabel("Azimuth:"), 9, 0);
    m_azimuthLimitsLabel = new QLabel("0.0 - 360.0 deg");
    m_azimuthLimitsLabel->setStyleSheet("color: #757575; font-size: 9pt;");
    statusLayout->addWidget(m_azimuthLimitsLabel, 9, 1);

    statusLayout->addWidget(new QLabel("Elevation:"), 10, 0);
    m_elevationLimitsLabel = new QLabel("-90.0 - 90.0 deg");
    m_elevationLimitsLabel->setStyleSheet("color: #757575; font-size: 9pt;");
    statusLayout->addWidget(m_elevationLimitsLabel, 10, 1);

    statusLayout->addWidget(new QLabel("Polarization:"), 11, 0);
    m_polarizationLimitsLabel = new QLabel("-180.0 - 180.0 deg");
    m_polarizationLimitsLabel->setStyleSheet("color: #757575; font-size: 9pt;");
    statusLayout->addWidget(m_polarizationLimitsLabel, 11, 1);

    statusLayout->addWidget(new QLabel("Max Speed:"), 12, 0);
    m_speedLimitsLabel = new QLabel("50.0 deg/s");
    m_speedLimitsLabel->setStyleSheet("color: #757575; font-size: 9pt;");
    statusLayout->addWidget(m_speedLimitsLabel, 12, 1);

    statusLayout->setColumnStretch(0, 0);
    statusLayout->setColumnStretch(1, 1);

    rightLayout->addWidget(statusGroup);

    // Логи
    m_logTabs = new QTabWidget();
    m_logTabs->setTabPosition(QTabWidget::South);

    // Текстовые логи
    m_logTextEdit = new QTextEdit();
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setFont(QFont("Consolas", 9));
    m_logTextEdit->setMinimumHeight(200);
    m_logTabs->addTab(m_logTextEdit, "Log");

    // Сырые данные
    m_rawDataTextEdit = new QTextEdit();
    m_rawDataTextEdit->setReadOnly(true);
    m_rawDataTextEdit->setFont(QFont("Consolas", 9));
    m_rawDataTextEdit->setMinimumHeight(200);
    m_logTabs->addTab(m_rawDataTextEdit, "Raw Data");

    rightLayout->addWidget(m_logTabs);

    mainPanelLayout->addWidget(rightPanel, 1);

    mainLayout->addLayout(mainPanelLayout);

    // ===== Статусная строка =====
    m_statusBar = new QStatusBar();
    m_statusBar->setMaximumHeight(25);
    mainLayout->addWidget(m_statusBar);

    // ===== Подключаем сигналы =====
    connect(m_startServerButton, &QPushButton::clicked, this, &ServerWidget::onStartServerClicked);
    connect(m_stopServerButton, &QPushButton::clicked, this, &ServerWidget::onStopServerClicked);

    connect(m_setPositionButton, &QPushButton::clicked,
            this, &ServerWidget::onSetPositionClicked,
        Qt::QueuedConnection);
    connect(m_resetButton, &QPushButton::clicked,
            this, &ServerWidget::onResetClicked,
        Qt::QueuedConnection);
    connect(m_startAntennaButton, &QPushButton::clicked,
            this, &ServerWidget::onStartAntennaClicked,
        Qt::QueuedConnection);
    connect(m_stopAntennaButton, &QPushButton::clicked,
            this, &ServerWidget::onStopAntennaClicked,
        Qt::QueuedConnection);

    connect(m_speedModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ServerWidget::onSpeedModeChanged,
        Qt::QueuedConnection);

    connect(m_antennaTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &ServerWidget::onAntennaTypeSelected);

    connect(m_loadConfigButton, &QPushButton::clicked, 
            this, &ServerWidget::onLoadConfigClicked,
        Qt::QueuedConnection);
    connect(m_saveConfigButton, &QPushButton::clicked, 
            this, &ServerWidget::onSaveConfigClicked,
        Qt::QueuedConnection);
    connect(m_addAntennaTypeButton, &QPushButton::clicked,
        this, &ServerWidget::onAddAntennaTypeClicked,
        Qt::QueuedConnection);

    updateUIState();
}

void ServerWidget::loadAntennaTypes()
{
    if (!m_manager) return;

    QStringList types = m_manager->getAntennaTypes();
    m_antennaTypeCombo->clear();
    m_antennaTypeCombo->addItems(types);

    // Если типов нет - показываем специальное сообщение
    if (types.isEmpty()) {
        m_antennaTypeCombo->addItem("No types available - add new type");
        m_antennaTypeCombo->setEnabled(false);

        // Также показываем предупреждение в логе
        appendLog("WARNING: No antenna types available. Please add a new type!", "orange");
    }
    else {
        m_antennaTypeCombo->setEnabled(true);
    }
}

void ServerWidget::updateUIState()
{
    bool serverRunning = m_isServerRunning;
    bool hasAntenna = m_manager && m_manager->getAntenna() != nullptr;

    m_startServerButton->setEnabled(!serverRunning);
    m_stopServerButton->setEnabled(serverRunning);
    m_portEdit->setEnabled(!serverRunning);

    // Элементы управления антенной доступны ТОЛЬКО если есть антенна
    m_antennaTypeCombo->setEnabled      (true);
    m_setPositionButton->setEnabled     (hasAntenna);
    m_resetButton->setEnabled           (hasAntenna);
    m_startAntennaButton->setEnabled    (hasAntenna);
    m_stopAntennaButton->setEnabled     (hasAntenna);
    m_speedModeCombo->setEnabled        (hasAntenna);
    m_loadConfigButton->setEnabled      (hasAntenna);
    m_saveConfigButton->setEnabled      (hasAntenna);
    m_addAntennaTypeButton->setEnabled  (true);

    if (serverRunning && hasAntenna) {
        m_runningStatusLabel->setText(m_manager->getAntenna()->isRunning() ? "RUNNING" : "STOPPED");
        m_runningStatusLabel->setStyleSheet(m_manager->getAntenna()->isRunning() ?
            "font-weight: bold; color: #4CAF50;" :
            "font-weight: bold; color: #f44336;");
    }
    else if (serverRunning && !hasAntenna) {
        m_runningStatusLabel->setText("NO ANTENNA");
        m_runningStatusLabel->setStyleSheet("font-weight: bold; color: #f44336;");
    }
}

void ServerWidget::appendLog(const QString& message, const QString& color)
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

void ServerWidget::appendRawData(const QString& direction, const QByteArray& data)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");

    // Преобразуем байты в шестнадцатеричный формат
    QString hexData;
    for (int i = 0; i < data.size(); ++i) {
        hexData += QString("%1 ").arg(static_cast<unsigned char>(data[i]), 2, 16, QChar('0')).toUpper();
    }

    // Также показываем ASCII представление
    QString strData;
    for (int i = 0; i < data.size(); ++i) {
        char ch = data[i];
        if (ch >= 32 && ch <= 126) {
            strData += ch;
        }
        else {
            strData += '.';
        }
    }

    QString formattedMsg = QString("[%1] %2 (%3 bytes):\n  HEX: %4\n  STR: %5\n")
        .arg(timestamp)
        .arg(direction)
        .arg(data.size())
        .arg(hexData.trimmed())
        .arg(strData);

    m_rawDataTextEdit->append(formattedMsg);

    QScrollBar* scrollbar = m_rawDataTextEdit->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

// ===== Слоты управления сервером =====

void ServerWidget::onStartServerClicked()
{
    if (m_isServerRunning) return;

    quint16 port = m_portEdit->text().toUInt();
    if (port == 0) {
        QMessageBox::warning(this, "Error", "Invalid port number");
        return;
    }

    if (m_manager && m_manager->start(port)) {
        updateUIState();

        m_serverStatusLabel->setText("RUNNING");
        m_serverStatusLabel->setStyleSheet("background-color: #4CAF50; color: white; padding: 5px; border-radius: 3px;");

        m_statusBar->showMessage("Server started on port " + QString::number(port), 2000);
    }
}

void ServerWidget::onStopServerClicked()
{
    if (m_manager) {
        m_manager->stop();
    }
}

// ===== Слоты управления антенной =====

void ServerWidget::onSetPositionClicked()
{
    if (!m_manager || !m_manager->getAntenna()) return;

    int azimuth = m_azimuthSpin->value();
    int elevation = m_elevationSpin->value();
    int polarization = m_polarizationSpin->value();

    IAntenn* antenn = m_manager->getAntenna();
    QMetaObject::invokeMethod(antenn, [antenn, azimuth, elevation, polarization]() {
        antenn->setPosition(azimuth, elevation, polarization);
        }, Qt::QueuedConnection);

    m_statusBar->showMessage(QString("Position set: AZ=%1 EL=%2 POL=%3")
        .arg(azimuth).arg(elevation).arg(polarization), 2000);
}

void ServerWidget::onResetClicked()
{
    if (!m_manager || !m_manager->getAntenna()) return;

    IAntenn* antenn = m_manager->getAntenna();
    QMetaObject::invokeMethod(antenn, [antenn]() {
        antenn->reset();
        }, Qt::QueuedConnection);

    m_statusBar->showMessage("Reset command sent", 2000);
}

void ServerWidget::onStartAntennaClicked()
{
    if (!m_manager || !m_manager->getAntenna()) return;

    IAntenn* antenn = m_manager->getAntenna();
    QMetaObject::invokeMethod(antenn, [antenn]() {
        antenn->start();
        }, Qt::QueuedConnection);

    m_statusBar->showMessage("Start command sent", 2000);
}

void ServerWidget::onStopAntennaClicked()
{
    if (!m_manager || !m_manager->getAntenna()) return;

    IAntenn* antenn = m_manager->getAntenna();
    QMetaObject::invokeMethod(antenn, [antenn]() {
        antenn->stop();
        }, Qt::QueuedConnection);

    m_statusBar->showMessage("Stop command sent", 2000);
}

void ServerWidget::onSpeedModeChanged(int index)
{
    if (!m_manager || !m_manager->getAntenna() || index < 0) return;

    IAntenn* antenn = m_manager->getAntenna();
    QMetaObject::invokeMethod(antenn, [antenn, index]() {
        antenn->setSpeedMode(index);
        }, Qt::QueuedConnection);
}

void ServerWidget::onAntennaTypeSelected(int index)
{
    if (!m_manager || index < 0) return;

    QString type = m_antennaTypeCombo->currentText();
    if (type.isEmpty()) return;

    if (!AntennaFactory::instance().getAvailableTypes().contains(type)) {
        appendLog("ERROR: Antenna type not available: " + type, "red");
        return;
    }

    // Проверяем, не выбран ли уже этот тип
    if (m_manager->getAntenna() && m_manager->getAntenna()->getAntennaType() == type) {
        return; // Уже выбран
    }

    // Пытаемся переключиться
    if (!m_manager->selectAntennaType(type)) {
        appendLog("CRITICAL: Failed to switch to antenna type: " + type, "red");
        QMessageBox::critical(this,
            "Antenna Switch Failed",
            QString("Failed to switch to antenna type '%1'.\n"
                "Check that config file exists and is valid.")
            .arg(type));
    }
    else {
        appendLog("Switched to antenna type: " + type, "green");
        m_statusBar->showMessage("Switched to antenna type: " + type, 2000);
    }
}

// ===== Конфигурация =====

void ServerWidget::onLoadConfigClicked()
{
    QString path = QFileDialog::getOpenFileName(this,
        "Load Antenna Config",
        m_configPathEdit->text(),
        "Config Files (*.ini *.conf);;All Files (*)");

    if (!path.isEmpty()) {
        m_configPathEdit->setText(path);

        if (m_manager && m_manager->getAntenna()) {
            IAntenn* antenn = m_manager->getAntenna();
            bool result;
            QMetaObject::invokeMethod(antenn, [antenn, path, &result]() {
                result = antenn->loadConfig(path);
                }, Qt::BlockingQueuedConnection);

            if (result) {
                m_statusBar->showMessage("Config loaded: " + path, 2000);
                appendLog("Config loaded: " + path, "green");
            }
            else {
                m_statusBar->showMessage("Failed to load config", 2000);
                appendLog("Failed to load config: " + path, "red");
            }
        }
    }
}

void ServerWidget::onSaveConfigClicked()
{
    QString path = QFileDialog::getSaveFileName(this,
        "Save Antenna Config",
        m_configPathEdit->text(),
        "Config Files (*.ini);;All Files (*)");

    if (!path.isEmpty()) {
        m_configPathEdit->setText(path);

        if (m_manager && m_manager->getAntenna()) {
            IAntenn* antenn = m_manager->getAntenna();
            bool result;
            QMetaObject::invokeMethod(antenn, [antenn, path, &result]() {
                result = antenn->saveConfig(path);
                }, Qt::BlockingQueuedConnection);

            if (result) {
                m_statusBar->showMessage("Config saved: " + path, 2000);
                appendLog("Config saved: " + path, "green");
            }
            else {
                m_statusBar->showMessage("Failed to save config", 2000);
                appendLog("Failed to save config: " + path, "red");
            }
        }
    }
}

void ServerWidget::onAddAntennaTypeClicked()
{
    QString type = m_newAntennaTypeEdit->text().trimmed();
    if (type.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please enter antenna type name");
        return;
    }

    // Проверяем, что тип уже не зарегистрирован
    if (AntennaFactory::instance().getAvailableTypes().contains(type)) {
        QMessageBox::warning(this, "Error",
            QString("Antenna type '%1' already exists").arg(type));
        return;
    }

    QString configPath = m_newAntennaConfigEdit->text().trimmed();
    if (configPath.isEmpty()) {
        configPath = QString("config/%1_config.ini").arg(type.toLower());
    }

    // ===== ПРОВЕРЯЕМ существование конфига =====
    if (!QFile::exists(configPath)) {
        QMessageBox::critical(this,
            "Config Not Found",
            QString("Config file '%1' does not exist."));
        return; // НЕ создаем конфиг, просто выходим
    }

    // Теперь конфиг существует - регистрируем тип
    AntennaFactory::instance().registerAntennaTypeWithDefaultCreator(type, configPath);

    // Сохраняем в общий список
    saveAntennaTypeToConfig(type, configPath);

    // Обновляем UI
    loadAntennaTypes();

    m_newAntennaTypeEdit->clear();
    m_newAntennaConfigEdit->clear();

    m_statusBar->showMessage("Antenna type added: " + type, 2000);
    appendLog("Antenna type added: " + type + " (config: " + configPath + ")", "green");
}

void ServerWidget::saveAntennaTypeToConfig(const QString& type, const QString& configPath)
{
    QSettings typeSettings("config/antenna_types.ini", QSettings::IniFormat);

    // Получаем текущее количество
    int count = typeSettings.value("Types/Count", 0).toInt();

    // Добавляем новый тип
    typeSettings.setValue(QString("Types/Type%1").arg(count), type);
    typeSettings.setValue(QString("Types/Config%1").arg(count), configPath);
    typeSettings.setValue("Types/Count", count + 1);

    typeSettings.sync();
}

// ===== Слоты сигналов менеджера =====

void ServerWidget::onServerStarted(quint16 port)
{
    m_isServerRunning = true;
    updateUIState();

    m_serverStatusLabel->setText("RUNNING");
    m_serverStatusLabel->setStyleSheet("background-color: #4CAF50; color: white; padding: 5px; border-radius: 3px;");

    if (m_statusTimerActive) {
        killTimer(m_statusTimerId);
    }

    m_statusTimerId = startTimer(STATUS_UPDATE_INTERVAL_MS);
    m_statusTimerActive = true;

    m_statusBar->showMessage("Server started on port " + QString::number(port), 2000);
    appendLog("Server started on port " + QString::number(port), "green");
}

void ServerWidget::onServerStopped()
{
    m_isServerRunning = false;
    updateUIState();

    m_serverStatusLabel->setText("STOPPED");
    m_serverStatusLabel->setStyleSheet("background-color: #f44336; color: white; padding: 5px; border-radius: 3px;");

    if (m_statusTimerActive) {
        killTimer(m_statusTimerId);
        m_statusTimerActive = false;
    }

    m_statusBar->showMessage("Server stopped", 2000);
    appendLog("Server stopped", "red");
}

void ServerWidget::onClientConnected()
{
    m_statusBar->showMessage("Client connected", 2000);
    appendLog("Client connected", "green");
}

void ServerWidget::onClientDisconnected()
{
    m_statusBar->showMessage("Client disconnected", 2000);
    appendLog("Client disconnected", "orange");
}

void ServerWidget::onInfo(const QString& message)
{
    appendLog("[INFO] " + message, "blue");
    m_statusBar->showMessage(message, 3000);
}

void ServerWidget::onWarning(const QString& message)
{
    appendLog("[WARN] " + message, "orange");
    m_statusBar->showMessage("WARNING: " + message, 3000);
}

void ServerWidget::onError(const QString& message)
{
    appendLog("[ERROR] " + message, "red");
    m_statusBar->showMessage("ERROR: " + message, 5000);
}

void ServerWidget::onLogRawData(const QString& direction, const QByteArray& data)
{
    appendRawData(direction, data);
}

void ServerWidget::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == m_statusTimerId) {
        if (!m_updateProgress) {
            m_updateProgress = true;
            QMetaObject::invokeMethod(this, [this]() {
                updateAntennaStatus();
                m_updateProgress = false;
                }, Qt::QueuedConnection);
        }
        updateAntennaStatus();
    }
}

void ServerWidget::updateAntennaStatus()
{
    qDebug() << "ServerWidget::updateAntennaStatus : ThreadID -> " << QThread::currentThreadId();
    if (!m_manager || !m_manager->getAntenna()) {
        // Показываем, что антенна не доступна
        m_azimuthStatusLabel->setText("N/A");
        m_elevationStatusLabel->setText("N/A");
        m_polarizationStatusLabel->setText("N/A");
        m_speedStatusLabel->setText("N/A");
        m_movingStatusLabel->setText("NO");
        m_runningStatusLabel->setText("NO ANTENNA");
        m_runningStatusLabel->setStyleSheet("font-weight: bold; color: #f44336;");
        m_uptimeStatusLabel->setText("0 seconds");

        m_azimuthLimitsLabel->setText("N/A");
        m_elevationLimitsLabel->setText("N/A");
        m_polarizationLimitsLabel->setText("N/A");
        m_speedLimitsLabel->setText("N/A");

        m_speedModeCombo->blockSignals(true);
        m_speedModeCombo->clear();
        m_speedModeCombo->addItem("No antenna");
        m_speedModeCombo->setEnabled(false);
        m_speedModeCombo->blockSignals(false);
        return;
    }

    // IAntenn* antenn = m_manager->getAntenna();
    AntennaStatus status = m_manager->getAntenna()->getStatus();
    AntennaConfig config = m_manager->getAntenna()->getConfig();
    //AntennaStatus status;
    //AntennaConfig config;

    //QMetaObject::invokeMethod(m_manager->getAntenna(), [this, &status, &config] {
    //    status = m_manager->getAntenna()->getStatus();
    //    config = m_manager->getAntenna()->getConfig();
    //    }, Qt::BlockingQueuedConnection);

    m_azimuthStatusLabel->setText(QString("%1 deg").arg(status.azimuth, 0, 'f', 2));
    m_elevationStatusLabel->setText(QString("%1 deg").arg(status.elevation, 0, 'f', 2));
    m_polarizationStatusLabel->setText(QString("%1 deg").arg(status.polarization, 0, 'f', 2));
    m_speedStatusLabel->setText(QString("%1 deg/s").arg(status.speed, 0, 'f', 2));

    m_azimuthLimitsLabel->setText(QString("%1 - %2 deg")
        .arg(config.limits.minAzimuth, 0, 'f', 1)
        .arg(config.limits.maxAzimuth, 0, 'f', 1));

    m_elevationLimitsLabel->setText(QString("%1 - %2 deg")
        .arg(config.limits.minElevation, 0, 'f', 1)
        .arg(config.limits.maxElevation, 0, 'f', 1));

    m_polarizationLimitsLabel->setText(QString("%1 - %2 deg")
        .arg(config.limits.minPolarization, 0, 'f', 1)
        .arg(config.limits.maxPolarization, 0, 'f', 1));

    m_speedLimitsLabel->setText(QString("%1 deg/s")
        .arg(config.limits.maxSpeed, 0, 'f', 1));

    m_movingStatusLabel->setText(status.isMoving ? "YES" : "NO");
    m_movingStatusLabel->setStyleSheet(status.isMoving ?
        "font-weight: bold; color: #FF9800;" :
        "font-weight: bold; color: #757575;");

    m_runningStatusLabel->setText(status.isRunning ? "RUNNING" : "STOPPED");
    m_runningStatusLabel->setStyleSheet(status.isRunning ?
        "font-weight: bold; color: #4CAF50;" :
        "font-weight: bold; color: #f44336;");

    m_uptimeStatusLabel->setText(QString("%1 sec").arg(status.uptimeSeconds));

    // ===== Обновляем комбобокс типа антенны =====
    updateAntennaTypeCombo(status.type);

    // ===== Обновляем список режимов скоростей =====
    updateSpeedModes(config, status.speedMode);
}

void ServerWidget::updateAntennaTypeCombo(const QString& currentType)
{
    // Блокируем сигналы, чтобы не вызывать onAntennaTypeSelected
    m_antennaTypeCombo->blockSignals(true);

    if (m_antennaTypeCombo->currentText() != currentType) {
    // Получаем все доступные типы
        QStringList allTypes = m_manager->getAntennaTypes();

        // Если список пустой - выходим
        if (allTypes.isEmpty()) {
            m_antennaTypeCombo->clear();
            m_antennaTypeCombo->addItem("No types available");
            m_antennaTypeCombo->setEnabled(false);
            m_antennaTypeCombo->blockSignals(false);
            return;
        }

        // Обновляем список
        m_antennaTypeCombo->clear();
        m_antennaTypeCombo->addItems(allTypes);
        m_antennaTypeCombo->setEnabled(true);

        // Выбираем текущий тип
        int index = m_antennaTypeCombo->findText(currentType);
        if (index >= 0) {
            m_antennaTypeCombo->setCurrentIndex(index);
        }
    }
    
    m_antennaTypeCombo->blockSignals(false);
}

void ServerWidget::updateSpeedModes(const AntennaConfig& config, int currentMode)
{
    if (m_speedModeCombo->currentIndex() < 0 || m_speedModeCombo->currentIndex() != currentMode) {
    // Блокируем сигналы, чтобы не вызывать onSpeedModeChanged
    m_speedModeCombo->blockSignals(true);
    m_speedModeCombo->clear();

    if (config.speedModes.isEmpty()) {
        m_speedModeCombo->addItem("No speed modes");
        m_speedModeCombo->setEnabled(false);
    }
    else {
        // Добавляем все режимы из конфига
        for (const SpeedMode& mode : config.speedModes) {
            QString itemText = QString("%1 (%2 deg/s)")
                .arg(mode.name)
                .arg(mode.speed, 0, 'f', 1);
            m_speedModeCombo->addItem(itemText, mode.id);
        }
        m_speedModeCombo->setEnabled(true);

        // Выбираем текущий режим
        if (currentMode >= 0 && currentMode < m_speedModeCombo->count()) {
            m_speedModeCombo->setCurrentIndex(currentMode);
        }
    }

    m_speedModeCombo->blockSignals(false);

    }
}
