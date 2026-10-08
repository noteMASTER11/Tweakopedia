#pragma once

#include "persistence/TransactionFiles.h"
#include "persistence/TransactionRepository.h"
#include "planning/ExecutionPlan.h"
#include "platform/IRegistryBackend.h"
#include "platform/WindowsAppxPackageProvider.h"

namespace tweakopedia::execution {

struct TransactionRunResult {
    bool success{};
    bool rolledBack{};
    QString code;
    QString message;
};

class TransactionRunner final
{
public:
    TransactionRunner(
        platform::IRegistryBackend& backend,
        persistence::TransactionFiles& files,
        persistence::TransactionRepository& repository,
        platform::IAppxPackageBackend* appxBackend = nullptr);

    [[nodiscard]] TransactionRunResult run(const planning::ExecutionPlan& plan);

private:
    platform::IRegistryBackend* backend_{};
    platform::IAppxPackageBackend* appxBackend_{};
    persistence::TransactionFiles* files_{};
    persistence::TransactionRepository* repository_{};
};

} // namespace tweakopedia::execution
