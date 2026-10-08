#include "execution/PlanValidator.h"

#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {
namespace {

void addError(PlanValidationResult& result, QString code, QString message)
{
    result.errors.append({.code = std::move(code), .message = std::move(message)});
}

bool sameProfile(const domain::SystemProfile& left, const domain::SystemProfile& right)
{
    return left.family == right.family
        && left.build == right.build
        && left.ubr == right.ubr
        && left.edition == right.edition
        && left.architecture == right.architecture;
}

} // namespace

bool PlanValidationResult::hasError(QStringView code) const
{
    return std::any_of(errors.cbegin(), errors.cend(), [code](const PlanValidationError& error) {
        return error.code == code;
    });
}

PlanValidationResult PlanValidator::validate(
    const planning::ExecutionPlan& plan,
    const domain::SystemProfile& currentProfile,
    const QDateTime& nowUtc,
    const QByteArray& expectedBodyHash,
    const QByteArray& actualBodyHash) const
{
    PlanValidationResult result;
    if (plan.schemaVersion != planning::executionPlanSchemaVersion) {
        addError(result, u"schema.unsupported"_s, u"Версия протокола не поддерживается."_s);
    }
    if (plan.transactionId.isNull()) {
        addError(result, u"transaction.invalid_id"_s, u"UUID транзакции отсутствует."_s);
    }
    if (!sameProfile(plan.profile, currentProfile)) {
        addError(result, u"profile.changed"_s, u"Профиль Windows изменился после построения плана."_s);
    }
    const auto ageSeconds = plan.createdAtUtc.secsTo(nowUtc);
    if (!plan.createdAtUtc.isValid() || ageSeconds > 300) {
        addError(result, u"plan.expired"_s, u"Предварительный план устарел."_s);
    } else if (ageSeconds < -30) {
        addError(result, u"plan.future"_s, u"Время создания плана находится в будущем."_s);
    }
    if (expectedBodyHash.size() != 64 || expectedBodyHash != actualBodyHash) {
        addError(result, u"hash.mismatch"_s, u"Ожидаемый SHA-256 плана не совпадает с полученным."_s);
    }
    if (plan.operations.isEmpty()) {
        addError(result, u"operations.empty"_s, u"План не содержит операций."_s);
    }

    static const QRegularExpression fingerprintPattern(u"^[0-9a-f]{64}$"_s);
    static const QRegularExpression packageNamePattern(u"^[A-Za-z0-9][A-Za-z0-9.-]{0,199}$"_s);
    for (const auto& operationVariant : plan.operations) {
        const auto fingerprint = std::visit(
            [](const auto& operation) { return operation.beforeFingerprint; },
            operationVariant);
        if (!fingerprintPattern.match(QString::fromLatin1(fingerprint)).hasMatch()) {
            addError(result, u"fingerprint.invalid"_s, u"Отпечаток исходного состояния имеет неверный формат."_s);
        }
        if (const auto* operation = std::get_if<planning::PlannedRegistryDwordChange>(&operationVariant)) {
            if (operation->change.location.key.isEmpty() || operation->change.location.valueName.isEmpty()) {
                addError(result, u"registry.path_invalid"_s, u"Путь значения реестра пуст."_s);
            }
        } else if (const auto* operation = std::get_if<planning::PlannedAppxRemoval>(&operationVariant)) {
            if (!packageNamePattern.match(operation->change.packageName).hasMatch()) {
                addError(result, u"appx.package_name_invalid"_s,
                         u"Имя AppX-пакета содержит недопустимые символы."_s);
            }
        } else if (const auto* operation = std::get_if<planning::PlannedFeatureStateChange>(&operationVariant)) {
            if (operation->change.featureId == 0
                || static_cast<quint32>(operation->change.state) > 2) {
                addError(result, u"feature.invalid"_s,
                         u"Feature ID или состояние недопустимы."_s);
            }
        }
    }
    result.accepted = result.errors.isEmpty();
    return result;
}

} // namespace tweakopedia::execution
