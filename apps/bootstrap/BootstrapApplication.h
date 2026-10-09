#pragma once

#include "bootstrap/IBootstrapPlatform.h"
#include "startup/StartupStage.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>

namespace tweakopedia::bootstrap {

using StartupProgress = std::function<void(startup::StartupStage)>;

class BootstrapApplication final
{
public:
    static constexpr int inspectFailure = 10;
    static constexpr int migrationFailure = 11;
    static constexpr int prepareFailure = 12;
    static constexpr int launchFailure = 13;

    explicit BootstrapApplication(IBootstrapPlatform& platform)
        : platform_(platform)
    {
    }

    int run(
        const std::filesystem::path& self,
        std::span<const std::wstring> arguments,
        StartupProgress progress = {},
        std::optional<std::uintptr_t> startupWindowHandle = std::nullopt);

private:
    int fail(int exitCode, std::string_view message);

    IBootstrapPlatform& platform_;
};

} // namespace tweakopedia::bootstrap
