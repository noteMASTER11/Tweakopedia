#include "bootstrap/BootstrapApplication.h"
#include "bootstrap/WindowsBootstrapPlatform.h"

#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <string>
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

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
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
    return application.run(executablePath(), arguments);
}
