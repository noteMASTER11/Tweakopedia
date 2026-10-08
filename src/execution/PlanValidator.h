#pragma once

#include "planning/ExecutionPlan.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QVector>

namespace tweakopedia::execution {

struct PlanValidationError {
    QString code;
    QString message;
};

struct PlanValidationResult {
    bool accepted{};
    QVector<PlanValidationError> errors;

    [[nodiscard]] bool hasError(QStringView code) const;
};

class PlanValidator final
{
public:
    [[nodiscard]] PlanValidationResult validate(
        const planning::ExecutionPlan& plan,
        const domain::SystemProfile& currentProfile,
        const QDateTime& nowUtc,
        const QByteArray& expectedBodyHash,
        const QByteArray& actualBodyHash) const;
};

} // namespace tweakopedia::execution
