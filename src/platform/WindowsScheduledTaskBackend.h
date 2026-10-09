#pragma once

#include "platform/IScheduledTaskBackend.h"

namespace tweakopedia::platform {

class WindowsScheduledTaskBackend final : public IScheduledTaskBackend
{
public:
    [[nodiscard]] ScheduledTaskReadResult read(
        const domain::ScheduledTaskLocation& location) override;
    [[nodiscard]] ScheduledTaskWriteResult setEnabled(
        const domain::ScheduledTaskLocation& location, bool enabled) override;
};

} // namespace tweakopedia::platform
