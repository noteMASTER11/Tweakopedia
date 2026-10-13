#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <functional>

namespace tweakopedia::execution {

enum class DismProcessTermination { Normal, Crashed, FailedToStart };

struct DismProcessResult {
    DismProcessTermination termination{DismProcessTermination::FailedToStart};
    int exitCode{-1};
    QString error;
};

class IDismProcessRunner
{
public:
    using OutputCallback = std::function<void(const QByteArray&)>;
    using FinishedCallback = std::function<void(DismProcessResult)>;

    virtual ~IDismProcessRunner() = default;
    virtual bool start(
        const QString& program,
        const QStringList& arguments,
        OutputCallback output,
        FinishedCallback finished) = 0;
};

} // namespace tweakopedia::execution
