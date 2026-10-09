#pragma once

#include <QHash>
#include <QJsonObject>
#include <QString>

#include <optional>

namespace tweakopedia::domain {

enum class PowerSource { Ac, Dc };

struct PowerSettingLocation {
    QString scheme;
    QString subgroup;
    QString setting;
    PowerSource source{PowerSource::Ac};
    friend bool operator==(const PowerSettingLocation&, const PowerSettingLocation&) = default;
};

struct PowerSettingDetection {
    PowerSettingLocation location;
    QHash<quint32, QString> statesByIndex;
};

struct SetPowerSettingOperation {
    PowerSettingLocation location;
    quint32 index{};
    friend bool operator==(const SetPowerSettingOperation&, const SetPowerSettingOperation&) = default;
};

struct PowerSettingSnapshot {
    PowerSettingLocation location;
    QString resolvedScheme;
    quint32 index{};
    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static std::optional<PowerSettingSnapshot> fromJson(const QJsonObject& object);
};

[[nodiscard]] bool isValidPowerGuid(QStringView value);
[[nodiscard]] bool isValidPowerLocation(const PowerSettingLocation& location);

} // namespace tweakopedia::domain
