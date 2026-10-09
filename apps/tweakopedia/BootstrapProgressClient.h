#pragma once

#include "startup/StartupIpc.h"

#include <QRect>
#include <QStringView>

#include <cstdint>
#include <optional>

namespace tweakopedia::app {

class IBootstrapWindowChannel
{
public:
    virtual ~IBootstrapWindowChannel() = default;
    [[nodiscard]] virtual bool exists(std::uintptr_t handle) const = 0;
    [[nodiscard]] virtual bool send(
        std::uintptr_t handle,
        const startup::StartupIpcMessage& message) const = 0;
    [[nodiscard]] virtual std::optional<QRect> geometry(std::uintptr_t handle) const = 0;
};

class BootstrapProgressClient final
{
public:
    [[nodiscard]] static std::optional<BootstrapProgressClient> fromArgument(
        QStringView argument);
    [[nodiscard]] static std::optional<BootstrapProgressClient> fromArgument(
        QStringView argument,
        IBootstrapWindowChannel& channel);

    [[nodiscard]] bool report(startup::StartupStage stage) const;
    [[nodiscard]] bool ready() const;
    [[nodiscard]] std::optional<QRect> splashGeometry() const;

private:
    BootstrapProgressClient(std::uintptr_t handle, IBootstrapWindowChannel& channel);

    std::uintptr_t handle_{};
    IBootstrapWindowChannel* channel_{};
};

} // namespace tweakopedia::app
