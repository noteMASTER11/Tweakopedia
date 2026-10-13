#pragma once

#include "execution/SystemToolProtocol.h"

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QUuid>

class QLocalServer;
class QLocalSocket;

namespace tweakopedia::execution {

class ExecutorServer final : public QObject
{
    Q_OBJECT

public:
    explicit ExecutorServer(QObject* parent = nullptr);
    ~ExecutorServer() override;

    [[nodiscard]] bool listen(const QString& serverName, const QString& nonce);
    void setExpectedProcessId(qint64 processId);
    [[nodiscard]] QString errorString() const;
    [[nodiscard]] bool sendPlan(
        const QByteArray& encodedPlan,
        const QString& dataRoot,
        const QString& transactionDirectory);
    [[nodiscard]] bool sendRollback(
        const QUuid& transactionId,
        const QString& dataRoot,
        const QString& transactionDirectory);
    [[nodiscard]] bool sendToolRequest(const SystemToolRequest& request);

signals:
    void authenticated(qint64 processId);
    void progressReceived(int percent, const QString& message);
    void completed(const QJsonObject& result);
    void toolStageReceived(const SystemToolStageEvent& event);
    void toolOutputReceived(const SystemToolOutputEvent& event);
    void toolProgressReceived(const SystemToolProgressEvent& event);
    void toolResultReceived(const SystemToolResult& result);
    void clientRejected(const QString& code);
    void clientDisconnected();

private:
    void acceptConnections();
    void readMessages(QLocalSocket* socket);
    void handleMessage(QLocalSocket* socket, const QJsonObject& message);
    void reject(QLocalSocket* socket, const QString& code);
    [[nodiscard]] bool send(QLocalSocket* socket, const QJsonObject& message);

    QLocalServer* server_{};
    QLocalSocket* client_{};
    QByteArray input_;
    QJsonObject pendingHello_;
    QLocalSocket* pendingHelloSocket_{};
    QString nonce_;
    QString error_;
    qint64 expectedProcessId_{};
    bool claimed_{};
    bool authenticated_{};
    QUuid activeToolRequestId_;
    qint64 expectedToolSequence_{};
};

} // namespace tweakopedia::execution
