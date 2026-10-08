#pragma once

#include "domain/SystemProfile.h"
#include "domain/TweakDefinition.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QUuid>
#include <QVector>

#include <variant>

namespace tweakopedia::planning {

inline constexpr int executionPlanSchemaVersion = 1;

struct PlannedRegistryDwordChange {
    domain::TweakId tweakId;
    QString targetState;
    domain::SetRegistryDwordOperation change;
    QByteArray beforeFingerprint;
    domain::RestartRequirement restart{domain::RestartRequirement::None};
};

struct PlannedAppxRemoval {
    domain::TweakId tweakId;
    QString targetState;
    domain::RemoveAppxPackageOperation change;
    QByteArray beforeFingerprint;
    domain::RestartRequirement restart{domain::RestartRequirement::None};
};

struct PlannedFeatureStateChange {
    domain::TweakId tweakId;
    QString targetState;
    domain::SetFeatureStateOperation change;
    QByteArray beforeFingerprint;
    domain::RestartRequirement restart{domain::RestartRequirement::None};
};

using PlannedOperation = std::variant<
    PlannedRegistryDwordChange,
    PlannedFeatureStateChange,
    PlannedAppxRemoval>;

struct ExecutionPlan {
    int schemaVersion{executionPlanSchemaVersion};
    QUuid transactionId;
    domain::SystemProfile profile;
    QDateTime createdAtUtc;
    QVector<PlannedOperation> operations;
    QString summary;
    domain::RestartRequirement restart{domain::RestartRequirement::None};
};

} // namespace tweakopedia::planning
