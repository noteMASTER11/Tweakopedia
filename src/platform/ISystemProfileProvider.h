#pragma once

#include "domain/SystemProfile.h"

namespace tweakopedia::platform {

class ISystemProfileProvider
{
public:
    virtual ~ISystemProfileProvider() = default;
    [[nodiscard]] virtual domain::SystemProfile current() const = 0;
};

} // namespace tweakopedia::platform
