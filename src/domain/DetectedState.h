#pragma once

#include "domain/RegistryTypes.h"

#include <QByteArray>
#include <QString>
#include <QVector>

namespace tweakopedia::domain {

enum class DetectionStatus {
    Named,
    Custom,
    Mixed,
    Unsupported,
    Unknown,
};

struct DetectedRegistryFingerprint {
    RegistryLocation location;
    QByteArray fingerprint;
};

struct DetectedState {
    DetectionStatus status{DetectionStatus::Unknown};
    QString stateId;
    QString details;
    QByteArray fingerprint;
    QVector<DetectedRegistryFingerprint> registryFingerprints;
};

} // namespace tweakopedia::domain
