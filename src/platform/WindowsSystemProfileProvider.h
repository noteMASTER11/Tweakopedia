#pragma once

#include "platform/ISystemProfileProvider.h"

namespace tweakopedia::platform {

class WindowsSystemProfileProvider final : public ISystemProfileProvider
{
public:
    [[nodiscard]] domain::SystemProfile current() const override;
};

} // namespace tweakopedia::platform
