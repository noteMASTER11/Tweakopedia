#pragma once

#include "platform/IRegistryBackend.h"

namespace tweakopedia::platform {

class WindowsRegistryBackend final : public IRegistryBackend
{
public:
    [[nodiscard]] RegistryReadResult read(const domain::RegistryLocation& location) const override;
    [[nodiscard]] RegistryWriteResult writeDword(
        const domain::RegistryLocation& location,
        quint32 value) override;
    [[nodiscard]] RegistryWriteResult writeRaw(
        const domain::RegistryLocation& location,
        quint32 nativeType,
        const QByteArray& rawValue) override;
    [[nodiscard]] RegistryWriteResult deleteValue(const domain::RegistryLocation& location) override;
};

} // namespace tweakopedia::platform
