#include "detection/AppxPackageStateDetector.h"

#include "detection/CompatibilityEvaluator.h"

#include <QCryptographicHash>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace tweakopedia::detection {

domain::DetectedState AppxPackageStateDetector::detect(
    const domain::TweakDefinition& tweak,
    const QVector<platform::AppxPackageIdentity>& packages,
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
    if (!tweak.appxDetection) {
        return {
            .status = domain::DetectionStatus::Unknown,
            .stateId = u"unknown"_s,
            .details = u"В определении отсутствует имя AppX-пакета."_s,
        };
    }

    QVector<QString> fullNames;
    for (const auto& package : packages) {
        if (package.name.compare(tweak.appxDetection->packageName, Qt::CaseInsensitive) == 0) {
            fullNames.append(package.fullName);
        }
    }
    std::sort(fullNames.begin(), fullNames.end(), [](const QString& left, const QString& right) {
        return left.compare(right, Qt::CaseInsensitive) < 0;
    });
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const auto& fullName : fullNames) {
        hash.addData(fullName.toUtf8());
        hash.addData("\n");
    }
    const auto fingerprint = hash.result().toHex();
    if (fullNames.isEmpty()) {
        return {
            .status = domain::DetectionStatus::Named,
            .stateId = u"removed"_s,
            .details = u"Пакет не установлен для текущей учётной записи."_s,
            .fingerprint = fingerprint,
        };
    }
    return {
        .status = domain::DetectionStatus::Named,
        .stateId = u"installed"_s,
        .details = u"Установлен пакет: "_s + fullNames.join(u", "_s),
        .fingerprint = fingerprint,
    };
}

} // namespace tweakopedia::detection
