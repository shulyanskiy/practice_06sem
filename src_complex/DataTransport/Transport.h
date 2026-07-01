// itransport.h
#ifndef ITRANSPORT_H
#define ITRANSPORT_H

#include <QObject>
#include <QByteArray>
#include <QDataStream>

class ITransport : public QObject
{
    Q_OBJECT
public:
    explicit ITransport(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~ITransport() = default;

    // ===== Управление подключениями =====

    // Запуск сервера для ожидания подключения сервера
    virtual bool startServerListener(quint16 port) = 0;

    // Подключение к антенне (активное)
    // virtual bool connectToAntenna(const QString& address, quint16 port) = 0;

    // Отключение
    virtual void disconnectServer() = 0;
    virtual void disconnectAntenna() = 0;

    // Проверка состояния
    virtual bool isServerConnected() const = 0;
    virtual bool isAntennaConnected() const = 0;

    // ===== Отправка данных =====

    // Отправка конкретному получателю
    virtual void sendToServer(const QByteArray& data) = 0;
    virtual void sendToAntenna(const QByteArray& data) = 0;

    // Отправка всем подключенным
    virtual void sendToAll(const QByteArray& data) = 0;

    // ===== Управление очередью =====
    virtual void startProcessing() = 0;
    virtual void stopProcessing() = 0;
    virtual int queueSize() const = 0;

signals:
    // ===== Данные от получателей =====
    void dataFromServer(const QByteArray& data);
    void dataFromAntenna(const QByteArray& data);

    // ===== События подключения =====
    void serverConnected();
    void serverDisconnected();
    void antennaConnected();
    void antennaDisconnected();

    // ===== Уведомления =====
    void msgOccurred(const QString msg);
    void warningOccurred(const QString warning);
    void errorOccurred(const QString error);
};

#endif // ITRANSPORT_H