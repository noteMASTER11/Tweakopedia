#pragma once

#include "platform/IWindowsComponentBackend.h"

#include <QStringList>

namespace tweakopedia::platform {

class WindowsComponentBackend final : public IWindowsComponentBackend
{
public:
    [[nodiscard]] WindowsComponentQueryResult query(
        const domain::WindowsComponentTarget& target) const override;
    [[nodiscard]] WindowsComponentMutationResult setState(
        const domain::WindowsComponentTarget& target,
        domain::WindowsComponentState state) override;

    [[nodiscard]] static QStringList queryArguments(
        const domain::WindowsComponentTarget& target);
    [[nodiscard]] static QStringList mutationArguments(
        const domain::WindowsComponentTarget& target,
        domain::WindowsComponentState state);
};

} // namespace tweakopedia::platform
