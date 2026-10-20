#pragma once

#include "execution/IDismProcessRunner.h"
#include "execution/SystemToolLogWriter.h"

#include <QByteArray>
#include <QObject>

namespace tweakopedia::execution {

class FileSystemCheckWorkflow final : public QObject
{
    Q_OBJECT

public:
    FileSystemCheckWorkflow(
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
    [[nodiscard]] QString decodeOutput(const QByteArray& bytes, bool flush = false);
    void processFinished(DismProcessResult result);
    void complete(SystemToolResult result);

    IDismProcessRunner* runner_{};
    QString logsRoot_;
    SystemToolLogWriter log_;
    SystemToolRequest request_;
    QByteArray pendingOutputBytes_;
    bool active_{};
};

} // namespace tweakopedia::execution
