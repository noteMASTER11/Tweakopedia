#include "BootstrapProgressClient.h"

#include <windows.h>

#include <limits>

namespace tweakopedia::app {
namespace {

class WindowsBootstrapWindowChannel final : public IBootstrapWindowChannel
{
public:
    bool exists(std::uintptr_t handle) const override
    {
        return handle != 0U && IsWindow(reinterpret_cast<HWND>(handle));
    }

    bool send(
        std::uintptr_t handle,
        const startup::StartupIpcMessage& message) const override
    {
        if (!exists(handle)) return false;
        COPYDATASTRUCT payload{
            .dwData = 0U,
            .cbData = static_cast<DWORD>(sizeof(message)),
            .lpData = const_cast<startup::StartupIpcMessage*>(&message),
        };
        DWORD_PTR result{};
        const auto sent = SendMessageTimeoutW(
            reinterpret_cast<HWND>(handle),
            WM_COPYDATA,
            0U,
            reinterpret_cast<LPARAM>(&payload),
            SMTO_ABORTIFHUNG | SMTO_BLOCK,
            2000U,
            &result);
        return sent != 0 && result != 0U;
    }

    std::optional<QRect> geometry(std::uintptr_t handle) const override
    {
        if (!exists(handle)) return std::nullopt;
        RECT bounds{};
        const auto window = reinterpret_cast<HWND>(handle);
        if (!GetClientRect(window, &bounds)) return std::nullopt;
        POINT topLeft{bounds.left, bounds.top};
        if (!ClientToScreen(window, &topLeft)) return std::nullopt;
        bounds.right += topLeft.x;
        bounds.bottom += topLeft.y;
        bounds.left = topLeft.x;
        bounds.top = topLeft.y;
        return QRect(bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top);
    }
};

WindowsBootstrapWindowChannel& nativeChannel()
{
    static WindowsBootstrapWindowChannel channel;
    return channel;
}

std::optional<std::uintptr_t> parseHandle(QStringView argument)
{
    if (argument.isEmpty()) return std::nullopt;
    for (const auto character : argument) {
        if (!character.isDigit()) return std::nullopt;
    }
    bool parsed{};
    const auto value = argument.toString().toULongLong(&parsed, 10);
    if (!parsed || value == 0U
        || value > static_cast<qulonglong>(std::numeric_limits<std::uintptr_t>::max())) {
        return std::nullopt;
    }
    return static_cast<std::uintptr_t>(value);
}

}

BootstrapProgressClient::BootstrapProgressClient(
    std::uintptr_t handle,
    IBootstrapWindowChannel& channel)
    : handle_(handle)
    , channel_(&channel)
{
}

std::optional<BootstrapProgressClient> BootstrapProgressClient::fromArgument(
    QStringView argument)
{
    return fromArgument(argument, nativeChannel());
}

std::optional<BootstrapProgressClient> BootstrapProgressClient::fromArgument(
    QStringView argument,
    IBootstrapWindowChannel& channel)
{
    const auto handle = parseHandle(argument);
    if (!handle || !channel.exists(*handle)) return std::nullopt;
    return BootstrapProgressClient(*handle, channel);
}

bool BootstrapProgressClient::report(startup::StartupStage stage) const
{
    if (!channel_ || !startup::isKnownStartupStage(stage)) return false;
    return channel_->send(handle_, startup::encodeStartupMessage(stage));
}

bool BootstrapProgressClient::ready() const
{
    return report(startup::StartupStage::Ready);
}

std::optional<QRect> BootstrapProgressClient::splashGeometry() const
{
    return channel_ ? channel_->geometry(handle_) : std::nullopt;
}

} // namespace tweakopedia::app
