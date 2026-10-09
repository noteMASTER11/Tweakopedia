#include "planning/PlanBuilder.h"

#include "detection/CompatibilityEvaluator.h"

#include <algorithm>
#include <limits>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

using namespace Qt::StringLiterals;

namespace tweakopedia::planning {
namespace {

int restartRank(domain::RestartRequirement requirement)
{
    switch (requirement) {
    case domain::RestartRequirement::None: return 0;
    case domain::RestartRequirement::Explorer: return 1;
    case domain::RestartRequirement::Service: return 2;
    case domain::RestartRequirement::SignOut: return 3;
    case domain::RestartRequirement::Reboot: return 4;
    }
    return 0;
}

const domain::TweakStateDefinition* findState(
    const domain::TweakDefinition& tweak,
    QStringView stateId)
{
    for (const auto& state : tweak.states) {
        if (state.id == stateId) {
            return &state;
        }
    }
    return nullptr;
}

void addIssue(
    PlanBuildResult& result,
    const domain::TweakId& tweakId,
    QString code,
    QString message)
{
    result.issues.append(PlanIssue{
        .code = std::move(code),
        .message = std::move(message),
        .tweakId = tweakId,
    });
}

std::optional<QByteArray> registryFingerprint(
    const domain::DetectedState& current,
    const domain::RegistryLocation& location)
{
    if (current.registryFingerprints.isEmpty()) return current.fingerprint;
    const auto captured = std::find_if(
        current.registryFingerprints.cbegin(), current.registryFingerprints.cend(),
        [&](const domain::DetectedRegistryFingerprint& item) {
            return item.location == location;
        });
    if (captured == current.registryFingerprints.cend() || captured->fingerprint.isEmpty()) {
        return std::nullopt;
    }
    return captured->fingerprint;
}

std::optional<QByteArray> registryTreeFingerprint(
    const domain::DetectedState& current,
    const domain::RegistryKeyLocation& location)
{
    const auto captured = std::find_if(
        current.registryTreeFingerprints.cbegin(), current.registryTreeFingerprints.cend(),
        [&](const domain::DetectedRegistryTreeFingerprint& item) {
            return item.location == location;
        });
    if (captured == current.registryTreeFingerprints.cend() || captured->fingerprint.isEmpty()) {
        return std::nullopt;
    }
    return captured->fingerprint;
}

bool isInsideTree(
    const domain::RegistryLocation& value,
    const domain::RegistryKeyLocation& tree)
{
    if (value.hive != tree.hive || value.view != tree.view) return false;
    if (value.key.compare(tree.key, Qt::CaseInsensitive) == 0) return true;
    return value.key.startsWith(tree.key + u'\\', Qt::CaseInsensitive);
}

std::optional<quint64> unsignedInput(const domain::TweakInputValue& value)
{
    if (const auto* integer = std::get_if<qint64>(&value)) {
        if (*integer < 0) return std::nullopt;
        return static_cast<quint64>(*integer);
    }
    if (const auto* boolean = std::get_if<bool>(&value)) return *boolean ? 1 : 0;
    if (const auto* text = std::get_if<QString>(&value)) {
        bool ok{};
        const auto parsed = text->toULongLong(&ok, 0);
        if (ok) return parsed;
    }
    return std::nullopt;
}

std::optional<domain::SetRegistryDwordOperation> materialize(
    const domain::SetRegistryDwordOperation& operation,
    const domain::TweakInputMap& inputs)
{
    if (!operation.valueInput) return operation;
    const auto found = inputs.constFind(*operation.valueInput);
    if (found == inputs.cend()) return std::nullopt;
    const auto value = unsignedInput(*found);
    if (!value || *value > std::numeric_limits<quint32>::max()) return std::nullopt;
    auto result = operation;
    result.value = static_cast<quint32>(*value);
    result.valueInput.reset();
    return result;
}

std::optional<domain::SetRegistryValueOperation> materialize(
    const domain::SetRegistryValueOperation& operation,
    const domain::TweakInputMap& inputs)
{
    if (!operation.valueInput) return operation;
    const auto found = inputs.constFind(*operation.valueInput);
    if (found == inputs.cend()) return std::nullopt;
    auto result = operation;
    switch (operation.value.nativeType) {
    case 1:
    case 2: {
        const auto* text = std::get_if<QString>(&*found);
        if (!text) return std::nullopt;
        result.value = operation.value.nativeType == 1
            ? domain::RegistryValueSpec::string(*text)
            : domain::RegistryValueSpec::expandString(*text);
        break;
    }
    case 4: {
        const auto value = unsignedInput(*found);
        if (!value || *value > std::numeric_limits<quint32>::max()) return std::nullopt;
        result.value = domain::RegistryValueSpec::dword(static_cast<quint32>(*value));
        break;
    }
    case 11: {
        const auto value = unsignedInput(*found);
        if (!value) return std::nullopt;
        result.value = domain::RegistryValueSpec::qword(*value);
        break;
    }
    case 3: {
        const auto* text = std::get_if<QString>(&*found);
        if (!text) return std::nullopt;
        const auto bytes = QByteArray::fromHex(text->toLatin1());
        if (bytes.toHex().compare(text->toLatin1(), Qt::CaseInsensitive) != 0) return std::nullopt;
        result.value = domain::RegistryValueSpec::binary(bytes);
        break;
    }
    default:
        return std::nullopt;
    }
    result.valueInput.reset();
    return result;
}

std::optional<domain::FileSnapshot> inspectFile(const QString& destination)
{
    domain::FileSnapshot snapshot{.destination = destination};
    const QFileInfo info(destination);
    if (!info.exists()) return snapshot;
    if (!info.isFile() || !info.isReadable() || info.size() < 0) return std::nullopt;
    QFile file(destination);
    if (!file.open(QIODevice::ReadOnly)) return std::nullopt;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) return std::nullopt;
    snapshot.existed = true;
    snapshot.size = static_cast<quint64>(info.size());
    snapshot.sha256 = hash.result().toHex();
    return snapshot;
}

QByteArray fileFingerprint(const domain::FileSnapshot& snapshot)
{
    QJsonObject state{
        {u"destination"_s, snapshot.destination},
        {u"existed"_s, snapshot.existed},
        {u"size"_s, QString::number(snapshot.size)},
        {u"sha256"_s, QString::fromLatin1(snapshot.sha256)},
    };
    return QCryptographicHash::hash(
        QJsonDocument(state).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex();
}

const domain::TweakInputDefinition* findInput(
    const domain::TweakDefinition& tweak, QStringView id)
{
    for (const auto& input : tweak.inputs) if (input.id == id) return &input;
    return nullptr;
}

} // namespace

PlanBuildResult PlanBuilder::build(
    const content::TweakCatalog& catalog,
    const TweakQueue& queue,
    const QHash<domain::TweakId, domain::DetectedState>& detected,
    const domain::SystemProfile& profile) const
{
    PlanBuildResult result;
    ExecutionPlan plan{
        .schemaVersion = executionPlanSchemaVersion,
        .transactionId = QUuid::createUuid(),
        .profile = profile,
        .createdAtUtc = QDateTime::currentDateTimeUtc(),
    };

    auto items = queue.items();
    std::sort(items.begin(), items.end(), [](const QueueItem& left, const QueueItem& right) {
        return left.tweakId.toString() < right.tweakId.toString();
    });

    for (const auto& item : items) {
        const auto* tweak = catalog.find(item.tweakId);
        if (!tweak) {
            addIssue(result, item.tweakId, u"tweak.unknown"_s, u"Твик из очереди отсутствует в каталоге."_s);
            continue;
        }

        const auto compatibility = detection::evaluate(*tweak, profile);
        if (!compatibility.supported) {
            addIssue(result, item.tweakId, compatibility.reasonCode, compatibility.explanation);
            continue;
        }

        const auto detectedIterator = detected.constFind(item.tweakId);
        if (detectedIterator == detected.cend()) {
            addIssue(result, item.tweakId, u"state.unknown"_s, u"Фактическое состояние не было определено."_s);
            continue;
        }
        const auto& current = *detectedIterator;
        if (current.status == domain::DetectionStatus::Unsupported) {
            addIssue(result, item.tweakId, u"state.unsupported"_s, u"Параметр не поддерживается текущей системой."_s);
            continue;
        }
        if (current.status == domain::DetectionStatus::Unknown
            || current.status == domain::DetectionStatus::Mixed) {
            addIssue(result, item.tweakId, u"state.unknown"_s, u"Исходное состояние нельзя определить однозначно."_s);
            continue;
        }
        if (current.stateId == item.targetState && item.inputs.isEmpty()) {
            continue;
        }
        if (current.fingerprint.isEmpty()) {
            addIssue(result, item.tweakId, u"state.fingerprint_missing"_s, u"Нет отпечатка исходного состояния."_s);
            continue;
        }

        const auto* target = findState(*tweak, item.targetState);
        if (!target) {
            addIssue(result, item.tweakId, u"state.unknown"_s, u"Целевое состояние отсутствует в определении."_s);
            continue;
        }

        for (const auto& operation : target->operations) {
            if (const auto* registry = std::get_if<domain::SetRegistryDwordOperation>(&operation)) {
                const auto resolved = materialize(*registry, item.inputs);
                if (!resolved) {
                    addIssue(result, item.tweakId, u"input.payload_invalid"_s,
                             u"Значение поля нельзя преобразовать в payload операции."_s);
                    continue;
                }
                const auto beforeFingerprint = registryFingerprint(current, registry->location);
                if (!beforeFingerprint) {
                    addIssue(
                        result,
                        item.tweakId,
                        u"state.fingerprint_missing"_s,
                        u"Нет отпечатка исходного значения одной из операций."_s);
                    continue;
                }
                plan.operations.append(PlannedRegistryDwordChange{
                    .tweakId = item.tweakId,
                    .targetState = item.targetState,
                    .change = *resolved,
                    .beforeFingerprint = *beforeFingerprint,
                    .restart = tweak->restart,
                    .inputs = item.inputs,
                });
            } else if (const auto* registry = std::get_if<domain::SetRegistryValueOperation>(&operation)) {
                const auto resolved = materialize(*registry, item.inputs);
                if (!resolved) {
                    addIssue(result, item.tweakId, u"input.payload_invalid"_s,
                             u"Значение поля нельзя преобразовать в payload операции."_s);
                    continue;
                }
                const auto beforeFingerprint = registryFingerprint(current, registry->location);
                if (!beforeFingerprint) {
                    addIssue(result, item.tweakId, u"state.fingerprint_missing"_s,
                             u"Нет отпечатка исходного значения одной из операций."_s);
                    continue;
                }
                plan.operations.append(PlannedRegistryValueChange{
                    .tweakId = item.tweakId,
                    .targetState = item.targetState,
                    .change = *resolved,
                    .beforeFingerprint = *beforeFingerprint,
                    .restart = tweak->restart,
                    .inputs = item.inputs,
                });
            } else if (const auto* registry = std::get_if<domain::DeleteRegistryValueOperation>(&operation)) {
                const auto beforeFingerprint = registryFingerprint(current, registry->location);
                if (!beforeFingerprint) {
                    addIssue(result, item.tweakId, u"state.fingerprint_missing"_s,
                             u"Нет отпечатка исходного значения одной из операций."_s);
                    continue;
                }
                plan.operations.append(PlannedRegistryValueChange{
                    .tweakId = item.tweakId,
                    .targetState = item.targetState,
                    .change = *registry,
                    .beforeFingerprint = *beforeFingerprint,
                    .restart = tweak->restart,
                    .inputs = item.inputs,
                });
            } else if (const auto* tree = std::get_if<domain::CreateRegistryKeyOperation>(&operation)) {
                const auto beforeFingerprint = registryTreeFingerprint(current, tree->location);
                if (!beforeFingerprint) {
                    addIssue(result, item.tweakId, u"state.fingerprint_missing"_s,
                             u"Нет отпечатка исходной ветви реестра."_s);
                    continue;
                }
                plan.operations.append(PlannedRegistryTreeChange{
                    .tweakId = item.tweakId,
                    .targetState = item.targetState,
                    .change = *tree,
                    .beforeFingerprint = *beforeFingerprint,
                    .restart = tweak->restart,
                    .inputs = item.inputs,
                });
            } else if (const auto* tree = std::get_if<domain::DeleteRegistryTreeOperation>(&operation)) {
                const auto beforeFingerprint = registryTreeFingerprint(current, tree->location);
                if (!beforeFingerprint) {
                    addIssue(result, item.tweakId, u"state.fingerprint_missing"_s,
                             u"Нет отпечатка исходной ветви реестра."_s);
                    continue;
                }
                plan.operations.append(PlannedRegistryTreeChange{
                    .tweakId = item.tweakId,
                    .targetState = item.targetState,
                    .change = *tree,
                    .beforeFingerprint = *beforeFingerprint,
                    .restart = tweak->restart,
                    .inputs = item.inputs,
                });
            } else if (const auto* file = std::get_if<domain::FileOperationDefinition>(&operation)) {
                const auto before = inspectFile(file->destination);
                if (!before) {
                    addIssue(result, item.tweakId, u"file.destination_unreadable"_s,
                             u"Не удалось прочитать файл назначения."_s);
                    continue;
                }
                domain::InputArtifact artifact;
                const domain::TweakInputDefinition* inputDefinition{};
                if (file->kind != domain::FileOperationKind::Delete) {
                    inputDefinition = findInput(*tweak, file->inputId);
                    const auto input = item.inputs.constFind(file->inputId);
                    if (!inputDefinition || input == item.inputs.cend()) {
                        addIssue(result, item.tweakId, u"input.required"_s,
                                 u"Для файловой операции не выбран входной файл."_s);
                        continue;
                    }
                    if (const auto* path = std::get_if<QString>(&*input)) {
                        artifact = {.id = file->inputId, .managedPath = *path};
                    } else if (const auto* managed = std::get_if<domain::InputArtifact>(&*input)) {
                        artifact = *managed;
                    } else {
                        addIssue(result, item.tweakId, u"input.type_invalid"_s,
                                 u"Входной параметр файловой операции имеет неверный тип."_s);
                        continue;
                    }
                }
                plan.operations.append(PlannedFileChange{
                    .tweakId = item.tweakId,
                    .targetState = item.targetState,
                    .change = domain::FileOperation{
                        .kind = file->kind,
                        .artifact = artifact,
                        .destination = file->destination,
                    },
                    .beforeFingerprint = fileFingerprint(*before),
                    .restart = tweak->restart,
                    .inputs = item.inputs,
                    .allowedExtensions = inputDefinition
                        ? inputDefinition->allowedExtensions : QStringList{},
                    .maximumInputSize = inputDefinition && inputDefinition->maximumFileSize
                        ? *inputDefinition->maximumFileSize : 0,
                });
            } else if (const auto* scheduledTask =
                           std::get_if<domain::SetScheduledTaskEnabledOperation>(&operation)) {
                plan.operations.append(PlannedScheduledTaskChange{
                    .tweakId = item.tweakId,
                    .targetState = item.targetState,
                    .change = *scheduledTask,
                    .beforeFingerprint = current.fingerprint,
                    .restart = tweak->restart,
                    .inputs = item.inputs,
                });
            } else if (const auto* bcd =
                           std::get_if<domain::SetBcdElementOperation>(&operation)) {
                plan.operations.append(PlannedBcdElementChange{
                    .tweakId = item.tweakId,
                    .targetState = item.targetState,
                    .change = *bcd,
                    .beforeFingerprint = current.fingerprint,
                    .restart = tweak->restart,
                    .inputs = item.inputs,
                });
            } else if (const auto* power =
                           std::get_if<domain::SetPowerSettingOperation>(&operation)) {
                plan.operations.append(PlannedPowerSettingChange{
                    .tweakId = item.tweakId, .targetState = item.targetState,
                    .change = *power, .beforeFingerprint = current.fingerprint,
                    .restart = tweak->restart, .inputs = item.inputs});
            } else if (const auto* component =
                           std::get_if<domain::SetWindowsComponentStateOperation>(&operation)) {
                plan.operations.append(PlannedWindowsComponentChange{
                    .tweakId = item.tweakId, .targetState = item.targetState,
                    .change = *component, .beforeFingerprint = current.fingerprint,
                    .restart = tweak->restart, .inputs = item.inputs});
            } else if (const auto* appx = std::get_if<domain::RemoveAppxPackageOperation>(&operation)) {
                plan.operations.append(PlannedAppxRemoval{
                    .tweakId = item.tweakId,
                    .targetState = item.targetState,
                    .change = *appx,
                    .beforeFingerprint = current.fingerprint,
                    .restart = tweak->restart,
                    .inputs = item.inputs,
                });
            } else if (const auto* feature = std::get_if<domain::SetFeatureStateOperation>(&operation)) {
                plan.operations.append(PlannedFeatureStateChange{
                    .tweakId = item.tweakId,
                    .targetState = item.targetState,
                    .change = *feature,
                    .beforeFingerprint = current.fingerprint,
                    .restart = tweak->restart,
                    .inputs = item.inputs,
                });
            }
        }
        if (restartRank(tweak->restart) > restartRank(plan.restart)) {
            plan.restart = tweak->restart;
        }
    }

    for (const auto& treeVariant : plan.operations) {
        const auto* treeOperation = std::get_if<PlannedRegistryTreeChange>(&treeVariant);
        if (!treeOperation) continue;
        const auto treeLocation = std::visit(
            [](const auto& change) { return change.location; }, treeOperation->change);
        for (const auto& valueVariant : plan.operations) {
            std::optional<domain::RegistryLocation> valueLocation;
            if (const auto* dword = std::get_if<PlannedRegistryDwordChange>(&valueVariant)) {
                valueLocation = dword->change.location;
            } else if (const auto* value = std::get_if<PlannedRegistryValueChange>(&valueVariant)) {
                valueLocation = std::visit(
                    [](const auto& change) { return change.location; }, value->change);
            }
            if (valueLocation && isInsideTree(*valueLocation, treeLocation)) {
                addIssue(result, treeOperation->tweakId, u"registry.object_conflict"_s,
                         u"Операция над ветвью пересекается с операцией над значением внутри неё."_s);
                break;
            }
        }
    }

    if (!result.issues.isEmpty()) {
        return result;
    }
    std::stable_sort(
        plan.operations.begin(), plan.operations.end(),
        [](const PlannedOperation& left, const PlannedOperation& right) {
            return left.index() < right.index();
        });
    plan.summary = plan.operations.isEmpty()
        ? u"Изменения не требуются."_s
        : u"Будет применено операций: "_s + QString::number(plan.operations.size()) + u"."_s;
    result.plan = std::move(plan);
    return result;
}

} // namespace tweakopedia::planning
