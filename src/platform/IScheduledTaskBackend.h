#pragma once

#include "domain/ScheduledTask.h"

#include <QString>

namespace tweakopedia::platform {

struct ScheduledTaskReadResult {
    bool success{};
    bool missing{};
    bool enabled{};
    QString error;

    [[nodiscard]] static ScheduledTaskReadResult present(bool enabled)
    {
        return {.success = true, .enabled = enabled};
    }
    [[nodiscard]] static ScheduledTaskReadResult missingTask()
    {
        return {.missing = true};
    }
    [[nodiscard]] static ScheduledTaskReadResult failed(QString error)
    {
        return {.error = std::move(error)};
    }
};

struct ScheduledTaskWriteResult {
    bool success{};
    QString error;
};

class IScheduledTaskBackend
{
public:
    virtual ~IScheduledTaskBackend() = default;
    [[nodiscard]] virtual ScheduledTaskReadResult read(
        const domain::ScheduledTaskLocation& location) = 0;
    [[nodiscard]] virtual ScheduledTaskWriteResult setEnabled(
        const domain::ScheduledTaskLocation& location, bool enabled) = 0;
};

} // namespace tweakopedia::platform
