#pragma once

#include <QJsonObject>
#include <QString>

#include <optional>

namespace tweakopedia::domain {

struct ScheduledTaskLocation {
    QString folder;
    QString name;

    friend bool operator==(const ScheduledTaskLocation&, const ScheduledTaskLocation&) = default;
};

struct ScheduledTaskDetection {
    ScheduledTaskLocation location;
    QString enabledState;
    QString disabledState;
};

struct SetScheduledTaskEnabledOperation {
    ScheduledTaskLocation location;
    bool enabled{};

    friend bool operator==(const SetScheduledTaskEnabledOperation&,
                           const SetScheduledTaskEnabledOperation&) = default;
};

struct ScheduledTaskSnapshot {
    ScheduledTaskLocation location;
    bool enabled{};

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static std::optional<ScheduledTaskSnapshot> fromJson(const QJsonObject& object);
};

} // namespace tweakopedia::domain
