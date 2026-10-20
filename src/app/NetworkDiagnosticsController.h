#pragma once

#include "app/ISystemIntegrityDesktopServices.h"
#include "app/TerminalOutputBuffer.h"
#include "execution/ISystemToolRunner.h"

#include <QObject>
#include <QUuid>
#include <QVariantList>
#include <QStringList>

namespace tweakopedia::app {

class NetworkDiagnosticsController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString stageText READ stageText NOTIFY changed)
    Q_PROPERTY(QString outputText READ outputText NOTIFY changed)
    Q_PROPERTY(QString resultTitle READ resultTitle NOTIFY changed)
    Q_PROPERTY(QString resultDescription READ resultDescription NOTIFY changed)
    Q_PROPERTY(QString errorCode READ errorCode NOTIFY changed)
    Q_PROPERTY(QString sessionLogPath READ sessionLogPath NOTIFY changed)
    Q_PROPERTY(QVariantList checks READ checks NOTIFY changed)
    Q_PROPERTY(int okCount READ okCount NOTIFY changed)
    Q_PROPERTY(int attentionCount READ attentionCount NOTIFY changed)
    Q_PROPERTY(int unavailableCount READ unavailableCount NOTIFY changed)
    Q_PROPERTY(bool hasAttention READ hasAttention NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canStart READ canStart NOTIFY changed)
    Q_PROPERTY(QString actionState READ actionState NOTIFY changed)
    Q_PROPERTY(QString actionMessage READ actionMessage NOTIFY changed)
    Q_PROPERTY(QString activeRuleKey READ activeRuleKey NOTIFY changed)
    Q_PROPERTY(QStringList removedRuleKeys READ removedRuleKeys NOTIFY changed)

public:
    explicit NetworkDiagnosticsController(
        execution::ISystemToolRunner& runner,
        ISystemIntegrityDesktopServices& desktopServices,
        QObject* parent = nullptr);

    [[nodiscard]] QString state() const;
    [[nodiscard]] QString stageText() const;
    [[nodiscard]] QString outputText() const;
    [[nodiscard]] QString resultTitle() const;
    [[nodiscard]] QString resultDescription() const;
    [[nodiscard]] QString errorCode() const;
    [[nodiscard]] QString sessionLogPath() const;
    [[nodiscard]] QVariantList checks() const;
    [[nodiscard]] int okCount() const;
    [[nodiscard]] int attentionCount() const;
    [[nodiscard]] int unavailableCount() const;
    [[nodiscard]] bool hasAttention() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] bool canStart() const;
    [[nodiscard]] QString actionState() const;
    [[nodiscard]] QString actionMessage() const;
    [[nodiscard]] QString activeRuleKey() const;
    [[nodiscard]] QStringList removedRuleKeys() const;

    Q_INVOKABLE bool startDiagnostics();
    Q_INVOKABLE bool removeFirewallRule(const QString& key);
    Q_INVOKABLE bool copyOutput();
    Q_INVOKABLE bool openSessionLog();

signals:
    void changed();

private:
    void beginConfirmedSession();
    void handleStage(const execution::SystemToolStageEvent& event);
    void handleOutput(const execution::SystemToolOutputEvent& event);
    void handleResult(const execution::SystemToolResult& result);
    void handleFailure(const QString& code, const QString& message);
    [[nodiscard]] bool hasFirewallRule(const QString& key) const;
    void clearActionState();

    enum class ActiveOperation { None, Diagnostics, RemoveFirewallRule };

    execution::ISystemToolRunner* runner_{};
    ISystemIntegrityDesktopServices* desktopServices_{};
    QString state_{QStringLiteral("idle")};
    QString stageText_;
    TerminalOutputBuffer output_;
    QString resultTitle_;
    QString resultDescription_;
    QString errorCode_;
    QString sessionLogPath_;
    QVariantList checks_;
    int okCount_{};
    int attentionCount_{};
    int unavailableCount_{};
    bool busy_{};
    bool pendingNewSession_{};
    QUuid activeRequestId_;
    ActiveOperation activeOperation_{ActiveOperation::None};
    QString actionState_{QStringLiteral("idle")};
    QString actionMessage_;
    QString activeRuleKey_;
    QStringList removedRuleKeys_;
};

} // namespace tweakopedia::app
