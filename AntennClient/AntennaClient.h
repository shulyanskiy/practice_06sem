#pragma once

#include <QWidget>
#include <QTcpSocket>
#include <QDataStream>
#include <QSettings>
#include <QPointer>
#include <QGridLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QGroupBox>
#include <QStatusBar>

class AntennaClient : public QWidget
{
    Q_OBJECT

public:
    explicit AntennaClient(QWidget* parent = nullptr);
    ~AntennaClient();

protected:
    // Встроенный таймер QWidget
    void timerEvent(QTimerEvent* event) override;

private slots:
    // Подключение
    void onConnectButtonClicked();
    void onDisconnectButtonClicked();

    // Управление антенной
    void onApplyPositionButtonClicked();
    void onSpeedModeChanged(int index);
    void onCalibrateButtonClicked();
    void onStartButtonClicked();
    void onStopButtonClicked();

    // Слайдеры
    void onAzimuthSliderChanged(int value);
    void onElevationSliderChanged(int value);
    void onPolarizationSliderChanged(int value);

    // Сокет
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);

    // Выбор типа антенны
    void onAntennaTypeSelected(int index);

private:
    void setupUI();
    void sendCommand(const QString& command);
    void sendData(const QByteArray& data);
    void processResponse(const QByteArray& data);
    void updateConnectionStatus(bool connected);
    void updateUIState();
    void loadSettings();
    void saveSettings();
    void parseAntennStatusResponse(const QString& response);
    void parseAntennaTypesResponse(const QString& response);
    void updateAntennaTypeCombo(const QString& currentType);
    void updateAntennaStatusDisplay(double azimuth, double elevation, double polarization,
        double speed, int speedMode, bool isMoving, bool isCalibrated);

    // Запрос статуса (вызывается по таймеру)
    void requestStatus();

private:
    // ===== Элементы управления =====
    // Подключение
    QLineEdit* m_hostEdit;
    QLineEdit* m_portEdit;
    QPushButton* m_connectButton;
    QPushButton* m_disconnectButton;
    QLabel* m_connectionStatusLabel;

    // Выбор типа антенны
    QComboBox* m_antennaTypeCombo;

    // Управление антенной (слайдеры для установки значений)
    QSlider* m_azimuthControlSlider;
    QSlider* m_elevationControlSlider;
    QSlider* m_polarizationControlSlider;
    QLabel* m_azimuthControlLabel;
    QLabel* m_elevationControlLabel;
    QLabel* m_polarizationControlLabel;
    QPushButton* m_applyPositionButton;
    QComboBox* m_speedModeCombo;

    // Кнопки управления
    QPushButton* m_calibrateButton;
    QPushButton* m_startButton;
    QPushButton* m_stopButton;

    // ===== Отображение статуса (только для чтения) =====
    QLabel* m_azimuthStatusLabel;
    QLabel* m_elevationStatusLabel;
    QLabel* m_polarizationStatusLabel;
    QLabel* m_speedStatusLabel;
    QLabel* m_speedModeStatusLabel;
    QLabel* m_movingStatusLabel;
    QLabel* m_calibratedStatusLabel;
    QLabel* m_runningStatusLabel;
    QLabel* m_uptimeStatusLabel;

    // Метки для лимитов
    QLabel* m_azimuthLimitsLabel;
    QLabel* m_elevationLimitsLabel;
    QLabel* m_polarizationLimitsLabel;
    QLabel* m_speedLimitsLabel;

    // Статусная строка
    QStatusBar* m_statusBar;
    QLabel* m_connectionInfoLabel;

    // Сокет
    QTcpSocket* m_socket;
    QString m_host;
    quint16 m_port;
    bool m_isConnected;

    // Встроенный таймер QWidget
    int m_timerId;
    bool m_timerActive;

    // Буфер для приема данных
    struct SocketBuffer {
        quint16 nextBlockSize = 0;
        QByteArray rawData;
        void reset() {
            nextBlockSize = 0;
            rawData.clear();
        }
    };
    SocketBuffer m_buffer;

    QPointer<QSettings> m_settings;

    static constexpr int DATA_STREAM_VERSION = QDataStream::Qt_5_15;
    static constexpr int MAX_MESSAGE_SIZE = 10 * 1024 * 1024;
    static constexpr int STATUS_UPDATE_INTERVAL_MS = 500; // 500ms
};