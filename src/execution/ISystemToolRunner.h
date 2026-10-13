#pragma once

#include "execution/SystemToolProtocol.h"

#include <QObject>

namespace tweakopedia::execution {

class ISystemToolRunner : public QObject
{
    Q_OBJECT

public:
    explicit ISystemToolRunner(QObject* parent = nullptr) : QObject(parent) {}
    ~ISystemToolRunner() override = default;

    virtual bool start(SystemToolOperation operation) = 0;

signals:
    void launchCancelled(const QString& message);
    void stageChanged(const SystemToolStageEvent& event);
    void outputReceived(const SystemToolOutputEvent& event);
    void progressChanged(const SystemToolProgressEvent& event);
    void finished(const SystemToolResult& result);
    void failed(const QString& code, const QString& message);
};

} // namespace tweakopedia::execution
