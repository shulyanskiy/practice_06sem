#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTextEdit>
#include <QGroupBox>
#include <QGridLayout>
#include <QStatusBar>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>

class EmuManager;
struct AntennaConfig;

class ServerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ServerWidget(QWidget* parent = nullptr);
    ~ServerWidget();

private slots:
    // Управление сервером
    void onStartServerClicked();
    void onStopServerClicked();

    // Управление антенной
    void onSetPositionClicked();
    void onResetClicked();
    void onStartAntennaClicked();
    void onStopAntennaClicked();
    void onSpeedModeChanged(int index);
    void onAntennaTypeSelected(int index);

    // Конфигурация
    void onLoadConfigClicked();
    void onSaveConfigClicked();
    void onAddAntennaTypeClicked();

    // Сигналы от менеджера
    void onServerStarted(quint16 port);
    void onServerStopped();
    void onClientConnected();
    void onClientDisconnected();
    void onInfo(const QString& message);
    void onWarning(const QString& message);
    void onError(const QString& message);
    void onLogRawData(const QString& direction, const QByteArray& data);

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    void setupUI();
    void updateUIState();
    void appendLog(const QString& message, const QString& color = "");
    void appendRawData(const QString& direction, const QByteArray& data);
    void loadAntennaTypes();
    void updateAntennaStatus();
    void updateAntennaTypeCombo(const QString& currentType);
    void updateSpeedModes(const AntennaConfig& config, int currentMode);
    void saveAntennaTypeToConfig(const QString& type, const QString& configPath);


private:
    // Элементы управления
    QLineEdit* m_portEdit;
    QPushButton* m_startServerButton;
    QPushButton* m_stopServerButton;
    QLabel* m_serverStatusLabel;

    // Управление антенной
    QComboBox* m_antennaTypeCombo;
    QSpinBox* m_azimuthSpin;
    QSpinBox* m_elevationSpin;
    QSpinBox* m_polarizationSpin;
    QPushButton* m_setPositionButton;
    QPushButton* m_resetButton;
    QPushButton* m_startAntennaButton;
    QPushButton* m_stopAntennaButton;
    QComboBox* m_speedModeCombo;

    // Статус антенны
    QLabel* m_azimuthStatusLabel;
    QLabel* m_elevationStatusLabel;
    QLabel* m_polarizationStatusLabel;
    QLabel* m_speedStatusLabel;
    QLabel* m_movingStatusLabel;
    QLabel* m_runningStatusLabel;
    QLabel* m_uptimeStatusLabel;
    // Ограничения антенны
    QLabel* m_azimuthLimitsLabel;
    QLabel* m_elevationLimitsLabel;
    QLabel* m_polarizationLimitsLabel;
    QLabel* m_speedLimitsLabel;

    // Таймер для статуса
    int m_statusTimerId;
    bool m_statusTimerActive;
    bool m_updateProgress;

    // Конфигурация
    QLineEdit* m_configPathEdit;
    QPushButton* m_loadConfigButton;
    QPushButton* m_saveConfigButton;
    QLineEdit* m_newAntennaTypeEdit;
    QLineEdit* m_newAntennaConfigEdit;
    QPushButton* m_addAntennaTypeButton;

    // Логирование
    QTabWidget* m_logTabs;
    QTextEdit* m_logTextEdit;
    QTextEdit* m_rawDataTextEdit;

    // Статусная строка
    QStatusBar* m_statusBar;

    // Менеджер
    EmuManager* m_manager;
    bool m_isServerRunning;

    static constexpr int DATA_STREAM_VERSION = QDataStream::Qt_5_15;
    static constexpr int STATUS_UPDATE_INTERVAL_MS = 500; // 500ms
};
