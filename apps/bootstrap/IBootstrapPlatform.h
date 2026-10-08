#pragma once

#include "package/PackageTypes.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace tweakopedia::bootstrap {

class IBootstrapPlatform
{
public:
    virtual ~IBootstrapPlatform() = default;

    [[nodiscard]] virtual std::filesystem::path localAppDataPath() const = 0;
    virtual void showError(std::wstring_view title, std::wstring_view message) = 0;
    virtual package::Result<std::uint32_t> launchAndWait(
        const std::filesystem::path& executable,
        std::span<const std::wstring> arguments,
        const std::filesystem::path& workingDirectory) = 0;
};

} // namespace tweakopedia::bootstrap
