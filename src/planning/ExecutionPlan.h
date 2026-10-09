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
    domain::TweakInputMap inputs;
};

using RegistryValueChange = std::variant<
    domain::SetRegistryValueOperation,
    domain::DeleteRegistryValueOperation>;

struct PlannedRegistryValueChange {
    domain::TweakId tweakId;
    QString targetState;
    RegistryValueChange change;
    QByteArray beforeFingerprint;
    domain::RestartRequirement restart{domain::RestartRequirement::None};
    domain::TweakInputMap inputs;
};

using RegistryTreeChange = std::variant<
    domain::CreateRegistryKeyOperation,
    domain::DeleteRegistryTreeOperation>;

struct PlannedRegistryTreeChange {
    domain::TweakId tweakId;
    QString targetState;
    RegistryTreeChange change;
    QByteArray beforeFingerprint;
    domain::RestartRequirement restart{domain::RestartRequirement::None};
    domain::TweakInputMap inputs;
};

struct PlannedFileChange {
    domain::TweakId tweakId;
    QString targetState;
    domain::FileOperation change;
    QByteArray beforeFingerprint;
    domain::RestartRequirement restart{domain::RestartRequirement::None};
    domain::TweakInputMap inputs;
    QStringList allowedExtensions;
    quint64 maximumInputSize{};
};

struct PlannedScheduledTaskChange {
    domain::TweakId tweakId;
    QString targetState;
    domain::SetScheduledTaskEnabledOperation change;
    QByteArray beforeFingerprint;
    domain::RestartRequirement restart{domain::RestartRequirement::None};
    domain::TweakInputMap inputs;
};

struct PlannedBcdElementChange {
    domain::TweakId tweakId;
    QString targetState;
    domain::SetBcdElementOperation change;
    QByteArray beforeFingerprint;
    domain::RestartRequirement restart{domain::RestartRequirement::None};
    domain::TweakInputMap inputs;
};

struct PlannedPowerSettingChange {
    domain::TweakId tweakId;
    QString targetState;
    domain::SetPowerSettingOperation change;
    QByteArray beforeFingerprint;
    domain::RestartRequirement restart{domain::RestartRequirement::None};
    domain::TweakInputMap inputs;
};

struct PlannedWindowsComponentChange {
    domain::TweakId tweakId;
    QString targetState;
    domain::SetWindowsComponentStateOperation change;
    QByteArray beforeFingerprint;
    domain::RestartRequirement restart{domain::RestartRequirement::None};
    domain::TweakInputMap inputs;
};

struct PlannedAppxRemoval {
    domain::TweakId tweakId;
    QString targetState;
    domain::RemoveAppxPackageOperation change;
    QByteArray beforeFingerprint;
    domain::RestartRequirement restart{domain::RestartRequirement::None};
    domain::TweakInputMap inputs;
};

struct PlannedFeatureStateChange {
    domain::TweakId tweakId;
    QString targetState;
    domain::SetFeatureStateOperation change;
    QByteArray beforeFingerprint;
    domain::RestartRequirement restart{domain::RestartRequirement::None};
    domain::TweakInputMap inputs;
};

using PlannedOperation = std::variant<
    PlannedRegistryDwordChange,
    PlannedRegistryValueChange,
    PlannedRegistryTreeChange,
    PlannedFileChange,
    PlannedScheduledTaskChange,
    PlannedBcdElementChange,
    PlannedPowerSettingChange,
    PlannedWindowsComponentChange,
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
