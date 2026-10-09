#pragma once

#include "domain/WindowsComponent.h"

#include <QString>

namespace tweakopedia::platform {

struct WindowsComponentQueryResult {
    bool success{};
    bool isUnsupported{};
    domain::WindowsComponentState state{domain::WindowsComponentState::Unsupported};
    QString error;

    static WindowsComponentQueryResult present(domain::WindowsComponentState value)
    {
        return {.success = true, .state = value};
    }
    static WindowsComponentQueryResult unsupported(QString error)
    {
        return {.isUnsupported = true, .error = std::move(error)};
    }
};

struct WindowsComponentMutationResult {
    bool success{};
    bool restartRequired{};
    QString error;
};

class IWindowsComponentBackend
{
public:
    virtual ~IWindowsComponentBackend() = default;
    [[nodiscard]] virtual WindowsComponentQueryResult query(
        const domain::WindowsComponentTarget& target) const = 0;
    [[nodiscard]] virtual WindowsComponentMutationResult setState(
        const domain::WindowsComponentTarget& target,
        domain::WindowsComponentState state) = 0;
};

} // namespace tweakopedia::platform
