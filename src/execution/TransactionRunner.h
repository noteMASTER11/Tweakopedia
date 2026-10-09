#pragma once

#include "persistence/TransactionFiles.h"
#include "persistence/TransactionRepository.h"
#include "planning/ExecutionPlan.h"
#include "platform/IRegistryBackend.h"
#include "platform/WindowsAppxPackageProvider.h"
#include "platform/IFeatureStoreBackend.h"
#include "platform/IScheduledTaskBackend.h"
#include "platform/IBcdBackend.h"
#include "platform/IPowerSettingBackend.h"
#include "platform/IWindowsComponentBackend.h"

#include <functional>

namespace tweakopedia::execution {

struct TransactionRunResult {
    bool success{};
    bool rolledBack{};
    QString code;
    QString message;
};

struct ExecutionBackends {
    platform::IRegistryBackend* registry{};
    platform::IAppxPackageBackend* appx{};
    platform::IFeatureStoreBackend* featureStore{};
    platform::IScheduledTaskBackend* scheduledTasks{};
    platform::IBcdBackend* bcd{};
    platform::IPowerSettingBackend* powerSettings{};
    platform::IWindowsComponentBackend* windowsComponents{};
};

using TransactionProgressCallback = std::function<void(
    qsizetype operationIndex,
    qsizetype operationCount,
    const domain::TweakId& tweakId,
    QStringView operationType)>;

class TransactionRunner final
{
public:
    TransactionRunner(
        ExecutionBackends backends,
        persistence::TransactionFiles& files,
        persistence::TransactionRepository& repository);

    [[nodiscard]] TransactionRunResult run(
        const planning::ExecutionPlan& plan,
        const TransactionProgressCallback& progress = {});

private:
    platform::IRegistryBackend* backend_{};
    platform::IAppxPackageBackend* appxBackend_{};
    platform::IFeatureStoreBackend* featureBackend_{};
    platform::IScheduledTaskBackend* scheduledTaskBackend_{};
    platform::IBcdBackend* bcdBackend_{};
    platform::IPowerSettingBackend* powerBackend_{};
    platform::IWindowsComponentBackend* windowsComponentBackend_{};
    persistence::TransactionFiles* files_{};
    persistence::TransactionRepository* repository_{};
};

} // namespace tweakopedia::execution
