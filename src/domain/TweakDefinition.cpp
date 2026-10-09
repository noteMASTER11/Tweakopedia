#include "domain/TweakDefinition.h"

#include <QRegularExpression>
#include <QSet>

#include <type_traits>
#include <algorithm>

namespace tweakopedia::domain {

QStringList referencedInputIds(const TweakStateDefinition& state)
{
    QStringList result;
    for (const auto& operation : state.operations) {
        std::visit([&](const auto& typed) {
            using Operation = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<Operation, SetRegistryDwordOperation>
                          || std::is_same_v<Operation, SetRegistryValueOperation>) {
                if (typed.valueInput && !result.contains(*typed.valueInput)) {
                    result.append(*typed.valueInput);
                }
            } else if constexpr (std::is_same_v<Operation, FileOperationDefinition>) {
                if (typed.kind != FileOperationKind::Delete && !typed.inputId.isEmpty()
                    && !result.contains(typed.inputId)) {
                    result.append(typed.inputId);
                }
            }
        }, operation);
    }
    return result;
}

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

    QSet<QString> inputIds;
    for (const auto& input : inputs) {
        if (input.id.trimmed().isEmpty() || input.label.trimmed().isEmpty()) {
            errors.append(QStringLiteral("input.invalid"));
            continue;
        }
        if (inputIds.contains(input.id)) errors.append(QStringLiteral("input.duplicate"));
        inputIds.insert(input.id);
        if (input.minimumLength && input.maximumLength
            && *input.minimumLength > *input.maximumLength) {
            errors.append(QStringLiteral("input.length_range_invalid"));
        }
        if (input.minimum && input.maximum && *input.minimum > *input.maximum) {
            errors.append(QStringLiteral("input.integer_range_invalid"));
        }
        if (input.type == TweakInputType::Choice && input.choices.isEmpty()) {
            errors.append(QStringLiteral("input.choices_required"));
        }
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
        for (const auto& operation : state.operations) {
            std::visit([&](const auto& typed) {
                using Operation = std::decay_t<decltype(typed)>;
                if constexpr (std::is_same_v<Operation, SetRegistryDwordOperation>
                              || std::is_same_v<Operation, SetRegistryValueOperation>) {
                    if (typed.valueInput && !inputIds.contains(*typed.valueInput)) {
                        errors.append(QStringLiteral("input.reference_unknown"));
                    }
                } else if constexpr (std::is_same_v<Operation, FileOperationDefinition>) {
                    if (typed.kind != FileOperationKind::Delete) {
                        const auto found = std::find_if(inputs.cbegin(), inputs.cend(),
                            [&](const auto& input) { return input.id == typed.inputId; });
                        if (found == inputs.cend()) {
                            errors.append(QStringLiteral("input.reference_unknown"));
                        } else if (found->type != TweakInputType::File) {
                            errors.append(QStringLiteral("input.file_required"));
                        }
                    }
                } else if constexpr (std::is_same_v<Operation, SetBcdElementOperation>) {
                    if (!isWhitelistedBcdElement(typed.spec)
                        || (typed.value && !bcdValueMatchesKind(*typed.value, typed.spec.valueKind))) {
                        errors.append(QStringLiteral("bcd.operation_invalid"));
                    }
                } else if constexpr (std::is_same_v<Operation,
                                                    SetWindowsComponentStateOperation>) {
                    if (!isValidWindowsComponentTarget(typed.target)
                        || typed.state == WindowsComponentState::Unsupported
                        || (typed.target.kind == WindowsComponentKind::Capability
                            && typed.state == WindowsComponentState::Disabled)) {
                        errors.append(QStringLiteral("windows_component.operation_invalid"));
                    }
                }
            }, operation);
        }
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
