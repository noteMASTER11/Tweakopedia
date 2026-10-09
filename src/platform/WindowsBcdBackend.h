#pragma once

#include "platform/IBcdBackend.h"

namespace tweakopedia::platform {

class WindowsBcdBackend final : public IBcdBackend
{
public:
    BcdReadResult read(const domain::BcdElementSpec& spec) const override;
    BcdMutationResult set(
        const domain::BcdElementSpec& spec, const domain::BcdValue& value) override;
    BcdMutationResult remove(const domain::BcdElementSpec& spec) override;
};

} // namespace tweakopedia::platform
