#ifndef EMULATOR_H
#define EMULATOR_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QAtomicInt>
#include <QDataStream>
#include <QTimer>
#include <QSettings>
#include <QMutex>
#include <QObject>
#include <QStringList>
#include <QByteArray>
#include <QMap>
#include <QList>
#include <QPointer>

// Структура для лимитов антенны
struct AntennaLimits {
    double minAzimuth;
    double maxAzimuth;
    double minElevation;
    double maxElevation;
    double minPolarization;
    double maxPolarization;
    double maxSpeed;

    AntennaLimits()
        : minAzimuth(0.0)
        , maxAzimuth(360.0)
        , minElevation(-90.0)
        , maxElevation(90.0)
        , minPolarization(-180.0)
        , maxPolarization(180.0)
        , maxSpeed(50.0)
    {
    }
};

// Структура для режима скорости
struct SpeedMode {
    int id;
    QString name;
    double speed;

    SpeedMode() : id(0), name(""), speed(0.0) {}
    SpeedMode(int i, const QString& n, double s) : id(i), name(n), speed(s) {}
};

// Структура для полной конфигурации антенны
struct AntennaConfig {
    AntennaLimits limits;
    QList<SpeedMode> speedModes;
    double defaultAzimuth;
    double defaultElevation;
    double defaultPolarization;
    int defaultSpeedMode;

    AntennaConfig()
        : defaultAzimuth(0.0)
        , defaultElevation(0.0)
        , defaultPolarization(0.0)
        , defaultSpeedMode(0)
    {
    }
};

// Структура для статуса антенны
struct AntennaStatus {
    QString type;
    double azimuth;
    double elevation;
    double polarization;
    double targetAzimuth;
    double targetElevation;
    double targetPolarization;
    double speed;
    int speedMode;
    bool isMoving;
    bool isCalibrated;
    bool isRunning;
    qint64 uptimeSeconds;

    AntennaStatus()
        : azimuth(0.0)
        , type("")
        , elevation(0.0)
        , polarization(0.0)
        , targetAzimuth(0.0)
        , targetElevation(0.0)
        , targetPolarization(0.0)
        , speed(0.0)
        , speedMode(0)
        , isMoving(false)
        , isCalibrated(false)
        , isRunning(false)
        , uptimeSeconds(0)
    {
    }
};

// Интерфейс антенны
class IAntenn : public QObject
{
    Q_OBJECT
public:
    explicit IAntenn(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~IAntenn() = default;

    // Управление (все void, кроме getStatus)
    virtual void setAzimuth(double azimuth) = 0;
    virtual void setElevation(double elevation) = 0;
    virtual void setPolarization(double polarization) = 0;
    virtual void setPosition(double azimuth, double elevation, double polarization) = 0;
    virtual void setSpeedMode(int mode) = 0;
    virtual void reset() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;

    // Геттеры
    virtual AntennaStatus getStatus() const = 0;
    virtual AntennaConfig getConfig() const = 0;
    virtual QString getAntennaType() const = 0;

    // Конфигурация
    virtual void applyConfig() = 0;
    virtual bool loadConfig(const QString& configPath) = 0;
    virtual bool saveConfig(const QString& configPath) = 0;
    virtual QString getConfigPath() const = 0;

signals:
    void infoOccurred(const QString& message);
    void warningOccurred(const QString& message);
    void errorOccurred(const QString& message);
    void movementStarted();
    void movementStopped();
    void resetDone();
};



// Менеджер эмулятора
class EmuManager : public QObject
{
    Q_OBJECT
public:
    explicit EmuManager(QObject* parent = nullptr);
    ~EmuManager();

    // Управление антенной
    void setAntenna(IAntenn* antenn);
    IAntenn* getAntenna() const { return m_p_antenn; }

    // Управление сервером
    bool start(quint16 port);
    void stop();
    bool isRunning() const;
    bool hasClient() const;
    quint16 getPort() const { return m_serverPort; }

    // Отправка данных клиенту
    void sendToClient(const QByteArray& data);

    // Обработка команд
    QByteArray execCMD(const QByteArray& request);

    // Получение списка типов антенн
    QStringList getAntennaTypes() const;
    bool selectAntennaType(const QString& type);

signals:
    void serverStarted(quint16 port);
    void serverStopped();
    void clientConnected();
    void clientDisconnected();
    void dataFromClient(const QByteArray& data);
    void commandExecuted(const QString& command, bool success);

    // Сигналы логирования
    void infoOccurred(const QString& message);
    void warningOccurred(const QString& message);
    void errorOccurred(const QString& message);
    void logRawData(const QString& direction, const QByteArray& data);

private slots:
    void onClientConnected();
    void onClientReadyRead();
    void onClientDisconnected();
    void onClientError(QAbstractSocket::SocketError error);

private:
    struct ParsedCMD {
        QString name;
        QList<QByteArray> params;
        bool isAntennaCommand;

        ParsedCMD() : isAntennaCommand(false) {}
        ParsedCMD(
            const QString& cmdName,
            const QList<QByteArray>& cmdParams = QList<QByteArray>(),
            bool antenna = false
        )
            : name(cmdName), params(cmdParams), isAntennaCommand(antenna)
        {
        }

        bool isValid() const { return !name.isEmpty(); }
        bool hasParams() const { return !params.isEmpty(); }
        int paramCount() const { return params.size(); }
    };

    struct SocketBuffer {
        quint16 nextBlockSize = 0;
        QByteArray rawData;
        void reset() {
            nextBlockSize = 0;
            rawData.clear();
        }
    };

    ParsedCMD parsingToCMD(const QByteArray& data);
    QByteArray processManagerCommand(const ParsedCMD& parsed);
    QByteArray processAntennaCommand(const ParsedCMD& parsed);
    QByteArray cmdHelp();
    QByteArray cmdStatus();
    QByteArray cmdDisconnect();
    QByteArray cmdAntennStatus();
    QByteArray cmdGetAntennaTypes();
    QByteArray cmdSelectAntennaType(const QString& type);
    bool isCommandValid(const ParsedCMD& cmd, int expectedParams = 0) const;
    QByteArray createErrorResponse(const QString& error) const;
    QByteArray createSuccessResponse(const QString& message) const;
    void sendData(QTcpSocket* socket, const QByteArray& data);
    void processSocketData(QTcpSocket* socket);
    void cleanupClient();
    void loadAntennaTypes();

    void createAntennaInThread(const QString& type);

private:
    QTcpServer* m_server;
    quint16 m_serverPort;
    QAtomicInt m_isRunning;

    // Клиент
    QTcpSocket* m_clientSocket;
    QAtomicInt m_hasClient;
    SocketBuffer m_clientBuffer;

    QPointer<IAntenn> m_p_antenn;  
    QPointer<QThread> m_antennaThread;
    QAtomicInt m_isSwitching;

    static const QStringList MANAGER_COMMANDS;
    static constexpr int MAX_MESSAGE_SIZE = 10 * 1024 * 1024;
    static constexpr int DATA_STREAM_VERSION = QDataStream::Qt_5_15;
};

#endif // EMULATOR_H

