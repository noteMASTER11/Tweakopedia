#pragma once

#include "execution/IDismProcessRunner.h"

#include <QObject>

class QProcess;

namespace tweakopedia::execution {

class QProcessDismRunner final : public QObject, public IDismProcessRunner
{
public:
    explicit QProcessDismRunner(QObject* parent = nullptr);

    bool start(
        const QString& program,
        const QStringList& arguments,
        OutputCallback output,
        FinishedCallback finished) override;

private:
    void complete(DismProcessResult result);

    QProcess* process_{};
    OutputCallback output_;
    FinishedCallback finished_;
    bool completed_{};
};

} // namespace tweakopedia::execution
