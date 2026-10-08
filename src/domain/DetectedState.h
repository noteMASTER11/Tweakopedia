#pragma once

#include <QByteArray>
#include <QString>

namespace tweakopedia::domain {

enum class DetectionStatus {
    Named,
    Custom,
    Mixed,
    Unsupported,
    Unknown,
};

struct DetectedState {
    DetectionStatus status{DetectionStatus::Unknown};
    QString stateId;
    QString details;
    QByteArray fingerprint;
};

} // namespace tweakopedia::domain
