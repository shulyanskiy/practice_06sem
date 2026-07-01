// iantenn.h
#ifndef IANTENN_H
#define IANTENN_H

#include <QObject>
#include <QByteArray>
#include <QList>
#include <QString>

class IAntenn : public QObject
{
    Q_OBJECT
public:
    explicit IAntenn(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~IAntenn() = default;

    // Структура для хранения распарсенной команды
    struct ParsedCMD {
        QString name;
        QList<QByteArray> params;

        bool isValid() const { return !name.isEmpty(); }
        bool hasParams() const { return !params.isEmpty(); }
        int paramCount() const { return params.size(); }
    };

    // Основные методы управления антенной
    virtual void setAzimuth(double azimuth) = 0;
    virtual void setElevation(double elevation) = 0;
    virtual void setSpeed(double speed) = 0;

    virtual double getAzimuth() const = 0;
    virtual double getElevation() const = 0;
    virtual double getSpeed() const = 0;

    // Метод выполнения команды
    virtual QByteArray execCMD(const QByteArray& request) = 0;
    virtual ParsedCMD parsingToCMD(const QByteArray& data) = 0;
    virtual QByteArray getStatus() = 0;

signals:
    // === Сигналы состояния ===
    void azimuthChanged(double newAzimuth);
    void elevationChanged(double newElevation);
    void speedChanged(double newSpeed);
    void positionReached(double azimuth, double elevation);
    void statusChanged(const QString& status);
    void commandExecuted(const QString& command, bool success);

    // === Сигналы подключения ===
    void connected();
    void disconnected();

    // === Сигналы логирования ===
    void infoOccurred(const QString& message);
    void warningOccurred(const QString& message);
    void errorOccurred(const QString& message);
};


#endif // IANTENN_H