#pragma once

#include "execution/DismOutputParser.h"
#include "execution/IDismProcessRunner.h"
#include "execution/SystemToolLogWriter.h"

#include <QObject>
#include <QVector>

namespace tweakopedia::execution {

class SystemIntegrityWorkflow final : public QObject
{
    Q_OBJECT

public:
    SystemIntegrityWorkflow(
        IDismProcessRunner& runner,
        QString logsRoot,
        QObject* parent = nullptr);

    [[nodiscard]] bool start(const SystemToolRequest& request);
    [[nodiscard]] bool active() const noexcept;

signals:
    void stageChanged(const SystemToolStageEvent& event);
    void outputReady(const QUuid& requestId, const QString& text);
    void progressChanged(const SystemToolProgressEvent& event);
    void finished(const SystemToolResult& result);

private:
    struct Command {
        SystemToolStage stage;
        QStringList arguments;
    };

    void runNext();
    void consumeOutput(const QByteArray& bytes);
    void commandFinished(DismProcessResult result);
    void emitDisplayLine(QString text);
    void flushDisplayLine();
    void complete(SystemToolResult result);

    IDismProcessRunner* runner_{};
    QString logsRoot_;
    SystemToolLogWriter log_;
    DismOutputParser parser_;
    SystemToolRequest request_;
    QVector<Command> commands_;
    qsizetype commandIndex_{};
    QString displayLine_;
    bool pendingCarriageReturn_{};
    int lastProgress_{-1};
    bool active_{};
};

} // namespace tweakopedia::execution
