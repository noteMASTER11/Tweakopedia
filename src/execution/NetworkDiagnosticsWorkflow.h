#pragma once

#include "execution/IDismProcessRunner.h"
#include "execution/SystemToolLogWriter.h"

#include <QJsonArray>
#include <QObject>
#include <QStringConverter>

namespace tweakopedia::execution {

class NetworkDiagnosticsWorkflow final : public QObject
{
    Q_OBJECT

public:
    NetworkDiagnosticsWorkflow(
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
    void consumeText(const QString& text);
    void processLine(QString line);
    void processMarker(const QJsonObject& marker);
    void processFinished(DismProcessResult result);
    void complete(SystemToolResult result);
    [[nodiscard]] QJsonObject buildDetails() const;

    IDismProcessRunner* runner_{};
    QString logsRoot_;
    SystemToolLogWriter log_;
    SystemToolRequest request_;
    QString lineBuffer_;
    QStringDecoder decoder_{QStringConverter::Utf8};
    QJsonArray checks_;
    bool active_{};
};

} // namespace tweakopedia::execution
