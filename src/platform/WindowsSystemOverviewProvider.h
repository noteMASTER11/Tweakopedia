#pragma once

#include "domain/SystemOverview.h"

namespace tweakopedia::platform {

class WindowsSystemOverviewProvider final
{
public:
    [[nodiscard]] domain::SystemOverviewSnapshot collect() const;
};

} // namespace tweakopedia::platform
