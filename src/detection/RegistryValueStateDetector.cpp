#include "detection/RegistryValueStateDetector.h"

#include "detection/CompatibilityEvaluator.h"

#include <QCryptographicHash>
#include <QJsonDocument>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace tweakopedia::detection {
namespace {

QByteArray fingerprint(const platform::RegistryReadResult& value)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArray::number(static_cast<int>(value.presence)));
    hash.addData("|");
    hash.addData(QByteArray::number(static_cast<int>(value.type)));
    hash.addData("|");
    hash.addData(QByteArray::number(value.nativeType));
    hash.addData("|");
    hash.addData(value.rawValue);
    return hash.result().toHex();
}

std::optional<domain::RegistryLocation> registryLocation(const domain::OperationSpec& operation)
{
    return std::visit([](const auto& value) -> std::optional<domain::RegistryLocation> {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, domain::SetRegistryDwordOperation>
                      || std::is_same_v<T, domain::SetRegistryValueOperation>
                      || std::is_same_v<T, domain::DeleteRegistryValueOperation>) {
            return value.location;
        }
        return std::nullopt;
    }, operation);
}

std::optional<domain::RegistryKeyLocation> registryTreeLocation(
    const domain::OperationSpec& operation)
{
    return std::visit([](const auto& value) -> std::optional<domain::RegistryKeyLocation> {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, domain::CreateRegistryKeyOperation>
                      || std::is_same_v<T, domain::DeleteRegistryTreeOperation>) {
            return value.location;
        }
        return std::nullopt;
    }, operation);
}

QByteArray treeFingerprint(const domain::RegistryTreeSnapshot& snapshot)
{
    return QCryptographicHash::hash(
        QJsonDocument(snapshot.toJson()).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex();
}

domain::DetectedState unknown(
    QString details,
    QByteArray stateFingerprint = {},
    QVector<domain::DetectedRegistryFingerprint> registryFingerprints = {},
    QVector<domain::DetectedRegistryTreeFingerprint> treeFingerprints = {})
{
    return {
        .status = domain::DetectionStatus::Unknown,
        .stateId = u"unknown"_s,
        .details = std::move(details),
        .fingerprint = std::move(stateFingerprint),
        .registryFingerprints = std::move(registryFingerprints),
        .registryTreeFingerprints = std::move(treeFingerprints),
    };
}

} // namespace

domain::DetectedState RegistryValueStateDetector::detect(
    const domain::TweakDefinition& tweak,
    const platform::IRegistryBackend& backend,
    const domain::SystemProfile& profile) const
{
    const auto compatibility = evaluate(tweak, profile);
    if (!compatibility.supported) {
        return {
            .status = domain::DetectionStatus::Unsupported,
            .stateId = u"unsupported"_s,
            .details = compatibility.explanation,
        };
    }
    if (!tweak.valueDetection) {
        return unknown(u"В определении отсутствует способ чтения состояния."_s);
    }

    const auto value = backend.read(tweak.valueDetection->location);
    const auto stateFingerprint = fingerprint(value);
    QVector<domain::DetectedRegistryFingerprint> registryFingerprints{
        {.location = tweak.valueDetection->location, .fingerprint = stateFingerprint},
    };
    QVector<domain::DetectedRegistryTreeFingerprint> treeFingerprints;
    for (const auto& state : tweak.states) {
        for (const auto& operation : state.operations) {
            const auto location = registryLocation(operation);
            if (location) {
                const auto captured = std::any_of(
                    registryFingerprints.cbegin(), registryFingerprints.cend(),
                    [&](const auto& item) { return item.location == *location; });
                if (!captured) {
                    registryFingerprints.append({
                        .location = *location,
                        .fingerprint = fingerprint(backend.read(*location)),
                    });
                }
            }
            const auto treeLocation = registryTreeLocation(operation);
            if (treeLocation) {
                const auto captured = std::any_of(
                    treeFingerprints.cbegin(), treeFingerprints.cend(),
                    [&](const auto& item) { return item.location == *treeLocation; });
                if (!captured) {
                    const auto tree = backend.readTree(*treeLocation);
                    if (tree.success) {
                        treeFingerprints.append({
                            .location = *treeLocation,
                            .fingerprint = treeFingerprint(tree.snapshot),
                        });
                    }
                }
            }
        }
    }

    if (value.presence == platform::RegistryPresence::Error) {
        return unknown(
            u"Не удалось прочитать значение реестра: "_s
                + QString::fromLocal8Bit(value.error.message()),
            stateFingerprint,
            std::move(registryFingerprints),
            std::move(treeFingerprints));
    }
    if (value.presence == platform::RegistryPresence::Missing) {
        return {
            .status = domain::DetectionStatus::Named,
            .stateId = tweak.valueDetection->missingState,
            .details = u"Значение отсутствует; применяется объявленное состояние."_s,
            .fingerprint = stateFingerprint,
            .registryFingerprints = std::move(registryFingerprints),
            .registryTreeFingerprints = std::move(treeFingerprints),
        };
    }

    for (auto iterator = tweak.valueDetection->valuesByState.cbegin();
         iterator != tweak.valueDetection->valuesByState.cend(); ++iterator) {
        if (value.nativeType == iterator.value().nativeType
            && value.rawValue == iterator.value().rawValue) {
            return {
                .status = domain::DetectionStatus::Named,
                .stateId = iterator.key(),
                .details = u"Состояние определено по точному типу и байтам значения."_s,
                .fingerprint = stateFingerprint,
                .registryFingerprints = std::move(registryFingerprints),
                .registryTreeFingerprints = std::move(treeFingerprints),
            };
        }
    }
    return {
        .status = domain::DetectionStatus::Custom,
        .stateId = u"custom"_s,
        .details = u"Значение не сопоставлено с именованным состоянием."_s,
        .fingerprint = stateFingerprint,
        .registryFingerprints = std::move(registryFingerprints),
        .registryTreeFingerprints = std::move(treeFingerprints),
    };
}

} // namespace tweakopedia::detection
