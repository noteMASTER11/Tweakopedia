#pragma once

#include "execution/IDismProcessRunner.h"
#include "execution/SystemToolLogWriter.h"

#include <QObject>
#include <QStringConverter>

namespace tweakopedia::execution {

class FirewallRuleRemovalWorkflow final : public QObject
{
    Q_OBJECT

public:
    FirewallRuleRemovalWorkflow(
        IDismProcessRunner& runner,
        QString logsRoot,
        QObject* parent = nullptr);

    [[nodiscard]] bool start(const SystemToolRequest& request);
    [[nodiscard]] bool active() const noexcept;

signals:
    void stageChanged(const SystemToolStageEvent& event);
    void outputReady(const QUuid& requestId, const QString& text);
    void finished(const SystemToolResult& result);

private:
    void consumeOutput(const QByteArray& bytes);
    void processFinished(DismProcessResult result);
    void complete(SystemToolResult result);

    IDismProcessRunner* runner_{};
    QString logsRoot_;
    SystemToolLogWriter log_;
    SystemToolRequest request_;
    QString backupPath_;
    QStringDecoder decoder_{QStringConverter::Utf8};
    bool active_{};
};

} // namespace tweakopedia::execution
