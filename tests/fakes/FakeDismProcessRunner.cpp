#include "fakes/FakeDismProcessRunner.h"

bool FakeDismProcessRunner::start(
    const QString& program,
    const QStringList& arguments,
    OutputCallback output,
    FinishedCallback finished)
{
    if (!acceptStart) return false;
    invocations.append({program, arguments, std::move(output), std::move(finished)});
    return true;
}

void FakeDismProcessRunner::emitOutput(int index, const QByteArray& bytes)
{
    invocations.at(index).output(bytes);
}

void FakeDismProcessRunner::finish(
    int index,
    tweakopedia::execution::DismProcessResult result)
{
    invocations.at(index).finished(std::move(result));
}
