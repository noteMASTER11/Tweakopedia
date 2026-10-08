#pragma once

#include "content/TweakCatalog.h"
#include "domain/DetectedState.h"
#include "planning/ExecutionPlan.h"
#include "planning/TweakQueue.h"

#include <QHash>

#include <optional>

namespace tweakopedia::planning {

struct PlanIssue {
    QString code;
    QString message;
    domain::TweakId tweakId;
};

struct PlanBuildResult {
    std::optional<ExecutionPlan> plan;
    QVector<PlanIssue> issues;
};

class PlanBuilder final
{
public:
    [[nodiscard]] PlanBuildResult build(
        const content::TweakCatalog& catalog,
        const TweakQueue& queue,
        const QHash<domain::TweakId, domain::DetectedState>& detected,
        const domain::SystemProfile& profile) const;
};

} // namespace tweakopedia::planning
