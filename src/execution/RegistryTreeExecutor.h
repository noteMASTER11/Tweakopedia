#pragma once

#include "execution/RegistryDwordExecutor.h"

namespace tweakopedia::execution {

struct RegistryTreeCaptureResult : RegistryExecutionResult {
    domain::RegistryTreeSnapshot snapshot;
};

class RegistryTreeExecutor final
{
public:
    explicit RegistryTreeExecutor(platform::IRegistryBackend& backend);

    [[nodiscard]] RegistryTreeCaptureResult capture(
        const domain::RegistryKeyLocation& location,
        const domain::RegistryTreeLimits& limits = {}) const;
    [[nodiscard]] RegistryExecutionResult compareBefore(
        const domain::RegistryTreeSnapshot& snapshot,
        const QByteArray& expectedFingerprint) const;
    [[nodiscard]] RegistryExecutionResult apply(const domain::CreateRegistryKeyOperation& operation);
    [[nodiscard]] RegistryExecutionResult apply(const domain::DeleteRegistryTreeOperation& operation);
    [[nodiscard]] RegistryExecutionResult restore(const domain::RegistryTreeSnapshot& snapshot);

    [[nodiscard]] static QByteArray fingerprint(const domain::RegistryTreeSnapshot& snapshot);

private:
    platform::IRegistryBackend* backend_{};
};

} // namespace tweakopedia::execution
