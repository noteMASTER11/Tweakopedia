#pragma once

#include "app/ISystemIntegrityDesktopServices.h"
#include "app/TerminalOutputBuffer.h"
#include "execution/ISystemToolRunner.h"

#include <QObject>

namespace tweakopedia::app {

class SystemIntegrityController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString stageText READ stageText NOTIFY changed)
    Q_PROPERTY(int progress READ progress NOTIFY changed)
    Q_PROPERTY(bool progressKnown READ progressKnown NOTIFY changed)
    Q_PROPERTY(QString outputText READ outputText NOTIFY changed)
    Q_PROPERTY(QString resultTitle READ resultTitle NOTIFY changed)
    Q_PROPERTY(QString resultDescription READ resultDescription NOTIFY changed)
    Q_PROPERTY(QString errorCode READ errorCode NOTIFY changed)
    Q_PROPERTY(int exitCode READ exitCode NOTIFY changed)
    Q_PROPERTY(QString sessionLogPath READ sessionLogPath NOTIFY changed)
    Q_PROPERTY(QString dismLogPath READ dismLogPath CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canScan READ canScan NOTIFY changed)
    Q_PROPERTY(bool canRestore READ canRestore NOTIFY changed)

public:
    explicit SystemIntegrityController(
        execution::ISystemToolRunner& runner,
        ISystemIntegrityDesktopServices& desktopServices,
        QObject* parent = nullptr);

    [[nodiscard]] QString state() const;
    [[nodiscard]] QString stageText() const;
    [[nodiscard]] int progress() const;
    [[nodiscard]] bool progressKnown() const;
    [[nodiscard]] QString outputText() const;
    [[nodiscard]] QString resultTitle() const;
    [[nodiscard]] QString resultDescription() const;
    [[nodiscard]] QString errorCode() const;
    [[nodiscard]] int exitCode() const;
    [[nodiscard]] QString sessionLogPath() const;
    [[nodiscard]] QString dismLogPath() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] bool canScan() const;
    [[nodiscard]] bool canRestore() const;

    Q_INVOKABLE bool startScan();
    Q_INVOKABLE bool startRestore();
    Q_INVOKABLE bool copyOutput();
    Q_INVOKABLE bool openDismLog();

signals:
    void changed();

private:
    bool start(execution::SystemToolOperation operation);
    void beginConfirmedSession();
    void handleStage(const execution::SystemToolStageEvent& event);
    void handleOutput(const execution::SystemToolOutputEvent& event);
    void handleProgress(const execution::SystemToolProgressEvent& event);
    void handleResult(const execution::SystemToolResult& result);
    void handleFailure(const QString& code, const QString& message);

    execution::ISystemToolRunner* runner_{};
    ISystemIntegrityDesktopServices* desktopServices_{};
    execution::SystemToolOperation operation_{execution::SystemToolOperation::Scan};
    QString state_{QStringLiteral("idle")};
    QString stageText_;
    int progress_{};
    bool progressKnown_{};
    TerminalOutputBuffer output_;
    QString resultTitle_;
    QString resultDescription_;
    QString errorCode_;
    int exitCode_{-1};
    QString sessionLogPath_;
    bool busy_{};
    bool pendingNewSession_{};
};

} // namespace tweakopedia::app
