#include "startup/StartupIpc.h"

#include <cstring>

namespace tweakopedia::startup {
namespace {

constexpr std::uint32_t messageMagic = 0x50574B54U; // TWKP in little endian.
constexpr std::uint32_t messageVersion = 1U;

}

StartupIpcMessage encodeStartupMessage(StartupStage stage) noexcept
{
    return {
        .magic = messageMagic,
        .version = messageVersion,
        .stage = static_cast<std::uint32_t>(stage),
        .reserved = 0U,
    };
}

std::optional<StartupStage> decodeStartupMessage(const void* bytes, std::size_t size) noexcept
{
    if (!bytes || size != sizeof(StartupIpcMessage)) return std::nullopt;
    StartupIpcMessage message;
    std::memcpy(&message, bytes, sizeof(message));
    if (message.magic != messageMagic || message.version != messageVersion
        || message.reserved != 0U) {
        return std::nullopt;
    }
    const auto stage = static_cast<StartupStage>(message.stage);
    return isKnownStartupStage(stage) ? std::optional(stage) : std::nullopt;
}

} // namespace tweakopedia::startup
