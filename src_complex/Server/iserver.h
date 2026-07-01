// iserver.h
#ifndef ISERVER_H
#define ISERVER_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QList>

class IServer : public QObject
{
    Q_OBJECT
public:
    explicit IServer(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~IServer() = default;

    // Структура для хранения распарсенной команды
    struct ParsedCMD {
        QString name;
        QList<QByteArray> params;

        bool isValid() const { return !name.isEmpty(); }
        bool hasParams() const { return !params.isEmpty(); }
        int paramCount() const { return params.size(); }
    };

    ParsedCMD parsingToCMD(const QByteArray& data)
    {
        ParsedCMD result;

        // Удаляем пробелы в начале и конце
        QByteArray trimmed = data.trimmed();

        if (trimmed.isEmpty()) {
            return result;  // Пустая строка - невалидная команда
        }

        // Ищем первый пробел (разделитель между командой и параметрами)
        int spacePos = trimmed.indexOf(' ');

        if (spacePos == -1) {
            // Нет пробела - значит это только команда без параметров
            result.name = QString(trimmed).toUpper();
            return result;
        }

        // Извлекаем имя команды (до пробела)
        result.name = QString(trimmed.left(spacePos)).toUpper();

        // Извлекаем параметры (после пробела)
        QByteArray paramParts = trimmed.mid(spacePos + 1);

        // Разбиваем параметры по запятой
        for (const QByteArray& param : paramParts.split(',')) {
            QByteArray trimmedParam = param.trimmed();
            if (!trimmedParam.isEmpty()) {
                result.params.append(trimmedParam);
            }
        }

        return result;
    }

    // Основные методы сервера
    virtual bool start(quint16 port) = 0;              // Порт для клиента
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;
    virtual bool hasClient() const = 0;

    // Подключение к транспорту
    virtual bool connectToTransport(const QString& address, quint16 port) = 0;
    virtual void disconnectFromTransport() = 0;
    virtual bool isTransportConnected() const = 0;

    // ===== Отправка данных =====
    virtual void sendToClient(const QByteArray& data) = 0;
    virtual void sendToTransport(const QByteArray& data) = 0;
    virtual void sendToAll(const QByteArray& data) = 0;

signals:
    // === Сигналы состояния ===
    void serverStarted(quint16 port);
    void serverStopped();
    void clientConnected();
    void clientDisconnected();
    void transportConnected();
    void transportDisconnected();

    // === Сигналы данных ===
    void dataFromClient(const QByteArray& data);
    void dataFromTransport(const QByteArray& data);
    void commandExecuted(const QString& command, bool success);

    // === Сигналы логирования ===
    void infoOccurred(const QString& message);
    void warningOccurred(const QString& message);
    void errorOccurred(const QString& message);
};

#endif // ISERVER_H