#include "domain/TweakDefinition.h"

#include <QRegularExpression>
#include <QSet>

namespace tweakopedia::domain {

bool TweakExplanation::isComplete() const noexcept
{
    return !purpose.trimmed().isEmpty()
        && !mechanism.trimmed().isEmpty()
        && !effect.trimmed().isEmpty()
        && !tradeoffs.trimmed().isEmpty()
        && !recommendation.trimmed().isEmpty()
        && !technicalDetails.trimmed().isEmpty();
}

bool TweakDefinition::supportsTargetState(QStringView stateId) const noexcept
{
    for (const auto& state : states) {
        if (state.id == stateId) {
            return true;
        }
    }
    return false;
}

QStringList TweakDefinition::validationErrors() const
{
    QStringList errors;
    static const QRegularExpression cyrillic(QStringLiteral("[А-Яа-яЁё]"));

    if (!id.isValid()) {
        errors.append(QStringLiteral("id.invalid"));
    }
    if (title.trimmed().isEmpty() || !cyrillic.match(title).hasMatch()) {
        errors.append(QStringLiteral("title.russian_required"));
    }
    if (category.trimmed().isEmpty()) {
        errors.append(QStringLiteral("category.required"));
    }
    if (subcategory.trimmed().isEmpty()) {
        errors.append(QStringLiteral("subcategory.required"));
    }
    if (summary.trimmed().isEmpty()) {
        errors.append(QStringLiteral("summary.required"));
    }
    if (!explanation.isComplete()) {
        errors.append(QStringLiteral("explanation.incomplete"));
    }
    if (states.isEmpty()) {
        errors.append(QStringLiteral("states.required"));
    }

    QSet<QString> stateIds;
    for (const auto& state : states) {
        if (state.id.trimmed().isEmpty() || state.title.trimmed().isEmpty()) {
            errors.append(QStringLiteral("state.invalid"));
            continue;
        }
        if (stateIds.contains(state.id)) {
            errors.append(QStringLiteral("state.duplicate"));
        }
        stateIds.insert(state.id);
    }

    for (const auto& rule : windowsDefaults) {
        if (!supportsTargetState(rule.stateId)) {
            errors.append(QStringLiteral("windows_default.unknown_state"));
        }
    }

    return errors;
}

bool TweakDefinition::isValid() const
{
    return validationErrors().isEmpty();
}

} // namespace tweakopedia::domain
