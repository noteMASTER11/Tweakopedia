#pragma once

#include "startup/StartupStage.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace tweakopedia::startup {

struct StartupIpcMessage {
    std::uint32_t magic{};
    std::uint32_t version{};
    std::uint32_t stage{};
    std::uint32_t reserved{};
};

[[nodiscard]] StartupIpcMessage encodeStartupMessage(StartupStage stage) noexcept;
[[nodiscard]] std::optional<StartupStage> decodeStartupMessage(
    const void* bytes,
    std::size_t size) noexcept;

} // namespace tweakopedia::startup
