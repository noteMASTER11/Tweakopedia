#include "bootstrap/BootstrapApplication.h"
#include "bootstrap/StartupWindow.h"
#include "bootstrap/WindowsBootstrapPlatform.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <filesystem>
#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace {

std::filesystem::path executablePath()
{
    std::vector<wchar_t> buffer(512);
    for (;;) {
        const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) return {};
        if (length < buffer.size() - 1) return std::filesystem::path(buffer.data(), buffer.data() + length);
        buffer.resize(buffer.size() * 2);
    }
}

}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    int count{};
    auto rawArguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!rawArguments || count < 1) return 9;
    std::vector<std::wstring> arguments;
    arguments.reserve(static_cast<std::size_t>(count - 1));
    for (int index = 1; index < count; ++index) arguments.emplace_back(rawArguments[index]);
    LocalFree(rawArguments);

    tweakopedia::bootstrap::WindowsBootstrapPlatform platform;
    tweakopedia::bootstrap::BootstrapApplication application(platform);
    const auto selfCheck = std::find(arguments.begin(), arguments.end(), L"--self-check")
        != arguments.end();
    if (selfCheck) return application.run(executablePath(), arguments);

    (void)SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    tweakopedia::bootstrap::StartupWindow startupWindow;
    if (!startupWindow.create(instance)) return application.run(executablePath(), arguments);
    startupWindow.show();

    const auto completed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!completed) return application.run(executablePath(), arguments);
    std::atomic<int> exitCode{9};
    std::thread worker([&] {
        exitCode = application.run(
            executablePath(), arguments,
            [&](auto stage) { startupWindow.report(stage); },
            startupWindow.nativeHandle());
        SetEvent(completed);
    });
    startupWindow.messageLoopUntil(completed);
    worker.join();
    CloseHandle(completed);
    return exitCode.load();
}
