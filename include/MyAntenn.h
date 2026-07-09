#pragma once
#include "emulator.h"
#include <QDateTime>
#include <QMutex>
#include <QSettings>
#include <QAtomicInt>

class MyAntenn : public IAntenn
{
    Q_OBJECT
public:
    explicit MyAntenn(const QString& type = "Default", QObject* parent = nullptr);
    ~MyAntenn();

    // Управление (все void)
    void setAzimuth(double azimuth) override;
    void setElevation(double elevation) override;
    void setPolarization(double polarization) override;
    void setPosition(double azimuth, double elevation, double polarization) override;
    void setSpeedMode(int mode) override;
    void reset() override;
    void start() override;
    void stop() override;
    bool isRunning() const override;

    // Геттеры
    AntennaStatus getStatus() const override;
    AntennaConfig getConfig() const override;
    QString getAntennaType() const override { return m_antennaType; }

    // Конфигурация
    void applyConfig() override;
    bool loadConfig(const QString& configPath) override;
    bool saveConfig(const QString& configPath) override;
    QString getConfigPath() const override;

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    void processMovement();
    void loadLimits();

private:
    // Тип антенны
    QString m_antennaType;

    // Параметры антенны
    double m_azimuth;
    double m_elevation;
    double m_polarization;
    double m_targetAzimuth;
    double m_targetElevation;
    double m_targetPolarization;
    double m_currentSpeed;
    int m_speedMode;

    // Ограничения
    double m_minAzimuth;
    double m_maxAzimuth;
    double m_minElevation;
    double m_maxElevation;
    double m_minPolarization;
    double m_maxPolarization;
    double m_maxSpeed;

    // Состояние
    bool m_isMoving;
    QAtomicInt m_isRunning;
    bool m_isCalibrated;

    // Встроенный таймер
    int m_timerId;
    bool m_timerActive;

    // Защита данных
    mutable QMutex m_dataMutex;

    // Статистика
    QDateTime m_startTime;

    // Конфигурация
    QString m_configPath;
    QSettings* m_settings;

    // Скорости по режимам
    QList<double> m_speedModes;
    QList<QString> m_speedModeNames;


    double m_defaultAzimuth;
    double m_defaultElevation;
    double m_defaultPolarization;
    int m_defaultSpeedMode;

    static constexpr int MOVEMENT_INTERVAL_MS = 50;
};

