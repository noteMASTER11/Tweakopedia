#pragma once

#include "domain/ScheduledTask.h"
#include "platform/IScheduledTaskBackend.h"

#include <QByteArray>
#include <QString>

namespace tweakopedia::execution {

struct ScheduledTaskExecutionResult {
    bool success{};
    QString code;
    QString message;
};

struct ScheduledTaskCaptureResult : ScheduledTaskExecutionResult {
    domain::ScheduledTaskSnapshot snapshot;
};

class ScheduledTaskExecutor final
{
public:
    explicit ScheduledTaskExecutor(platform::IScheduledTaskBackend& backend);

    [[nodiscard]] ScheduledTaskCaptureResult capture(
        const domain::ScheduledTaskLocation& location) const;
    [[nodiscard]] ScheduledTaskExecutionResult compareBefore(
        const domain::ScheduledTaskSnapshot& snapshot,
        const QByteArray& expectedFingerprint) const;
    [[nodiscard]] ScheduledTaskExecutionResult apply(
        const domain::SetScheduledTaskEnabledOperation& operation) const;
    [[nodiscard]] ScheduledTaskExecutionResult restore(
        const domain::ScheduledTaskSnapshot& snapshot) const;
    [[nodiscard]] static QByteArray fingerprint(
        const domain::ScheduledTaskSnapshot& snapshot);

private:
    platform::IScheduledTaskBackend* backend_{};
};

} // namespace tweakopedia::execution
