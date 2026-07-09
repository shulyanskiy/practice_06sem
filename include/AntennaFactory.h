#pragma once
#include "include/emulator.h"
#include <QMap>
#include <QString>
#include <functional>

class AntennaFactory
{
public:
    using AntennaCreator = std::function<IAntenn* (const QString&, QObject*)>;

    static AntennaFactory& instance();

    void registerAntennaType(const QString& type, AntennaCreator creator, const QString& configPath = "");
    void registerAntennaTypeWithDefaultCreator(const QString& type, const QString& configPath = "");

    QStringList getAvailableTypes() const;
    IAntenn* createAntenna(const QString& type, QObject* parent = nullptr);

    QString getConfigPathForType(const QString& type) const;

private:
    AntennaFactory() = default;
    ~AntennaFactory() = default;
    AntennaFactory(const AntennaFactory&) = delete;
    AntennaFactory& operator=(const AntennaFactory&) = delete;

    struct AntennaTypeInfo {
        QString type;
        QString configPath;
        AntennaCreator creator;
    };

    // Стандартный создатель для MyAntenn
    static IAntenn* defaultCreator(const QString& type, QObject* parent);

    QMap<QString, AntennaTypeInfo> m_types;
};
