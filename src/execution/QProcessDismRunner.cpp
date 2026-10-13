#include "execution/QProcessDismRunner.h"

#include <QProcess>

namespace tweakopedia::execution {

QProcessDismRunner::QProcessDismRunner(QObject* parent)
    : QObject(parent)
{
}

bool QProcessDismRunner::start(
    const QString& program,
    const QStringList& arguments,
    OutputCallback output,
    FinishedCallback finished)
{
    if (process_) return false;
    completed_ = false;
    output_ = std::move(output);
    finished_ = std::move(finished);
    process_ = new QProcess(this);
    process_->setProcessChannelMode(QProcess::MergedChannels);
    connect(process_, &QProcess::readyRead, this, [this] {
        const auto bytes = process_->readAll();
        if (!bytes.isEmpty() && output_) output_(bytes);
    });
    connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            complete({DismProcessTermination::FailedToStart, -1, process_->errorString()});
        }
    });
    connect(process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus status) {
        const auto bytes = process_->readAll();
        if (!bytes.isEmpty() && output_) output_(bytes);
        complete({status == QProcess::NormalExit
                      ? DismProcessTermination::Normal
                      : DismProcessTermination::Crashed,
                  exitCode,
                  status == QProcess::NormalExit ? QString{} : process_->errorString()});
    });
    process_->setProgram(program);
    process_->setArguments(arguments);
    process_->start(QIODevice::ReadOnly);
    return true;
}

void QProcessDismRunner::complete(DismProcessResult result)
{
    if (completed_) return;
    completed_ = true;
    auto finished = std::move(finished_);
    output_ = {};
    auto* process = process_;
    process_ = nullptr;
    if (process) process->deleteLater();
    if (finished) finished(std::move(result));
}

} // namespace tweakopedia::execution
