#pragma once

#include <QHash>
#include <QJsonObject>
#include <QString>

#include <optional>

namespace tweakopedia::domain {

enum class WindowsComponentKind { Feature, Capability };
enum class WindowsComponentState { Enabled, Disabled, Absent, Unsupported };

struct WindowsComponentTarget {
    WindowsComponentKind kind{WindowsComponentKind::Feature};
    QString name;

    friend bool operator==(const WindowsComponentTarget&, const WindowsComponentTarget&) = default;
};

struct WindowsComponentDetection {
    WindowsComponentTarget target;
    QHash<WindowsComponentState, QString> states;
};

struct SetWindowsComponentStateOperation {
    WindowsComponentTarget target;
    WindowsComponentState state{WindowsComponentState::Disabled};

    friend bool operator==(const SetWindowsComponentStateOperation&,
                           const SetWindowsComponentStateOperation&) = default;
};

struct WindowsComponentSnapshot {
    WindowsComponentTarget target;
    WindowsComponentState state{WindowsComponentState::Unsupported};

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static std::optional<WindowsComponentSnapshot> fromJson(
        const QJsonObject& object);
};

[[nodiscard]] bool isValidWindowsComponentTarget(const WindowsComponentTarget& target);
[[nodiscard]] QString windowsComponentKindName(WindowsComponentKind kind);
[[nodiscard]] QString windowsComponentStateName(WindowsComponentState state);
[[nodiscard]] std::optional<WindowsComponentKind> parseWindowsComponentKind(QStringView value);
[[nodiscard]] std::optional<WindowsComponentState> parseWindowsComponentState(QStringView value);

} // namespace tweakopedia::domain
