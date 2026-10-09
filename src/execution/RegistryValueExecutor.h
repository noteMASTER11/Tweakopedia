#pragma once

#include "execution/RegistryDwordExecutor.h"

namespace tweakopedia::execution {

class RegistryValueExecutor final
{
public:
    explicit RegistryValueExecutor(platform::IRegistryBackend& backend);

    [[nodiscard]] RegistryCaptureResult capture(const domain::RegistryLocation& location) const;
    [[nodiscard]] RegistryExecutionResult compareBefore(
        const RegistrySnapshot& snapshot,
        const QByteArray& expectedFingerprint) const;
    [[nodiscard]] RegistryExecutionResult apply(const domain::SetRegistryValueOperation& operation);
    [[nodiscard]] RegistryExecutionResult apply(const domain::DeleteRegistryValueOperation& operation);
    [[nodiscard]] RegistryExecutionResult restore(const RegistrySnapshot& snapshot);

private:
    platform::IRegistryBackend* backend_{};
};

} // namespace tweakopedia::execution
