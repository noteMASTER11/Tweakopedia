#pragma once

#include "execution/IDismProcessRunner.h"

#include <QVector>

class FakeDismProcessRunner final : public tweakopedia::execution::IDismProcessRunner
{
public:
    struct Invocation {
        QString program;
        QStringList arguments;
        OutputCallback output;
        FinishedCallback finished;
    };

    bool start(
        const QString& program,
        const QStringList& arguments,
        OutputCallback output,
        FinishedCallback finished) override;

    void emitOutput(int index, const QByteArray& bytes);
    void finish(int index, tweakopedia::execution::DismProcessResult result);

    QVector<Invocation> invocations;
    bool acceptStart{true};
};
