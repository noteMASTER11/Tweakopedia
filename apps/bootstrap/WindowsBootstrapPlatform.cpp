#include "bootstrap/WindowsBootstrapPlatform.h"

#include <windows.h>
#include <shlobj.h>

#include <memory>
#include <vector>

namespace tweakopedia::bootstrap {
namespace {

std::string windowsError(const char* prefix)
{
    return std::string(prefix) + " (Windows error " + std::to_string(GetLastError()) + ")";
}

}

std::filesystem::path WindowsBootstrapPlatform::localAppDataPath() const
{
    PWSTR rawPath{};
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &rawPath))) {
        return {};
    }
    const std::filesystem::path result(rawPath);
    CoTaskMemFree(rawPath);
    return result;
}

void WindowsBootstrapPlatform::showError(std::wstring_view title, std::wstring_view message)
{
    using TaskDialogFunction = HRESULT(WINAPI*)(
        HWND, HINSTANCE, PCWSTR, PCWSTR, PCWSTR, TASKDIALOG_COMMON_BUTTON_FLAGS,
        PCWSTR, int*);
    const auto library = LoadLibraryW(L"comctl32.dll");
    const auto taskDialog = library
        ? reinterpret_cast<TaskDialogFunction>(GetProcAddress(library, "TaskDialog"))
        : nullptr;
    if (taskDialog) {
        taskDialog(nullptr, nullptr, std::wstring(title).c_str(), L"Ошибка запуска",
            std::wstring(message).c_str(), TDCBF_OK_BUTTON, TD_ERROR_ICON, nullptr);
    } else {
        MessageBoxW(nullptr, std::wstring(message).c_str(), std::wstring(title).c_str(),
            MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    }
    if (library) FreeLibrary(library);
}

std::wstring WindowsBootstrapPlatform::quoteArgument(std::wstring_view argument)
{
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring_view::npos) {
        return std::wstring(argument);
    }

    std::wstring quoted(1, L'"');
    std::size_t backslashes{};
    for (const auto character : argument) {
        if (character == L'\\') {
            ++backslashes;
            continue;
        }
        if (character == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(character);
            backslashes = 0;
            continue;
        }
        quoted.append(backslashes, L'\\');
        backslashes = 0;
        quoted.push_back(character);
    }
    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}

package::Result<std::uint32_t> WindowsBootstrapPlatform::launchAndWait(
    const std::filesystem::path& executable,
    std::span<const std::wstring> arguments,
    const std::filesystem::path& workingDirectory)
{
    std::wstring commandLine = quoteArgument(executable.wstring());
    for (const auto& argument : arguments) {
        commandLine.push_back(L' ');
        commandLine += quoteArgument(argument);
    }
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startup{.cb = sizeof(STARTUPINFOW)};
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), mutableCommand.data(), nullptr, nullptr, FALSE, 0,
            nullptr, workingDirectory.c_str(), &startup, &process)) {
        return package::Result<std::uint32_t>::failure(
            package::ErrorCode::Io, windowsError("Cannot launch Tweakopedia"));
    }
    CloseHandle(process.hThread);
    const auto waitResult = WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode{};
    const auto gotExitCode = waitResult == WAIT_OBJECT_0
        && GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hProcess);
    if (!gotExitCode) {
        return package::Result<std::uint32_t>::failure(
            package::ErrorCode::Io, windowsError("Cannot wait for Tweakopedia"));
    }
    return package::Result<std::uint32_t>::success(exitCode);
}

} // namespace tweakopedia::bootstrap
