#include "domain/WindowsComponent.h"

#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace tweakopedia::domain {

bool isValidWindowsComponentTarget(const WindowsComponentTarget& target)
{
    static const QRegularExpression pattern(u"^[A-Za-z0-9][A-Za-z0-9._~+-]{0,255}$"_s);
    return pattern.match(target.name).hasMatch();
}

QString windowsComponentKindName(WindowsComponentKind kind)
{
    return kind == WindowsComponentKind::Feature ? u"feature"_s : u"capability"_s;
}

QString windowsComponentStateName(WindowsComponentState state)
{
    switch (state) {
    case WindowsComponentState::Enabled: return u"enabled"_s;
    case WindowsComponentState::Disabled: return u"disabled"_s;
    case WindowsComponentState::Absent: return u"absent"_s;
    case WindowsComponentState::Unsupported: return u"unsupported"_s;
    }
    return {};
}

std::optional<WindowsComponentKind> parseWindowsComponentKind(QStringView value)
{
    if (value == u"feature") return WindowsComponentKind::Feature;
    if (value == u"capability") return WindowsComponentKind::Capability;
    return std::nullopt;
}

std::optional<WindowsComponentState> parseWindowsComponentState(QStringView value)
{
    if (value == u"enabled") return WindowsComponentState::Enabled;
    if (value == u"disabled") return WindowsComponentState::Disabled;
    if (value == u"absent") return WindowsComponentState::Absent;
    if (value == u"unsupported") return WindowsComponentState::Unsupported;
    return std::nullopt;
}

QJsonObject WindowsComponentSnapshot::toJson() const
{
    return {{u"kind"_s, windowsComponentKindName(target.kind)},
            {u"name"_s, target.name},
            {u"state"_s, windowsComponentStateName(state)},
            {u"type"_s, u"windows_component"_s}};
}

std::optional<WindowsComponentSnapshot> WindowsComponentSnapshot::fromJson(
    const QJsonObject& object)
{
    const auto kind = parseWindowsComponentKind(object.value(u"kind"_s).toString());
    const auto state = parseWindowsComponentState(object.value(u"state"_s).toString());
    WindowsComponentSnapshot snapshot{{kind.value_or(WindowsComponentKind::Feature),
                                       object.value(u"name"_s).toString()},
                                      state.value_or(WindowsComponentState::Unsupported)};
    if (!kind || !state || !isValidWindowsComponentTarget(snapshot.target)
        || snapshot.state == WindowsComponentState::Unsupported) {
        return std::nullopt;
    }
    return snapshot;
}

} // namespace tweakopedia::domain
