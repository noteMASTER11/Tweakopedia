#pragma once

#include "execution/RegistrySnapshot.h"

namespace tweakopedia::execution {

struct RegistryExecutionResult {
    bool success{};
    QString code;
    QString message;
};

struct RegistryCaptureResult : RegistryExecutionResult {
    RegistrySnapshot snapshot;
};

class RegistryDwordExecutor final
{
public:
    explicit RegistryDwordExecutor(platform::IRegistryBackend& backend);

    [[nodiscard]] RegistryCaptureResult capture(const domain::RegistryLocation& location) const;
    [[nodiscard]] RegistryExecutionResult compareBefore(
        const RegistrySnapshot& snapshot,
        const QByteArray& expectedFingerprint) const;
    [[nodiscard]] RegistryExecutionResult apply(const domain::SetRegistryDwordOperation& operation);
    [[nodiscard]] RegistryExecutionResult restore(const RegistrySnapshot& snapshot);

private:
    platform::IRegistryBackend* backend_{};
};

} // namespace tweakopedia::execution
