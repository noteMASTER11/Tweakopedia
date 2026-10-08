#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>

class QLocalSocket;

namespace tweakopedia::execution {

class ExecutorClient final : public QObject
{
    Q_OBJECT

public:
    explicit ExecutorClient(QObject* parent = nullptr);
    ~ExecutorClient() override;

    void connectToServer(const QString& serverName, const QString& nonce, qint64 processId = 0);
    [[nodiscard]] bool sendProgress(int percent, const QString& message);
    [[nodiscard]] bool sendResult(const QJsonObject& result);

signals:
    void authenticated();
    void planReceived(
        const QByteArray& encodedPlan,
        const QString& dataRoot,
        const QString& transactionDirectory);
    void failed(const QString& code);

private:
    void readMessages();
    void handleMessage(const QJsonObject& message);
    [[nodiscard]] bool send(const QJsonObject& message);

    QLocalSocket* socket_{};
    QByteArray input_;
    QString nonce_;
    qint64 processId_{};
};

} // namespace tweakopedia::execution
