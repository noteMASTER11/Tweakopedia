#pragma once

#include "bootstrap/IBootstrapPlatform.h"

#include <filesystem>
#include <span>
#include <string>

namespace tweakopedia::bootstrap {

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
        std::span<const std::wstring> arguments);

private:
    int fail(int exitCode, std::string_view message);

    IBootstrapPlatform& platform_;
};

} // namespace tweakopedia::bootstrap
