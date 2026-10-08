#pragma once

#include "bootstrap/IBootstrapPlatform.h"

namespace tweakopedia::bootstrap {

class WindowsBootstrapPlatform final : public IBootstrapPlatform
{
public:
    [[nodiscard]] std::filesystem::path localAppDataPath() const override;
    void showError(std::wstring_view title, std::wstring_view message) override;
    package::Result<std::uint32_t> launchAndWait(
        const std::filesystem::path& executable,
        std::span<const std::wstring> arguments,
        const std::filesystem::path& workingDirectory) override;

    [[nodiscard]] static std::wstring quoteArgument(std::wstring_view argument);
};

} // namespace tweakopedia::bootstrap
