#pragma once

#include "persistence/TransactionFiles.h"
#include "persistence/TransactionRepository.h"
#include "planning/ExecutionPlan.h"
#include "platform/IRegistryBackend.h"

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
        persistence::TransactionRepository& repository);

    [[nodiscard]] TransactionRunResult run(const planning::ExecutionPlan& plan);

private:
    platform::IRegistryBackend* backend_{};
    persistence::TransactionFiles* files_{};
    persistence::TransactionRepository* repository_{};
};

} // namespace tweakopedia::execution
