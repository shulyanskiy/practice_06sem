#pragma once
#include <QWidget>
#include <QTcpSocket>
#include <QDataStream>
#include <QSettings>
#include <QPointer>
#include <QGridLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QGroupBox>
#include <QStatusBar>
#include <QTableWidget>

class AntennaClient : public QWidget
{
    Q_OBJECT

public:
    explicit AntennaClient(QWidget* parent = nullptr);
    ~AntennaClient();

private slots:
    // Подключение
    void onConnectButtonClicked();
    void onDisconnectButtonClicked();

    // Команды
    void onSendCommandButtonClicked();
    void onRequestStatusButtonClicked();
    void onRequestAntennStatusButtonClicked();

    // Управление антенной
    void onCalibrateButtonClicked();
    void onStartButtonClicked();
    void onStopButtonClicked();
    void onSpeedModeChanged(int index);
    void onApplyAzimuthButtonClicked();
    void onApplyElevationButtonClicked();
    void onApplyPolarizationButtonClicked();

    // Конфигурация
    void onLoadConfigButtonClicked();
    void onSaveConfigButtonClicked();

    // Слайдеры (только управление)
    void onAzimuthSliderChanged(int value);
    void onElevationSliderChanged(int value);
    void onPolarizationSliderChanged(int value);

    // Сокет
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    void setupUI();
    void sendCommand(const QString& command);
    void sendData(const QByteArray& data);
    void processResponse(const QByteArray& data);
    void updateConnectionStatus(bool connected);
    void updateUIState();
    void loadSettings();
    void saveSettings();
    void parseStatusResponse(const QString& response);
    void parseAntennStatusResponse(const QString& response);
    void appendLog(const QString& message, const QString& color = "");
    void updateAntennaStatusDisplay(double azimuth, double elevation, double polarization,
        double speed, int speedMode, bool isMoving, bool isCalibrated);

private:
    // ===== Элементы управления =====
    // Подключение
    QLineEdit* m_hostEdit;
    QLineEdit* m_portEdit;
    QPushButton* m_connectButton;
    QPushButton* m_disconnectButton;
    QLabel* m_connectionStatusLabel;

    // Управление антенной (слайдеры для установки значений)
    QSlider* m_azimuthControlSlider;
    QSlider* m_elevationControlSlider;
    QSlider* m_polarizationControlSlider;
    QLabel* m_azimuthControlLabel;
    QLabel* m_elevationControlLabel;
    QLabel* m_polarizationControlLabel;
    QPushButton* m_applyAzimuthButton;
    QPushButton* m_applyElevationButton;
    QPushButton* m_applyPolarizationButton;
    QComboBox* m_speedModeCombo;

    // Кнопки управления
    QPushButton* m_calibrateButton;
    QPushButton* m_startButton;
    QPushButton* m_stopButton;

    // Команды
    QLineEdit* m_commandEdit;
    QPushButton* m_sendButton;
    QPushButton* m_statusButton;
    QPushButton* m_antennStatusButton;

    // Конфигурация
    QLineEdit* m_configPathEdit;
    QPushButton* m_loadConfigButton;
    QPushButton* m_saveConfigButton;

    // ===== Отображение статуса (только для чтения) =====
    QLabel* m_azimuthStatusLabel;      // Текущий азимут (только чтение)
    QLabel* m_elevationStatusLabel;    // Текущий угол места (только чтение)
    QLabel* m_polarizationStatusLabel; // Текущая поляризация (только чтение)
    QLabel* m_speedStatusLabel;        // Текущая скорость (только чтение)
    QLabel* m_speedModeStatusLabel;    // Текущий режим скорости (только чтение)
    QLabel* m_movingStatusLabel;       // Движется ли антенна (только чтение)
    QLabel* m_calibratedStatusLabel;   // Откалибрована ли (только чтение)
    QLabel* m_runningStatusLabel;      // Запущена ли антенна (только чтение)
    QLabel* m_uptimeStatusLabel;       // Время работы (только чтение)

    // Метки для лемитов
    QLabel* m_azimuthLimitsLabel;      // Лимиты азимута
    QLabel* m_elevationLimitsLabel;    // Лимиты угла места
    QLabel* m_polarizationLimitsLabel; // Лимиты поляризации
    QLabel* m_speedLimitsLabel;        // Лимит скорости

    // Статусная строка
    QLabel* m_clientsLabel;
    QLabel* m_antennaRunningLabel;

    // Лог
    QTextEdit* m_logTextEdit;
    QStatusBar* m_statusBar;

    // Сокет
    QTcpSocket* m_socket;
    QString m_host;
    quint16 m_port;
    bool m_isConnected;

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
};
