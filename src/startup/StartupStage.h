#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace tweakopedia::startup {

enum class StartupStage : std::uint32_t {
    ContainerIntegrity = 1,
    LocalData = 2,
    RuntimeCache = 3,
    RuntimeExtract = 4,
    AppLaunch = 5,
    DataStore = 6,
    CatalogLoad = 7,
    CatalogValidate = 8,
    WindowsProfile = 9,
    StateDetection = 10,
    Models = 11,
    Interface = 12,
    Ready = 13,
};

[[nodiscard]] bool isKnownStartupStage(StartupStage stage) noexcept;
[[nodiscard]] std::wstring_view startupStageText(StartupStage stage) noexcept;
[[nodiscard]] std::vector<StartupStage> startupPipeline(bool includeRuntimeExtraction);

} // namespace tweakopedia::startup
