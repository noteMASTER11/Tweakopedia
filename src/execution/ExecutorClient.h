#pragma once

#include "execution/SystemToolProtocol.h"

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QUuid>

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
    [[nodiscard]] bool sendToolStage(const SystemToolStageEvent& event);
    [[nodiscard]] bool sendToolOutput(const QUuid& requestId, const QString& text);
    [[nodiscard]] bool sendToolProgress(const SystemToolProgressEvent& event);
    [[nodiscard]] bool sendToolResult(const SystemToolResult& result);

signals:
    void authenticated();
    void planReceived(
        const QByteArray& encodedPlan,
        const QString& dataRoot,
        const QString& transactionDirectory);
    void rollbackReceived(
        const QUuid& transactionId,
        const QString& dataRoot,
        const QString& transactionDirectory);
    void resultAcknowledged();
    void toolRequestReceived(const SystemToolRequest& request);
    void toolResultAcknowledged();
    void failed(const QString& code);

private:
    void readMessages();
    void handleMessage(const QJsonObject& message);
    [[nodiscard]] bool send(const QJsonObject& message);

    QLocalSocket* socket_{};
    QByteArray input_;
    QString nonce_;
    qint64 processId_{};
    QUuid activeToolRequestId_;
    qint64 nextToolSequence_{};
};

} // namespace tweakopedia::execution
