// tcpantenna.h
#ifndef TCPANTENNA_H
#define TCPANTENNA_H

#include "iantenn.h"
#include <QTcpSocket>
#include <QTimer>
#include <QMutex>
#include <QAtomicInt>
#include <QDateTime>
#include <QDataStream>

class TcpAntenna : public IAntenn
{
    Q_OBJECT
public:
    explicit TcpAntenna(QObject* parent = nullptr);
    ~TcpAntenna();

    // IAntenn interface
    void setAzimuth(double azimuth)     override;
    void setElevation(double elevation) override;
    void setSpeed(double speed)         override;

    double getAzimuth()   const override { return m_azimuth; }
    double getElevation() const override { return m_elevation; }
    double getSpeed()     const override { return m_movementSpeed; }

    QByteArray getStatus()      override;

    QByteArray execCMD(const QByteArray& request)  override;
    ParsedCMD parsingToCMD(const QByteArray& data) override;

    // Методы для работы с сетью
    bool connectToServer(const QString& address, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;

    // Запуск/остановка
    void start();
    void stop();

signals:
    void MsgReceived(const QByteArray& data);

private slots:
    // Слоты сокета
    void onReadyRead();
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);

    // Таймеры движения
    void onMovementTimer();

private:
    // Команды антенны
    QByteArray cmdSetAzimuth(const QList<QByteArray>& params);
    QByteArray cmdSetElevation(const QList<QByteArray>& params);
    QByteArray cmdSetSpeed(const QList<QByteArray>& params);
    QByteArray cmdMoveAzimuth(const QList<QByteArray>& params);
    QByteArray cmdMoveElevation(const QList<QByteArray>& params);
    QByteArray cmdCalibrate();
    QByteArray cmdStatus();
    QByteArray cmdStop();
    QByteArray cmdHelp();

    // Вспомогательные методы
    bool parseDouble(const QByteArray& data, double& value) const;
    bool isCommandValid(const ParsedCMD& cmd, int expectedParams = 0) const;
    QByteArray createErrorResponse(const QString& error) const;
    QByteArray createSuccessResponse(const QString& message) const;
    QString formatStatus() const;

    // Движение антенны
    void processMovement();

    // Отправка данных
    void sendData(const QByteArray& data);

private:
    // Сокет для связи
    QTcpSocket* m_socket;
    QString m_serverAddress;
    quint16 m_serverPort;
    bool m_isConnected;

    // Буфер для чтения с QDataStream
    quint16 m_nextBlockSize;
    QByteArray m_readBuffer;

    // Параметры антенны
    double m_azimuth;          // Текущий азимут (0-360)
    double m_elevation;        // Текущий угол места (-90 до +90)
    double m_targetAzimuth;    // Целевой азимут
    double m_targetElevation;  // Целевой угол места
    double m_movementSpeed;    // Скорость движения (градусов/сек)

    // Состояние
    bool m_isMoving;
    bool m_isCalibrated;
    QAtomicInt m_isRunning;

    // Таймер движения
    QTimer* m_movementTimer;

    // Защита данных
    mutable QMutex m_dataMutex;

    // Статистика
    qint64 m_totalCommands;
    qint64 m_successCommands;
    qint64 m_failedCommands;
    QDateTime m_startTime;

    // Константы
    static constexpr double MIN_AZIMUTH = 0.0;
    static constexpr double MAX_AZIMUTH = 360.0;
    static constexpr double MIN_ELEVATION = -90.0;
    static constexpr double MAX_ELEVATION = 90.0;
    static constexpr double DEFAULT_SPEED = 10.0;
    static constexpr int MOVEMENT_INTERVAL_MS = 50;
    static constexpr int MAX_MESSAGE_SIZE = 10 * 1024 * 1024; // 10 MB
    static constexpr int DATA_STREAM_VERSION = QDataStream::Qt_5_15;
};

#endif // TCPANTENNA_H