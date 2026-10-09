#include "execution/PlanValidator.h"

#include <QRegularExpression>
#include <QDir>
#include <QSet>

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
    QSet<QString> fileDestinations;
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
        } else if (const auto* operation = std::get_if<planning::PlannedRegistryValueChange>(&operationVariant)) {
            std::visit([&](const auto& change) {
                if (change.location.key.isEmpty() || change.location.valueName.isEmpty()) {
                    addError(result, u"registry.path_invalid"_s, u"Путь значения реестра пуст."_s);
                }
                using T = std::decay_t<decltype(change)>;
                if constexpr (std::is_same_v<T, domain::SetRegistryValueOperation>) {
                    const auto type = change.value.nativeType;
                    if (type != 1 && type != 2 && type != 3
                        && type != 4 && type != 7 && type != 11) {
                        addError(result, u"registry.native_type_unknown"_s,
                                 u"Native type значения реестра не поддерживается."_s);
                    }
                    if (change.value.rawValue.size() > 1024 * 1024) {
                        addError(result, u"registry.value_too_large"_s,
                                 u"Значение реестра превышает допустимый размер."_s);
                    }
                }
            }, operation->change);
        } else if (const auto* operation = std::get_if<planning::PlannedRegistryTreeChange>(&operationVariant)) {
            const auto location = std::visit(
                [](const auto& change) { return change.location; }, operation->change);
            if (location.key.isEmpty()) {
                addError(result, u"registry.path_invalid"_s,
                         u"Путь раздела реестра пуст."_s);
            }
        } else if (const auto* operation = std::get_if<planning::PlannedAppxRemoval>(&operationVariant)) {
            if (!packageNamePattern.match(operation->change.packageName).hasMatch()) {
                addError(result, u"appx.package_name_invalid"_s,
                         u"Имя AppX-пакета содержит недопустимые символы."_s);
            }
        } else if (const auto* operation = std::get_if<planning::PlannedFileChange>(&operationVariant)) {
            const auto destination = QDir::cleanPath(operation->change.destination);
            if (!QDir::isAbsolutePath(destination) || destination != operation->change.destination) {
                addError(result, u"file.destination_invalid"_s,
                         u"Путь назначения файла недопустим."_s);
            }
            const auto key = destination.toLower();
            if (fileDestinations.contains(key)) {
                addError(result, u"file.destination_conflict"_s,
                         u"Несколько операций изменяют один файл назначения."_s);
            }
            fileDestinations.insert(key);
            if (operation->change.kind != domain::FileOperationKind::Delete) {
                const auto path = QDir::cleanPath(operation->change.artifact.managedPath);
                if (QDir::isAbsolutePath(path) || path == u".."_s
                    || path.startsWith(u"../"_s)
                    || !(path.startsWith(u"inputs/"_s) || path.startsWith(u"inputs\\"_s))
                    || !fingerprintPattern.match(
                        QString::fromLatin1(operation->change.artifact.sha256)).hasMatch()) {
                    addError(result, u"file.artifact_invalid"_s,
                             u"Ссылка на файловый артефакт недопустима."_s);
                }
            }
        } else if (const auto* operation = std::get_if<planning::PlannedFeatureStateChange>(&operationVariant)) {
            if (operation->change.featureId == 0
                || static_cast<quint32>(operation->change.state) > 2) {
                addError(result, u"feature.invalid"_s,
                         u"Feature ID или состояние недопустимы."_s);
            }
        } else if (const auto* operation =
                       std::get_if<planning::PlannedScheduledTaskChange>(&operationVariant)) {
            const auto& location = operation->change.location;
            if (!location.folder.startsWith(u'\\') || location.folder.contains(u".."_s)
                || location.name.trimmed().isEmpty() || location.name.contains(u'\\')
                || location.name.contains(u'/')) {
                addError(result, u"scheduled_task.location_invalid"_s,
                         u"Параметры задачи планировщика недопустимы."_s);
            }
        } else if (const auto* operation =
                       std::get_if<planning::PlannedBcdElementChange>(&operationVariant)) {
            if (!domain::isWhitelistedBcdElement(operation->change.spec)
                || (operation->change.value
                    && !domain::bcdValueMatchesKind(
                        *operation->change.value, operation->change.spec.valueKind))) {
                addError(result, u"bcd.element_invalid"_s,
                         u"BCD-операция не входит в whitelist или имеет неверный тип."_s);
            }
        } else if (const auto* operation =
                       std::get_if<planning::PlannedPowerSettingChange>(&operationVariant)) {
            if (!domain::isValidPowerLocation(operation->change.location))
                addError(result, u"power.location_invalid"_s,
                         u"Параметры схемы питания недопустимы."_s);
        } else if (const auto* operation =
                       std::get_if<planning::PlannedWindowsComponentChange>(&operationVariant)) {
            const auto& change = operation->change;
            if (!domain::isValidWindowsComponentTarget(change.target)
                || change.state == domain::WindowsComponentState::Unsupported
                || (change.target.kind == domain::WindowsComponentKind::Capability
                    && change.state == domain::WindowsComponentState::Disabled)) {
                addError(result, u"windows_component.invalid"_s,
                         u"Параметры компонента Windows недопустимы."_s);
            }
        }
    }
    result.accepted = result.errors.isEmpty();
    return result;
}

} // namespace tweakopedia::execution
