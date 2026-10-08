#include "detection/RegistryDwordStateDetector.h"

#include "detection/CompatibilityEvaluator.h"

#include <QCryptographicHash>
#include <QtEndian>

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

domain::DetectedState unknown(
    QString details,
    QByteArray stateFingerprint = {},
    QVector<domain::DetectedRegistryFingerprint> registryFingerprints = {})
{
    return {
        .status = domain::DetectionStatus::Unknown,
        .stateId = u"unknown"_s,
        .details = std::move(details),
        .fingerprint = std::move(stateFingerprint),
        .registryFingerprints = std::move(registryFingerprints),
    };
}

} // namespace

domain::DetectedState RegistryDwordStateDetector::detect(
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
    if (!tweak.detection) {
        return unknown(u"В определении отсутствует способ чтения состояния."_s);
    }

    const auto value = backend.read(tweak.detection->location);
    const auto stateFingerprint = fingerprint(value);
    QVector<domain::DetectedRegistryFingerprint> registryFingerprints{
        {.location = tweak.detection->location, .fingerprint = stateFingerprint},
    };
    for (const auto& state : tweak.states) {
        for (const auto& operation : state.operations) {
            const auto* registry = std::get_if<domain::SetRegistryDwordOperation>(&operation);
            if (!registry) continue;
            const auto alreadyCaptured = std::any_of(
                registryFingerprints.cbegin(), registryFingerprints.cend(),
                [&](const domain::DetectedRegistryFingerprint& item) {
                    return item.location == registry->location;
                });
            if (alreadyCaptured) continue;
            registryFingerprints.append({
                .location = registry->location,
                .fingerprint = fingerprint(backend.read(registry->location)),
            });
        }
    }
    if (value.presence == platform::RegistryPresence::Error) {
        return unknown(
            u"Не удалось прочитать значение реестра: "_s
                + QString::fromLocal8Bit(value.error.message()),
            stateFingerprint,
            std::move(registryFingerprints));
    }
    if (value.presence == platform::RegistryPresence::Missing) {
        return {
            .status = domain::DetectionStatus::Named,
            .stateId = tweak.detection->missingState,
            .details = u"Значение отсутствует; применяется объявленное состояние по умолчанию."_s,
            .fingerprint = stateFingerprint,
            .registryFingerprints = std::move(registryFingerprints),
        };
    }
    if (value.type != platform::RegistryValueType::Dword || value.rawValue.size() != sizeof(quint32)) {
        return unknown(
            u"Значение существует, но имеет неожиданный тип или размер."_s,
            stateFingerprint,
            std::move(registryFingerprints));
    }

    const auto dword = qFromLittleEndian<quint32>(value.rawValue.constData());
    const auto state = tweak.detection->statesByValue.constFind(dword);
    if (state == tweak.detection->statesByValue.cend()) {
        return {
            .status = domain::DetectionStatus::Custom,
            .stateId = u"custom"_s,
            .details = u"DWORD содержит значение, не сопоставленное с именованным состоянием."_s,
            .fingerprint = stateFingerprint,
            .registryFingerprints = std::move(registryFingerprints),
        };
    }
    return {
        .status = domain::DetectionStatus::Named,
        .stateId = *state,
        .details = u"Состояние определено по фактическому DWORD."_s,
        .fingerprint = stateFingerprint,
        .registryFingerprints = std::move(registryFingerprints),
    };
}

} // namespace tweakopedia::detection
