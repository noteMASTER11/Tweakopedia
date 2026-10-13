#pragma once

#include "app/ISystemIntegrityDesktopServices.h"
#include "execution/ISystemToolRunner.h"
#include "platform/ILocalVolumeProvider.h"

#include <QObject>
#include <QVariantList>

namespace tweakopedia::app {

class FileSystemCheckController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList volumes READ volumes NOTIFY changed)
    Q_PROPERTY(int selectedVolumeIndex READ selectedVolumeIndex NOTIFY changed)
    Q_PROPERTY(bool hasVolumes READ hasVolumes NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString stageText READ stageText NOTIFY changed)
    Q_PROPERTY(QString outputText READ outputText NOTIFY changed)
    Q_PROPERTY(QString resultTitle READ resultTitle NOTIFY changed)
    Q_PROPERTY(QString resultDescription READ resultDescription NOTIFY changed)
    Q_PROPERTY(QString errorCode READ errorCode NOTIFY changed)
    Q_PROPERTY(int exitCode READ exitCode NOTIFY changed)
    Q_PROPERTY(QString sessionLogPath READ sessionLogPath NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canCheck READ canCheck NOTIFY changed)

public:
    explicit FileSystemCheckController(
        platform::ILocalVolumeProvider& volumeProvider,
        execution::ISystemToolRunner& runner,
        ISystemIntegrityDesktopServices& desktopServices,
        QObject* parent = nullptr);

    [[nodiscard]] QVariantList volumes() const;
    [[nodiscard]] int selectedVolumeIndex() const;
    [[nodiscard]] bool hasVolumes() const;
    [[nodiscard]] QString state() const;
    [[nodiscard]] QString stageText() const;
    [[nodiscard]] QString outputText() const;
    [[nodiscard]] QString resultTitle() const;
    [[nodiscard]] QString resultDescription() const;
    [[nodiscard]] QString errorCode() const;
    [[nodiscard]] int exitCode() const;
    [[nodiscard]] QString sessionLogPath() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] bool canCheck() const;

    Q_INVOKABLE bool refreshVolumes();
    Q_INVOKABLE bool setSelectedVolumeIndex(int index);
    Q_INVOKABLE bool startCheck();
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
    void trimOutput();
    void rebuildVolumeModel(const QString& preferredTarget = {});

    platform::ILocalVolumeProvider* volumeProvider_{};
    execution::ISystemToolRunner* runner_{};
    ISystemIntegrityDesktopServices* desktopServices_{};
    QVector<platform::LocalVolume> volumeData_;
    QVariantList volumes_;
    int selectedVolumeIndex_{-1};
    QString state_{QStringLiteral("idle")};
    QString stageText_;
    QString outputText_;
    QString resultTitle_;
    QString resultDescription_;
    QString errorCode_;
    int exitCode_{-1};
    QString sessionLogPath_;
    bool busy_{};
    bool pendingNewSession_{};
};

} // namespace tweakopedia::app
