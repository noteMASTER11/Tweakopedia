#pragma once

#include "domain/RegistryTypes.h"
#include "domain/RegistryTree.h"
#include "domain/SystemProfile.h"
#include "domain/TweakId.h"
#include "domain/TweakInput.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace tweakopedia::domain {

enum class TweakKind {
    Setting,
    Action,
    Diagnostic,
};

enum class Impact {
    Low,
    Medium,
    High,
    Critical,
};

enum class Reversibility {
    Reversible,
    Conditional,
    Irreversible,
};

enum class RestartRequirement {
    None,
    Explorer,
    Service,
    SignOut,
    Reboot,
};

struct TweakExplanation {
    QString purpose;
    QString mechanism;
    QString effect;
    QString tradeoffs;
    QString recommendation;
    QString technicalDetails;

    [[nodiscard]] bool isComplete() const noexcept;
};

struct BuildRange {
    quint32 minimum{};
    quint32 maximum{};
};

struct WindowsCompatibility {
    QVector<CpuArchitecture> architectures;
    QVector<WindowsFamily> operatingSystems;
    quint32 minimumBuild{};
    std::optional<quint32> maximumBuild;
    QStringList requiredComponents;
};

struct TweakStateDefinition {
    QString id;
    QString title;
    QVector<OperationSpec> operations;
};

[[nodiscard]] QStringList referencedInputIds(const TweakStateDefinition& state);

struct WindowsDefaultRule {
    WindowsFamily operatingSystem{WindowsFamily::Unknown};
    BuildRange builds;
    QString stateId;
};

struct TweakDefinition {
    TweakId id;
    QString title;
    QString category;
    QString subcategory;
    TweakKind kind{TweakKind::Setting};
    QString summary;
    TweakExplanation explanation;
    WindowsCompatibility compatibility;
    QVector<TweakInputDefinition> inputs;
    QVector<TweakStateDefinition> states;
    QVector<WindowsDefaultRule> windowsDefaults;
    std::optional<RegistryDwordDetection> detection;
    std::optional<RegistryValueDetection> valueDetection;
    std::optional<RegistryTreeDetection> treeDetection;
    std::optional<AppxPackageDetection> appxDetection;
    std::optional<FeatureStateDetection> featureDetection;
    std::optional<ScheduledTaskDetection> scheduledTaskDetection;
    std::optional<BcdElementDetection> bcdDetection;
    std::optional<PowerSettingDetection> powerDetection;
    std::optional<WindowsComponentDetection> windowsComponentDetection;
    QVector<TweakId> dependencies;
    QVector<TweakId> conflicts;
    Impact impact{Impact::Low};
    Reversibility reversibility{Reversibility::Reversible};
    RestartRequirement restart{RestartRequirement::None};
    bool requiresNetwork{false};

    [[nodiscard]] bool supportsTargetState(QStringView stateId) const noexcept;
    [[nodiscard]] QStringList validationErrors() const;
    [[nodiscard]] bool isValid() const;
};

} // namespace tweakopedia::domain
