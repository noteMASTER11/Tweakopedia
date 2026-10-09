#include "bootstrap/StartupWindow.h"
#include "startup/StartupStage.h"

#include <shellapi.h>
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <thread>

namespace {

std::uint32_t numericArgument(
    int count,
    wchar_t** arguments,
    const wchar_t* name,
    std::uint32_t fallback)
{
    for (int index = 1; index + 1 < count; ++index) {
        if (wcscmp(arguments[index], name) != 0) continue;
        wchar_t* end{};
        const auto value = wcstoul(arguments[index + 1], &end, 10);
        return end && *end == L'\0' ? static_cast<std::uint32_t>(value) : fallback;
    }
    return fallback;
}

}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    int count{};
    auto** arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments) return 2;
    const auto stageValue = numericArgument(
        count, arguments, L"--stage",
        static_cast<std::uint32_t>(tweakopedia::startup::StartupStage::CatalogValidate));
    const auto holdMilliseconds = numericArgument(count, arguments, L"--hold-ms", 8000U);
    LocalFree(arguments);

    const auto stage = static_cast<tweakopedia::startup::StartupStage>(stageValue);
    if (!tweakopedia::startup::isKnownStartupStage(stage)
        || stage == tweakopedia::startup::StartupStage::Ready) {
        return 3;
    }

    (void)SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    tweakopedia::bootstrap::StartupWindow window;
    if (!window.create(instance)) return 4;
    window.show();
    window.report(stage);

    const auto completed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!completed) return 5;
    std::thread timer([completed, holdMilliseconds] {
        std::this_thread::sleep_for(std::chrono::milliseconds(holdMilliseconds));
        SetEvent(completed);
    });
    window.messageLoopUntil(completed);
    timer.join();
    CloseHandle(completed);
    return 0;
}
