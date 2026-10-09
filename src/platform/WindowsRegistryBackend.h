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
    [[nodiscard]] RegistryWriteResult writeValue(
        const domain::RegistryLocation& location,
        const domain::RegistryValueSpec& value) override;
    [[nodiscard]] RegistryWriteResult writeRaw(
        const domain::RegistryLocation& location,
        quint32 nativeType,
        const QByteArray& rawValue) override;
    [[nodiscard]] RegistryWriteResult deleteValue(const domain::RegistryLocation& location) override;
    [[nodiscard]] RegistryTreeReadResult readTree(
        const domain::RegistryKeyLocation& location,
        const domain::RegistryTreeLimits& limits = {}) const override;
    [[nodiscard]] RegistryWriteResult createKey(
        const domain::RegistryKeyLocation& location) override;
    [[nodiscard]] RegistryWriteResult deleteTree(
        const domain::RegistryKeyLocation& location) override;
    [[nodiscard]] RegistryWriteResult restoreTree(
        const domain::RegistryTreeSnapshot& snapshot) override;
};

} // namespace tweakopedia::platform
