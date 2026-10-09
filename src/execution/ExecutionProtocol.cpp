#include "execution/ExecutionProtocol.h"

#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>

#include <type_traits>

using namespace Qt::StringLiterals;

namespace tweakopedia::execution {
namespace {

using domain::CpuArchitecture;
using domain::RegistryHive;
using domain::RegistryKeyLocation;
using domain::RegistryLocation;
using domain::RegistryValueSpec;
using domain::RegistryView;
using domain::RestartRequirement;
using domain::RemoveAppxPackageOperation;
using domain::SetRegistryDwordOperation;
using domain::SetRegistryValueOperation;
using domain::DeleteRegistryValueOperation;
using domain::CreateRegistryKeyOperation;
using domain::DeleteRegistryTreeOperation;
using domain::SetFeatureStateOperation;
using domain::SetScheduledTaskEnabledOperation;
using domain::SetBcdElementOperation;
using domain::FeatureEnabledState;
using domain::SystemProfile;
using domain::TweakId;
using domain::WindowsFamily;
using planning::ExecutionPlan;
using planning::PlannedAppxRemoval;
using planning::PlannedFeatureStateChange;
using planning::PlannedOperation;
using planning::PlannedRegistryDwordChange;
using planning::PlannedRegistryValueChange;
using planning::PlannedRegistryTreeChange;
using planning::PlannedFileChange;
using planning::PlannedScheduledTaskChange;
using planning::PlannedBcdElementChange;
using planning::PlannedPowerSettingChange;
using planning::PlannedWindowsComponentChange;

QString familyName(WindowsFamily family)
{
    switch (family) {
    case WindowsFamily::Windows10: return u"windows10"_s;
    case WindowsFamily::Windows11: return u"windows11"_s;
    case WindowsFamily::Unknown: return u"unknown"_s;
    }
    return u"unknown"_s;
}

QString architectureName(CpuArchitecture architecture)
{
    switch (architecture) {
    case CpuArchitecture::X64: return u"x64"_s;
    case CpuArchitecture::Arm64: return u"arm64"_s;
    case CpuArchitecture::X86: return u"x86"_s;
    case CpuArchitecture::Unknown: return u"unknown"_s;
    }
    return u"unknown"_s;
}

QString restartName(RestartRequirement restart)
{
    switch (restart) {
    case RestartRequirement::None: return u"none"_s;
    case RestartRequirement::Explorer: return u"explorer"_s;
    case RestartRequirement::Service: return u"service"_s;
    case RestartRequirement::SignOut: return u"sign_out"_s;
    case RestartRequirement::Reboot: return u"reboot"_s;
    }
    return u"none"_s;
}

QString hiveName(RegistryHive hive)
{
    switch (hive) {
    case RegistryHive::CurrentUser: return u"HKCU"_s;
    case RegistryHive::LocalMachine: return u"HKLM"_s;
    case RegistryHive::ClassesRoot: return u"HKCR"_s;
    case RegistryHive::Users: return u"HKU"_s;
    }
    return {};
}

QString viewName(RegistryView view)
{
    switch (view) {
    case RegistryView::Default: return u"default"_s;
    case RegistryView::Registry32: return u"registry32"_s;
    case RegistryView::Registry64: return u"registry64"_s;
    }
    return u"default"_s;
}

QJsonObject profileObject(const SystemProfile& profile)
{
    return {
        {u"architecture"_s, architectureName(profile.architecture)},
        {u"build"_s, static_cast<qint64>(profile.build)},
        {u"edition"_s, profile.edition},
        {u"family"_s, familyName(profile.family)},
        {u"ubr"_s, static_cast<qint64>(profile.ubr)},
    };
}

QJsonObject registryObject(const RegistryLocation& location, quint32 value)
{
    return {
        {u"hive"_s, hiveName(location.hive)},
        {u"key"_s, location.key},
        {u"value"_s, static_cast<qint64>(value)},
        {u"value_name"_s, location.valueName},
        {u"view"_s, viewName(location.view)},
    };
}

QJsonObject registryLocationObject(const RegistryLocation& location)
{
    return {
        {u"hive"_s, hiveName(location.hive)},
        {u"key"_s, location.key},
        {u"value_name"_s, location.valueName},
        {u"view"_s, viewName(location.view)},
    };
}

QJsonObject registryKeyObject(const RegistryKeyLocation& location)
{
    return {
        {u"hive"_s, hiveName(location.hive)},
        {u"key"_s, location.key},
        {u"view"_s, viewName(location.view)},
    };
}

QJsonObject inputsObject(const domain::TweakInputMap& inputs)
{
    QJsonObject result;
    for (auto iterator = inputs.cbegin(); iterator != inputs.cend(); ++iterator) {
        QJsonObject encoded;
        std::visit([&](const auto& value) {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, QString>) {
                encoded = {{u"type"_s, u"string"_s}, {u"value"_s, value}};
            } else if constexpr (std::is_same_v<Value, qint64>) {
                encoded = {{u"type"_s, u"integer"_s},
                           {u"value"_s, QString::number(value)}};
            } else if constexpr (std::is_same_v<Value, bool>) {
                encoded = {{u"type"_s, u"boolean"_s}, {u"value"_s, value}};
            } else {
                encoded = {
                    {u"type"_s, u"artifact"_s},
                    {u"id"_s, value.id},
                    {u"storage_id"_s, value.storageId},
                    {u"path"_s, value.managedPath},
                    {u"size"_s, QString::number(value.size)},
                    {u"sha256"_s, QString::fromLatin1(value.sha256)},
                };
            }
        }, iterator.value());
        result.insert(iterator.key(), encoded);
    }
    return result;
}

QJsonObject operationObject(const PlannedRegistryDwordChange& operation)
{
    return {
        {u"before_fingerprint"_s, QString::fromLatin1(operation.beforeFingerprint)},
        {u"inputs"_s, inputsObject(operation.inputs)},
        {u"registry"_s, registryObject(operation.change.location, operation.change.value)},
        {u"restart"_s, restartName(operation.restart)},
        {u"target_state"_s, operation.targetState},
        {u"tweak_id"_s, operation.tweakId.toString()},
        {u"type"_s, u"registry.set_dword"_s},
    };
}

QJsonObject operationObject(const PlannedRegistryValueChange& operation)
{
    QJsonObject result{
        {u"before_fingerprint"_s, QString::fromLatin1(operation.beforeFingerprint)},
        {u"inputs"_s, inputsObject(operation.inputs)},
        {u"restart"_s, restartName(operation.restart)},
        {u"target_state"_s, operation.targetState},
        {u"tweak_id"_s, operation.tweakId.toString()},
    };
    if (const auto* set = std::get_if<SetRegistryValueOperation>(&operation.change)) {
        auto registry = registryLocationObject(set->location);
        registry.insert(u"native_type"_s, static_cast<qint64>(set->value.nativeType));
        registry.insert(u"raw_base64"_s, QString::fromLatin1(set->value.rawValue.toBase64()));
        result.insert(u"registry"_s, registry);
        result.insert(u"type"_s, u"registry.set_value"_s);
    } else {
        const auto& remove = std::get<DeleteRegistryValueOperation>(operation.change);
        result.insert(u"registry"_s, registryLocationObject(remove.location));
        result.insert(u"type"_s, u"registry.delete_value"_s);
    }
    return result;
}

QJsonObject operationObject(const PlannedRegistryTreeChange& operation)
{
    const auto location = std::visit(
        [](const auto& change) { return change.location; }, operation.change);
    return {
        {u"before_fingerprint"_s, QString::fromLatin1(operation.beforeFingerprint)},
        {u"inputs"_s, inputsObject(operation.inputs)},
        {u"registry"_s, registryKeyObject(location)},
        {u"restart"_s, restartName(operation.restart)},
        {u"target_state"_s, operation.targetState},
        {u"tweak_id"_s, operation.tweakId.toString()},
        {u"type"_s, std::holds_alternative<CreateRegistryKeyOperation>(operation.change)
            ? u"registry.create_key"_s : u"registry.delete_key"_s},
    };
}

QString fileOperationName(domain::FileOperationKind kind)
{
    switch (kind) {
    case domain::FileOperationKind::Copy: return u"file.copy"_s;
    case domain::FileOperationKind::Replace: return u"file.replace"_s;
    case domain::FileOperationKind::Delete: return u"file.delete"_s;
    }
    return {};
}

QJsonObject artifactObject(const domain::InputArtifact& artifact)
{
    return {
        {u"id"_s, artifact.id},
        {u"storage_id"_s, artifact.storageId},
        {u"path"_s, artifact.managedPath},
        {u"size"_s, QString::number(artifact.size)},
        {u"sha256"_s, QString::fromLatin1(artifact.sha256)},
    };
}

QJsonObject operationObject(const PlannedFileChange& operation)
{
    QJsonObject result{
        {u"before_fingerprint"_s, QString::fromLatin1(operation.beforeFingerprint)},
        {u"destination"_s, operation.change.destination},
        {u"inputs"_s, inputsObject(operation.inputs)},
        {u"restart"_s, restartName(operation.restart)},
        {u"target_state"_s, operation.targetState},
        {u"tweak_id"_s, operation.tweakId.toString()},
        {u"type"_s, fileOperationName(operation.change.kind)},
    };
    if (operation.change.kind != domain::FileOperationKind::Delete) {
        result.insert(u"artifact"_s, artifactObject(operation.change.artifact));
    }
    return result;
}

QJsonObject operationObject(const PlannedScheduledTaskChange& operation)
{
    return {
        {u"before_fingerprint"_s, QString::fromLatin1(operation.beforeFingerprint)},
        {u"enabled"_s, operation.change.enabled},
        {u"folder"_s, operation.change.location.folder},
        {u"inputs"_s, inputsObject(operation.inputs)},
        {u"name"_s, operation.change.location.name},
        {u"restart"_s, restartName(operation.restart)},
        {u"target_state"_s, operation.targetState},
        {u"tweak_id"_s, operation.tweakId.toString()},
        {u"type"_s, u"scheduled_task.set_enabled"_s},
    };
}

QJsonObject operationObject(const PlannedBcdElementChange& operation)
{
    QJsonObject result{
        {u"before_fingerprint"_s, QString::fromLatin1(operation.beforeFingerprint)},
        {u"element_type"_s, QString::number(operation.change.spec.elementType)},
        {u"inputs"_s, inputsObject(operation.inputs)},
        {u"object_id"_s, operation.change.spec.objectId},
        {u"restart"_s, restartName(operation.restart)},
        {u"target_state"_s, operation.targetState},
        {u"tweak_id"_s, operation.tweakId.toString()},
        {u"type"_s, operation.change.value ? u"bcd.set_element"_s
                                             : u"bcd.delete_element"_s},
        {u"value_kind"_s, operation.change.spec.valueKind == domain::BcdValueKind::Boolean
            ? u"boolean"_s : operation.change.spec.valueKind == domain::BcdValueKind::Integer
                ? u"integer"_s : u"string"_s},
    };
    if (operation.change.value) {
        result.insert(u"value"_s, domain::bcdValueToJson(*operation.change.value));
    }
    return result;
}

QJsonObject operationObject(const PlannedPowerSettingChange& operation)
{
    return {{u"before_fingerprint"_s, QString::fromLatin1(operation.beforeFingerprint)},
        {u"index"_s, QString::number(operation.change.index)},
        {u"inputs"_s, inputsObject(operation.inputs)},
        {u"restart"_s, restartName(operation.restart)},
        {u"scheme"_s, operation.change.location.scheme},
        {u"source"_s, operation.change.location.source == domain::PowerSource::Ac ? u"ac"_s : u"dc"_s},
        {u"setting"_s, operation.change.location.setting},
        {u"subgroup"_s, operation.change.location.subgroup},
        {u"target_state"_s, operation.targetState}, {u"tweak_id"_s, operation.tweakId.toString()},
        {u"type"_s, u"power.set_index"_s}};
}

QJsonObject operationObject(const PlannedWindowsComponentChange& operation)
{
    return {{u"before_fingerprint"_s, QString::fromLatin1(operation.beforeFingerprint)},
        {u"component_kind"_s, domain::windowsComponentKindName(operation.change.target.kind)},
        {u"inputs"_s, inputsObject(operation.inputs)},
        {u"name"_s, operation.change.target.name},
        {u"restart"_s, restartName(operation.restart)},
        {u"state"_s, domain::windowsComponentStateName(operation.change.state)},
        {u"target_state"_s, operation.targetState},
        {u"tweak_id"_s, operation.tweakId.toString()},
        {u"type"_s, operation.change.target.kind == domain::WindowsComponentKind::Feature
            ? u"windows_feature.set_state"_s : u"windows_capability.set_state"_s}};
}

QJsonObject operationObject(const PlannedAppxRemoval& operation)
{
    return {
        {u"before_fingerprint"_s, QString::fromLatin1(operation.beforeFingerprint)},
        {u"inputs"_s, inputsObject(operation.inputs)},
        {u"package_name"_s, operation.change.packageName},
        {u"restart"_s, restartName(operation.restart)},
        {u"target_state"_s, operation.targetState},
        {u"tweak_id"_s, operation.tweakId.toString()},
        {u"type"_s, u"appx.remove"_s},
    };
}

QJsonObject operationObject(const PlannedFeatureStateChange& operation)
{
    return {
        {u"before_fingerprint"_s, QString::fromLatin1(operation.beforeFingerprint)},
        {u"inputs"_s, inputsObject(operation.inputs)},
        {u"feature_id"_s, static_cast<qint64>(operation.change.featureId)},
        {u"restart"_s, restartName(operation.restart)},
        {u"state"_s, static_cast<qint64>(operation.change.state)},
        {u"target_state"_s, operation.targetState},
        {u"tweak_id"_s, operation.tweakId.toString()},
        {u"type"_s, u"feature.set_state"_s},
    };
}

void addError(ProtocolDecodeResult& result, QString code, QString message)
{
    result.errors.append({.code = std::move(code), .message = std::move(message)});
}

bool validateKeys(
    ProtocolDecodeResult& result,
    const QJsonObject& object,
    const QSet<QString>& allowed)
{
    bool valid = true;
    for (auto iterator = object.constBegin(); iterator != object.constEnd(); ++iterator) {
        if (!allowed.contains(iterator.key())) {
            addError(result, u"field.unknown"_s, u"Неизвестное поле JSON: "_s + iterator.key());
            valid = false;
        }
    }
    return valid;
}

std::optional<domain::TweakInputMap> parseInputs(
    ProtocolDecodeResult& result,
    const QJsonValue& value)
{
    domain::TweakInputMap inputs;
    if (value.isUndefined()) return inputs;
    if (!value.isObject()) {
        addError(result, u"input.invalid"_s, u"Параметры операции должны быть объектом."_s);
        return std::nullopt;
    }
    const auto object = value.toObject();
    if (object.size() > 64) {
        addError(result, u"input.too_many"_s, u"Слишком много параметров операции."_s);
        return std::nullopt;
    }
    static const QRegularExpression idPattern(u"^[a-z][a-z0-9_-]{0,63}$"_s);
    for (auto iterator = object.constBegin(); iterator != object.constEnd(); ++iterator) {
        if (!idPattern.match(iterator.key()).hasMatch() || !iterator->isObject()) {
            addError(result, u"input.invalid"_s, u"Некорректный параметр операции."_s);
            continue;
        }
        const auto encoded = iterator->toObject();
        const auto type = encoded.value(u"type"_s).toString();
        if (type == u"artifact") {
            validateKeys(result, encoded, {
                u"type"_s, u"id"_s, u"storage_id"_s, u"path"_s,
                u"size"_s, u"sha256"_s,
            });
            bool sizeOk{};
            const auto size = encoded.value(u"size"_s).toString().toULongLong(&sizeOk);
            const auto hash = encoded.value(u"sha256"_s).toString().toLatin1();
            const auto path = QDir::cleanPath(encoded.value(u"path"_s).toString());
            if (!sizeOk || hash.size() != 64 || QDir::isAbsolutePath(path)
                || path == u".."_s || path.startsWith(u"../"_s)
                || !(path.startsWith(u"inputs/"_s) || path.startsWith(u"inputs\\"_s))) {
                addError(result, u"input.invalid"_s, u"Некорректный файловый параметр."_s);
            } else {
                inputs.insert(iterator.key(), domain::InputArtifact{
                    .id = encoded.value(u"id"_s).toString(),
                    .storageId = encoded.value(u"storage_id"_s).toString(),
                    .managedPath = path,
                    .size = size,
                    .sha256 = hash,
                });
            }
            continue;
        }
        validateKeys(result, encoded, {u"type"_s, u"value"_s});
        const auto encodedValue = encoded.value(u"value"_s);
        if (type == u"string" && encodedValue.isString()
            && encodedValue.toString().size() <= 32768) {
            inputs.insert(iterator.key(), encodedValue.toString());
        } else if (type == u"integer" && encodedValue.isString()) {
            bool ok{};
            const auto integer = encodedValue.toString().toLongLong(&ok, 10);
            if (ok) inputs.insert(iterator.key(), integer);
            else addError(result, u"input.invalid"_s, u"Некорректное целое значение параметра."_s);
        } else if (type == u"boolean" && encodedValue.isBool()) {
            inputs.insert(iterator.key(), encodedValue.toBool());
        } else {
            addError(result, u"input.invalid"_s, u"Тип или значение параметра недопустимы."_s);
        }
    }
    if (!result.errors.isEmpty()) return std::nullopt;
    return inputs;
}

std::optional<WindowsFamily> parseFamily(ProtocolDecodeResult& result, const QString& value)
{
    if (value == u"windows10") return WindowsFamily::Windows10;
    if (value == u"windows11") return WindowsFamily::Windows11;
    addError(result, u"profile.family_unknown"_s, u"Неизвестное семейство Windows."_s);
    return std::nullopt;
}

std::optional<CpuArchitecture> parseArchitecture(ProtocolDecodeResult& result, const QString& value)
{
    if (value == u"x64") return CpuArchitecture::X64;
    addError(result, u"profile.architecture_unknown"_s, u"Executor принимает только x64."_s);
    return std::nullopt;
}

std::optional<RestartRequirement> parseRestart(ProtocolDecodeResult& result, const QString& value)
{
    if (value == u"none") return RestartRequirement::None;
    if (value == u"explorer") return RestartRequirement::Explorer;
    if (value == u"service") return RestartRequirement::Service;
    if (value == u"sign_out") return RestartRequirement::SignOut;
    if (value == u"reboot") return RestartRequirement::Reboot;
    addError(result, u"restart.unknown"_s, u"Неизвестное требование перезапуска."_s);
    return std::nullopt;
}

std::optional<RegistryHive> parseHive(ProtocolDecodeResult& result, const QString& value)
{
    if (value == u"HKLM") return RegistryHive::LocalMachine;
    if (value == u"HKCU") return RegistryHive::CurrentUser;
    if (value == u"HKCR") return RegistryHive::ClassesRoot;
    if (value == u"HKU") return RegistryHive::Users;
    addError(result, u"registry.hive_unknown"_s, u"Hive реестра не входит в whitelist."_s);
    return std::nullopt;
}

std::optional<RegistryView> parseView(ProtocolDecodeResult& result, const QString& value)
{
    if (value == u"default") return RegistryView::Default;
    if (value == u"registry32") return RegistryView::Registry32;
    if (value == u"registry64") return RegistryView::Registry64;
    addError(result, u"registry.view_unknown"_s, u"Неизвестное представление реестра."_s);
    return std::nullopt;
}

std::optional<SystemProfile> parseProfile(ProtocolDecodeResult& result, const QJsonValue& value)
{
    if (!value.isObject()) {
        addError(result, u"profile.invalid"_s, u"Профиль ОС должен быть объектом."_s);
        return std::nullopt;
    }
    const auto object = value.toObject();
    validateKeys(result, object, {u"architecture"_s, u"build"_s, u"edition"_s, u"family"_s, u"ubr"_s});
    const auto family = parseFamily(result, object.value(u"family"_s).toString());
    const auto architecture = parseArchitecture(result, object.value(u"architecture"_s).toString());
    if (!family || !architecture || !object.value(u"build"_s).isDouble() || !object.value(u"ubr"_s).isDouble()) {
        if (!object.value(u"build"_s).isDouble() || !object.value(u"ubr"_s).isDouble()) {
            addError(result, u"profile.invalid"_s, u"Build и UBR должны быть числами."_s);
        }
        return std::nullopt;
    }
    return SystemProfile{
        .family = *family,
        .build = static_cast<quint32>(object.value(u"build"_s).toInteger()),
        .ubr = static_cast<quint32>(object.value(u"ubr"_s).toInteger()),
        .edition = object.value(u"edition"_s).toString(),
        .architecture = *architecture,
    };
}

std::optional<RegistryLocation> parseRegistryLocationObject(
    ProtocolDecodeResult& result,
    const QJsonObject& registry,
    const QSet<QString>& allowedKeys)
{
    validateKeys(result, registry, allowedKeys);
    const auto hive = parseHive(result, registry.value(u"hive"_s).toString());
    const auto view = parseView(result, registry.value(u"view"_s).toString());
    const auto key = registry.value(u"key"_s).toString();
    const auto valueName = registry.value(u"value_name"_s).toString();
    if (!hive || !view) return std::nullopt;
    if (key.isEmpty() || valueName.isEmpty()) {
        addError(result, u"registry.path_invalid"_s, u"Путь значения реестра пуст."_s);
        return std::nullopt;
    }
    return RegistryLocation{
        .hive = *hive,
        .key = key,
        .valueName = valueName,
        .view = *view,
    };
}

std::optional<RegistryKeyLocation> parseRegistryKeyObject(
    ProtocolDecodeResult& result,
    const QJsonObject& registry)
{
    validateKeys(result, registry, {u"hive"_s, u"key"_s, u"view"_s});
    const auto hive = parseHive(result, registry.value(u"hive"_s).toString());
    const auto view = parseView(result, registry.value(u"view"_s).toString());
    const auto key = registry.value(u"key"_s).toString();
    if (!hive || !view) return std::nullopt;
    if (key.isEmpty()) {
        addError(result, u"registry.path_invalid"_s, u"Путь раздела реестра пуст."_s);
        return std::nullopt;
    }
    return RegistryKeyLocation{.hive = *hive, .key = key, .view = *view};
}

bool supportedNativeType(qint64 nativeType)
{
    return nativeType == 1 || nativeType == 2 || nativeType == 3
        || nativeType == 4 || nativeType == 7 || nativeType == 11;
}

std::optional<PlannedOperation> parseOperation(ProtocolDecodeResult& result, const QJsonValue& value)
{
    if (!value.isObject()) {
        addError(result, u"operation.invalid"_s, u"Операция должна быть объектом."_s);
        return std::nullopt;
    }
    const auto object = value.toObject();
    const auto type = object.value(u"type"_s).toString();
    if (type != u"registry.set_dword" && type != u"registry.set_value"
        && type != u"registry.delete_value" && type != u"appx.remove"
        && type != u"registry.create_key" && type != u"registry.delete_key"
        && type != u"feature.set_state" && type != u"file.copy"
        && type != u"file.replace" && type != u"file.delete"
        && type != u"scheduled_task.set_enabled"
        && type != u"bcd.set_element" && type != u"bcd.delete_element"
        && type != u"power.set_index"
        && type != u"windows_feature.set_state"
        && type != u"windows_capability.set_state") {
        addError(result, u"operation.unknown"_s, u"Тип операции не входит в whitelist."_s);
        return std::nullopt;
    }
    const auto id = TweakId::parse(object.value(u"tweak_id"_s).toString());
    const auto restart = parseRestart(result, object.value(u"restart"_s).toString());
    const auto targetState = object.value(u"target_state"_s).toString();
    const auto inputs = parseInputs(result, object.value(u"inputs"_s));
    if (!id || !restart || !inputs || targetState.isEmpty()) {
        if (!id) addError(result, u"id.invalid"_s, u"Некорректный ID твика."_s);
        if (targetState.isEmpty()) {
            addError(result, u"operation.invalid"_s, u"Целевое состояние отсутствует."_s);
        }
        return std::nullopt;
    }

    if (type == u"registry.create_key" || type == u"registry.delete_key") {
        validateKeys(result, object, {
            u"before_fingerprint"_s, u"registry"_s, u"restart"_s,
            u"target_state"_s, u"tweak_id"_s, u"type"_s, u"inputs"_s,
        });
        const auto registryValue = object.value(u"registry"_s);
        if (!registryValue.isObject()) {
            addError(result, u"registry.invalid"_s, u"Параметры реестра должны быть объектом."_s);
            return std::nullopt;
        }
        const auto location = parseRegistryKeyObject(result, registryValue.toObject());
        if (!location) return std::nullopt;
        planning::RegistryTreeChange change = type == u"registry.create_key"
            ? planning::RegistryTreeChange{CreateRegistryKeyOperation{.location = *location}}
            : planning::RegistryTreeChange{DeleteRegistryTreeOperation{.location = *location}};
        return PlannedRegistryTreeChange{
            .tweakId = *id,
            .targetState = targetState,
            .change = std::move(change),
            .beforeFingerprint = object.value(u"before_fingerprint"_s).toString().toLatin1(),
            .restart = *restart,
            .inputs = *inputs,
        };
    }

    if (type == u"registry.set_value" || type == u"registry.delete_value") {
        validateKeys(result, object, {
            u"before_fingerprint"_s, u"registry"_s, u"restart"_s,
            u"target_state"_s, u"tweak_id"_s, u"type"_s, u"inputs"_s,
        });
        const auto registryValue = object.value(u"registry"_s);
        if (!registryValue.isObject()) {
            addError(result, u"registry.invalid"_s, u"Параметры реестра должны быть объектом."_s);
            return std::nullopt;
        }
        const auto registry = registryValue.toObject();
        const auto location = parseRegistryLocationObject(
            result,
            registry,
            type == u"registry.set_value"
                ? QSet<QString>{u"hive"_s, u"key"_s, u"native_type"_s,
                                u"raw_base64"_s, u"value_name"_s, u"view"_s}
                : QSet<QString>{u"hive"_s, u"key"_s, u"value_name"_s, u"view"_s});
        if (!location) return std::nullopt;

        planning::RegistryValueChange change;
        if (type == u"registry.delete_value") {
            change = DeleteRegistryValueOperation{.location = *location};
        } else {
            const auto nativeType = registry.value(u"native_type"_s).toInteger(-1);
            const auto decoded = QByteArray::fromBase64Encoding(
                registry.value(u"raw_base64"_s).toString().toLatin1(),
                QByteArray::AbortOnBase64DecodingErrors);
            if (!supportedNativeType(nativeType)) {
                addError(result, u"registry.native_type_unknown"_s,
                         u"Native type значения реестра не поддерживается."_s);
                return std::nullopt;
            }
            if (!decoded) {
                addError(result, u"registry.value_invalid"_s,
                         u"Байты значения реестра имеют неверный Base64."_s);
                return std::nullopt;
            }
            if (decoded.decoded.size() > 1024 * 1024) {
                addError(result, u"registry.value_too_large"_s,
                         u"Значение реестра превышает допустимый размер."_s);
                return std::nullopt;
            }
            change = SetRegistryValueOperation{
                .location = *location,
                .value = RegistryValueSpec{
                    .nativeType = static_cast<quint32>(nativeType),
                    .rawValue = decoded.decoded,
                },
            };
        }
        return PlannedRegistryValueChange{
            .tweakId = *id,
            .targetState = targetState,
            .change = std::move(change),
            .beforeFingerprint = object.value(u"before_fingerprint"_s).toString().toLatin1(),
            .restart = *restart,
            .inputs = *inputs,
        };
    }

    if (type == u"file.copy" || type == u"file.replace" || type == u"file.delete") {
        validateKeys(result, object, {
            u"artifact"_s, u"before_fingerprint"_s, u"destination"_s, u"inputs"_s,
            u"restart"_s, u"target_state"_s, u"tweak_id"_s, u"type"_s,
        });
        const auto destination = object.value(u"destination"_s).toString();
        if (!QDir::isAbsolutePath(destination) || QDir::cleanPath(destination) != destination) {
            addError(result, u"file.destination_invalid"_s,
                     u"Путь назначения файла недопустим."_s);
            return std::nullopt;
        }
        domain::InputArtifact artifact;
        const auto kind = type == u"file.copy" ? domain::FileOperationKind::Copy
            : type == u"file.replace" ? domain::FileOperationKind::Replace
                                       : domain::FileOperationKind::Delete;
        if (kind != domain::FileOperationKind::Delete) {
            const auto encoded = object.value(u"artifact"_s);
            if (!encoded.isObject()) {
                addError(result, u"file.artifact_invalid"_s,
                         u"Файловая операция не содержит артефакт."_s);
                return std::nullopt;
            }
            const auto value = encoded.toObject();
            validateKeys(result, value, {
                u"id"_s, u"storage_id"_s, u"path"_s, u"size"_s, u"sha256"_s,
            });
            bool sizeOk{};
            const auto size = value.value(u"size"_s).toString().toULongLong(&sizeOk);
            const auto hash = value.value(u"sha256"_s).toString().toLatin1();
            const auto path = QDir::cleanPath(value.value(u"path"_s).toString());
            if (!sizeOk || hash.size() != 64 || path.isEmpty() || QDir::isAbsolutePath(path)
                || path == u".."_s || path.startsWith(u"../"_s)
                || !(path.startsWith(u"inputs/"_s) || path.startsWith(u"inputs\\"_s))) {
                addError(result, u"file.artifact_invalid"_s,
                         u"Ссылка на файловый артефакт недопустима."_s);
                return std::nullopt;
            }
            artifact = {
                .id = value.value(u"id"_s).toString(),
                .storageId = value.value(u"storage_id"_s).toString(),
                .managedPath = path,
                .size = size,
                .sha256 = hash,
            };
        }
        return PlannedFileChange{
            .tweakId = *id,
            .targetState = targetState,
            .change = domain::FileOperation{
                .kind = kind, .artifact = artifact, .destination = destination},
            .beforeFingerprint = object.value(u"before_fingerprint"_s).toString().toLatin1(),
            .restart = *restart,
            .inputs = *inputs,
        };
    }

    if (type == u"scheduled_task.set_enabled") {
        validateKeys(result, object, {
            u"before_fingerprint"_s, u"enabled"_s, u"folder"_s, u"inputs"_s,
            u"name"_s, u"restart"_s, u"target_state"_s, u"tweak_id"_s, u"type"_s,
        });
        const auto folder = object.value(u"folder"_s).toString();
        const auto name = object.value(u"name"_s).toString();
        if (!object.value(u"enabled"_s).isBool() || !folder.startsWith(u'\\')
            || folder.contains(u".."_s) || name.trimmed().isEmpty()
            || name.contains(u'\\') || name.contains(u'/')) {
            addError(result, u"scheduled_task.location_invalid"_s,
                     u"Параметры задачи планировщика недопустимы."_s);
            return std::nullopt;
        }
        return PlannedScheduledTaskChange{
            .tweakId = *id,
            .targetState = targetState,
            .change = SetScheduledTaskEnabledOperation{
                .location = {.folder = folder, .name = name},
                .enabled = object.value(u"enabled"_s).toBool(),
            },
            .beforeFingerprint = object.value(u"before_fingerprint"_s).toString().toLatin1(),
            .restart = *restart,
            .inputs = *inputs,
        };
    }

    if (type == u"bcd.set_element" || type == u"bcd.delete_element") {
        validateKeys(result, object, {
            u"before_fingerprint"_s, u"element_type"_s, u"inputs"_s,
            u"object_id"_s, u"restart"_s, u"target_state"_s, u"tweak_id"_s,
            u"type"_s, u"value"_s, u"value_kind"_s,
        });
        bool typeOk{};
        const auto elementType = object.value(u"element_type"_s).toString().toUInt(&typeOk, 10);
        const auto kindName = object.value(u"value_kind"_s).toString();
        const auto kind = kindName == u"boolean" ? std::optional{domain::BcdValueKind::Boolean}
            : kindName == u"integer" ? std::optional{domain::BcdValueKind::Integer}
            : kindName == u"string" ? std::optional{domain::BcdValueKind::String}
                                      : std::nullopt;
        if (!typeOk || !kind) {
            addError(result, u"bcd.element_invalid"_s, u"BCD element type недопустим."_s);
            return std::nullopt;
        }
        const domain::BcdElementSpec spec{
            .objectId = object.value(u"object_id"_s).toString(),
            .elementType = elementType,
            .valueKind = *kind,
        };
        if (!domain::isWhitelistedBcdElement(spec)) {
            addError(result, u"bcd.element_not_whitelisted"_s,
                     u"BCD-объект или element type не входит в whitelist."_s);
            return std::nullopt;
        }
        std::optional<domain::BcdValue> bcdValue;
        if (type == u"bcd.set_element") {
            if (!object.value(u"value"_s).isObject()) {
                addError(result, u"bcd.value_invalid"_s, u"BCD-значение отсутствует."_s);
                return std::nullopt;
            }
            bcdValue = domain::bcdValueFromJson(object.value(u"value"_s).toObject(), *kind);
            if (!bcdValue) {
                addError(result, u"bcd.value_invalid"_s,
                         u"BCD-значение имеет неверный тип."_s);
                return std::nullopt;
            }
        }
        return PlannedBcdElementChange{
            .tweakId = *id,
            .targetState = targetState,
            .change = {.spec = spec, .value = std::move(bcdValue)},
            .beforeFingerprint = object.value(u"before_fingerprint"_s).toString().toLatin1(),
            .restart = *restart,
            .inputs = *inputs,
        };
    }

    if (type == u"power.set_index") {
        validateKeys(result, object, {u"before_fingerprint"_s, u"index"_s, u"inputs"_s,
            u"restart"_s, u"scheme"_s, u"source"_s, u"setting"_s, u"subgroup"_s,
            u"target_state"_s, u"tweak_id"_s, u"type"_s});
        bool indexOk{};
        const auto index = object.value(u"index"_s).toString().toUInt(&indexOk, 10);
        const auto sourceName = object.value(u"source"_s).toString();
        const auto source = sourceName == u"ac" ? std::optional{domain::PowerSource::Ac}
            : sourceName == u"dc" ? std::optional{domain::PowerSource::Dc} : std::nullopt;
        domain::PowerSettingLocation location{object.value(u"scheme"_s).toString(),
            object.value(u"subgroup"_s).toString(), object.value(u"setting"_s).toString(),
            source.value_or(domain::PowerSource::Ac)};
        if (!indexOk || !source || !domain::isValidPowerLocation(location)) {
            addError(result, u"power.location_invalid"_s, u"Параметры схемы питания недопустимы."_s);
            return std::nullopt;
        }
        return PlannedPowerSettingChange{.tweakId = *id, .targetState = targetState,
            .change = {location, index},
            .beforeFingerprint = object.value(u"before_fingerprint"_s).toString().toLatin1(),
            .restart = *restart, .inputs = *inputs};
    }

    if (type == u"windows_feature.set_state" || type == u"windows_capability.set_state") {
        validateKeys(result, object, {u"before_fingerprint"_s, u"component_kind"_s,
            u"inputs"_s, u"name"_s, u"restart"_s, u"state"_s,
            u"target_state"_s, u"tweak_id"_s, u"type"_s});
        const auto impliedKind = type == u"windows_feature.set_state"
            ? domain::WindowsComponentKind::Feature : domain::WindowsComponentKind::Capability;
        const auto encodedKind = domain::parseWindowsComponentKind(
            object.value(u"component_kind"_s).toString());
        const auto state = domain::parseWindowsComponentState(object.value(u"state"_s).toString());
        domain::WindowsComponentTarget target{impliedKind, object.value(u"name"_s).toString()};
        if (!encodedKind || *encodedKind != impliedKind || !state
            || *state == domain::WindowsComponentState::Unsupported
            || (impliedKind == domain::WindowsComponentKind::Capability
                && *state == domain::WindowsComponentState::Disabled)
            || !domain::isValidWindowsComponentTarget(target)) {
            addError(result, u"windows_component.invalid"_s,
                     u"Параметры компонента Windows недопустимы."_s);
            return std::nullopt;
        }
        return PlannedWindowsComponentChange{
            .tweakId = *id, .targetState = targetState,
            .change = {target, *state},
            .beforeFingerprint = object.value(u"before_fingerprint"_s).toString().toLatin1(),
            .restart = *restart, .inputs = *inputs};
    }

    if (type == u"appx.remove") {
        validateKeys(result, object, {
            u"before_fingerprint"_s, u"package_name"_s, u"restart"_s,
            u"target_state"_s, u"tweak_id"_s, u"type"_s, u"inputs"_s,
        });
        const auto packageName = object.value(u"package_name"_s).toString();
        static const QRegularExpression packageNamePattern(u"^[A-Za-z0-9][A-Za-z0-9.-]{0,199}$"_s);
        if (!packageNamePattern.match(packageName).hasMatch()) {
            addError(result, u"appx.package_name_invalid"_s,
                     u"Имя AppX-пакета содержит недопустимые символы."_s);
            return std::nullopt;
        }
        return PlannedAppxRemoval{
            .tweakId = *id,
            .targetState = targetState,
            .change = RemoveAppxPackageOperation{.packageName = packageName},
            .beforeFingerprint = object.value(u"before_fingerprint"_s).toString().toLatin1(),
            .restart = *restart,
            .inputs = *inputs,
        };
    }

    if (type == u"feature.set_state") {
        validateKeys(result, object, {
            u"before_fingerprint"_s, u"feature_id"_s, u"restart"_s, u"state"_s,
            u"target_state"_s, u"tweak_id"_s, u"type"_s, u"inputs"_s,
        });
        const auto featureId = object.value(u"feature_id"_s).toInteger();
        const auto state = object.value(u"state"_s).toInteger(-1);
        if (featureId <= 0 || featureId > std::numeric_limits<quint32>::max()
            || state < 0 || state > 2) {
            addError(result, u"feature.invalid"_s, u"Feature ID или состояние недопустимы."_s);
            return std::nullopt;
        }
        return PlannedFeatureStateChange{
            .tweakId = *id,
            .targetState = targetState,
            .change = SetFeatureStateOperation{
                .featureId = static_cast<quint32>(featureId),
                .state = static_cast<FeatureEnabledState>(state),
            },
            .beforeFingerprint = object.value(u"before_fingerprint"_s).toString().toLatin1(),
            .restart = *restart,
            .inputs = *inputs,
        };
    }

    validateKeys(result, object, {
        u"before_fingerprint"_s, u"registry"_s, u"restart"_s,
        u"target_state"_s, u"tweak_id"_s, u"type"_s, u"inputs"_s,
    });
    const auto registryValue = object.value(u"registry"_s);
    if (!registryValue.isObject()) {
        addError(result, u"registry.invalid"_s, u"Параметры реестра должны быть объектом."_s);
        return std::nullopt;
    }
    const auto registry = registryValue.toObject();
    validateKeys(result, registry, {u"hive"_s, u"key"_s, u"value"_s, u"value_name"_s, u"view"_s});
    const auto hive = parseHive(result, registry.value(u"hive"_s).toString());
    const auto view = parseView(result, registry.value(u"view"_s).toString());
    const auto rawValue = registry.value(u"value"_s).toInteger(-1);
    if (!hive || !view || rawValue < 0 || rawValue > std::numeric_limits<quint32>::max()) {
        if (rawValue < 0 || rawValue > std::numeric_limits<quint32>::max()) {
            addError(result, u"value.invalid_dword"_s, u"DWORD выходит за допустимый диапазон."_s);
        }
        return std::nullopt;
    }
    const auto key = registry.value(u"key"_s).toString();
    const auto valueName = registry.value(u"value_name"_s).toString();
    if (key.isEmpty() || valueName.isEmpty() || targetState.isEmpty()) {
        addError(result, u"operation.invalid"_s, u"Операция содержит пустое обязательное поле."_s);
        return std::nullopt;
    }
    return PlannedRegistryDwordChange{
        .tweakId = *id,
        .targetState = targetState,
        .change = SetRegistryDwordOperation{
            .location = {.hive = *hive, .key = key, .valueName = valueName, .view = *view},
            .value = static_cast<quint32>(rawValue),
        },
        .beforeFingerprint = object.value(u"before_fingerprint"_s).toString().toLatin1(),
        .restart = *restart,
        .inputs = *inputs,
    };
}

} // namespace

QJsonObject ExecutionProtocol::bodyObject(const planning::ExecutionPlan& plan)
{
    QJsonArray operations;
    for (const auto& operation : plan.operations) {
        operations.append(std::visit([](const auto& value) { return operationObject(value); }, operation));
    }
    return {
        {u"created_at_utc"_s, plan.createdAtUtc.toUTC().toString(Qt::ISODateWithMs)},
        {u"operations"_s, operations},
        {u"profile"_s, profileObject(plan.profile)},
        {u"restart"_s, restartName(plan.restart)},
        {u"schema_version"_s, plan.schemaVersion},
        {u"summary"_s, plan.summary},
        {u"transaction_id"_s, plan.transactionId.toString(QUuid::WithoutBraces)},
    };
}

QByteArray ExecutionProtocol::canonicalHash(const QJsonObject& body)
{
    return QCryptographicHash::hash(
        QJsonDocument(body).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex();
}

QByteArray ExecutionProtocol::bodyHash(const planning::ExecutionPlan& plan)
{
    return canonicalHash(bodyObject(plan));
}

QByteArray ExecutionProtocol::encode(const planning::ExecutionPlan& plan)
{
    const auto body = bodyObject(plan);
    const QJsonObject envelope{
        {u"body"_s, body},
        {u"sha256"_s, QString::fromLatin1(canonicalHash(body))},
    };
    return QJsonDocument(envelope).toJson(QJsonDocument::Compact);
}

ProtocolDecodeResult ExecutionProtocol::decode(const QByteArray& json)
{
    ProtocolDecodeResult result;
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        addError(result, u"json.invalid"_s, u"Некорректный JSON плана."_s);
        return result;
    }
    const auto envelope = document.object();
    validateKeys(result, envelope, {u"body"_s, u"sha256"_s});
    if (!envelope.value(u"body"_s).isObject()) {
        addError(result, u"body.invalid"_s, u"Тело плана отсутствует."_s);
        return result;
    }
    const auto body = envelope.value(u"body"_s).toObject();
    result.bodyHash = canonicalHash(body);
    if (envelope.value(u"sha256"_s).toString().toLatin1() != result.bodyHash) {
        addError(result, u"hash.mismatch"_s, u"SHA-256 тела плана не совпадает."_s);
        return result;
    }
    validateKeys(result, body, {
        u"created_at_utc"_s, u"operations"_s, u"profile"_s, u"restart"_s,
        u"schema_version"_s, u"summary"_s, u"transaction_id"_s,
    });
    const auto schemaVersion = body.value(u"schema_version"_s).toInt(-1);
    if (schemaVersion != planning::executionPlanSchemaVersion) {
        addError(result, u"schema.unsupported"_s, u"Версия протокола плана не поддерживается."_s);
    }
    const auto profile = parseProfile(result, body.value(u"profile"_s));
    const auto restart = parseRestart(result, body.value(u"restart"_s).toString());
    const auto transactionId = QUuid(body.value(u"transaction_id"_s).toString());
    const auto createdAt = QDateTime::fromString(body.value(u"created_at_utc"_s).toString(), Qt::ISODateWithMs);
    if (transactionId.isNull()) addError(result, u"transaction.invalid_id"_s, u"Некорректный UUID транзакции."_s);
    if (!createdAt.isValid()) addError(result, u"plan.invalid_time"_s, u"Некорректное время создания плана."_s);

    QVector<PlannedOperation> operations;
    const auto operationsValue = body.value(u"operations"_s);
    if (!operationsValue.isArray()) {
        addError(result, u"operations.invalid"_s, u"operations должен быть массивом."_s);
    } else {
        for (const auto& operationValue : operationsValue.toArray()) {
            if (const auto operation = parseOperation(result, operationValue)) {
                operations.append(*operation);
            }
        }
    }
    if (!result.errors.isEmpty() || !profile || !restart) {
        return result;
    }
    result.plan = ExecutionPlan{
        .schemaVersion = schemaVersion,
        .transactionId = transactionId,
        .profile = *profile,
        .createdAtUtc = createdAt.toUTC(),
        .operations = std::move(operations),
        .summary = body.value(u"summary"_s).toString(),
        .restart = *restart,
    };
    return result;
}

} // namespace tweakopedia::execution
