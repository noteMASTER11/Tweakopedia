#include "platform/WindowsSystemProfileProvider.h"

#include <windows.h>
#include <winternl.h>

#include <QString>
#include <QVector>

namespace tweakopedia::platform {
namespace {

constexpr auto currentVersionKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

quint32 readDword(const wchar_t* valueName)
{
    DWORD value{};
    DWORD size = sizeof(value);
    const auto status = RegGetValueW(
        HKEY_LOCAL_MACHINE,
        currentVersionKey,
        valueName,
        RRF_RT_REG_DWORD | RRF_SUBKEY_WOW6464KEY,
        nullptr,
        &value,
        &size);
    return status == ERROR_SUCCESS ? value : 0;
}

QString readString(const wchar_t* valueName)
{
    DWORD bytes{};
    auto status = RegGetValueW(
        HKEY_LOCAL_MACHINE,
        currentVersionKey,
        valueName,
        RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY,
        nullptr,
        nullptr,
        &bytes);
    if (status != ERROR_SUCCESS || bytes < sizeof(wchar_t)) {
        return {};
    }

    QVector<wchar_t> buffer(static_cast<qsizetype>(bytes / sizeof(wchar_t)));
    status = RegGetValueW(
        HKEY_LOCAL_MACHINE,
        currentVersionKey,
        valueName,
        RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY,
        nullptr,
        buffer.data(),
        &bytes);
    if (status != ERROR_SUCCESS) {
        return {};
    }
    return QString::fromWCharArray(buffer.constData()).trimmed();
}

quint32 registryBuildNumber()
{
    bool ok = false;
    const auto value = readString(L"CurrentBuildNumber").toUInt(&ok);
    return ok ? value : 0;
}

quint32 rtlBuildNumber()
{
    const auto module = GetModuleHandleW(L"ntdll.dll");
    if (!module) {
        return 0;
    }
    const auto function = reinterpret_cast<NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW)>(
        GetProcAddress(module, "RtlGetVersion"));
    if (!function) {
        return 0;
    }

    RTL_OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    if (function(&version) != 0) {
        return 0;
    }
    return version.dwBuildNumber;
}

domain::CpuArchitecture nativeArchitecture()
{
    SYSTEM_INFO info{};
    GetNativeSystemInfo(&info);
    switch (info.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:
        return domain::CpuArchitecture::X64;
    case PROCESSOR_ARCHITECTURE_ARM64:
        return domain::CpuArchitecture::Arm64;
    case PROCESSOR_ARCHITECTURE_INTEL:
        return domain::CpuArchitecture::X86;
    default:
        return domain::CpuArchitecture::Unknown;
    }
}

domain::WindowsFamily familyForBuild(quint32 build)
{
    if (build >= 22000) {
        return domain::WindowsFamily::Windows11;
    }
    if (build >= 10240) {
        return domain::WindowsFamily::Windows10;
    }
    return domain::WindowsFamily::Unknown;
}

} // namespace

domain::SystemProfile WindowsSystemProfileProvider::current() const
{
    const auto nativeBuild = rtlBuildNumber();
    const auto registryBuild = registryBuildNumber();
    const auto build = nativeBuild != 0 ? nativeBuild : registryBuild;

    return domain::SystemProfile{
        .family = familyForBuild(build),
        .build = build,
        .ubr = readDword(L"UBR"),
        .edition = readString(L"EditionID"),
        .architecture = nativeArchitecture(),
    };
}

} // namespace tweakopedia::platform
