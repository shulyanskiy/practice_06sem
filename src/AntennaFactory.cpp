#include "include/AntennaFactory.h"
#include "include/MyAntenn.h"
#include <QFile>
#include <QDebug>

AntennaFactory& AntennaFactory::instance()
{
    static AntennaFactory factory;
    return factory;
}

IAntenn* AntennaFactory::defaultCreator(const QString& type, QObject* parent)
{
    // Здесь мы создаем конкретный экземпляр MyAntenn,
    // но фабрика предоставляет эту функциональность как "стандартную"
    return new MyAntenn(type, parent);
}

void AntennaFactory::registerAntennaType(const QString& type, AntennaCreator creator, const QString& configPath)
{
    if (!m_types.contains(type)) {
        AntennaTypeInfo info;
        info.type = type;
        info.configPath = configPath.isEmpty() ?
            QString("config/antenna_%1.ini").arg(type.toLower()) :
            configPath;
        info.creator = creator;
        m_types[type] = info;
    }
}

void AntennaFactory::registerAntennaTypeWithDefaultCreator(const QString& type, const QString& configPath)
{
    registerAntennaType(type, defaultCreator, configPath);
}

QStringList AntennaFactory::getAvailableTypes() const
{
    return m_types.keys();
}

IAntenn* AntennaFactory::createAntenna(const QString& type, QObject* parent)
{
    if (type.isEmpty())
        return nullptr;

    auto it = m_types.find(type);
    if (it == m_types.end())
        return nullptr;

    const AntennaTypeInfo& info = it.value();

    // Создаем антенну
    IAntenn* antenn = info.creator(type, parent);
    if (!antenn) {
        qCritical() << "AntennaFactory::createAntenna: Failed to create antenna of type:" << type;
        return nullptr;
    }

    // Загружаем конфиг для этого типа
    if (!antenn->loadConfig(info.configPath)) {
        qCritical() << "AntennaFactory::createAntenna: Failed to load config for type:" << type
            << "path:" << info.configPath;
        delete antenn;
        return nullptr;
    }

    // Применяем конфиг
    // Добавляем метод applyConfig() в интерфейс или вызываем через dynamic_cast
    // Лучше добавить виртуальный метод в IAntenn

    return antenn;
}

QString AntennaFactory::getConfigPathForType(const QString& type) const
{
    if (m_types.contains(type)) {
        return m_types[type].configPath;
    }
    return QString();
}
