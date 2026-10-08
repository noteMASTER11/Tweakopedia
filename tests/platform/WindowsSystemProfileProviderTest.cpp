#include "platform/WindowsSystemProfileProvider.h"

#include <QtTest/QTest>

#include <windows.h>
#include <winternl.h>

using namespace tweakopedia::domain;
using namespace tweakopedia::platform;
using namespace Qt::StringLiterals;

namespace {

quint32 readRegistryDword(const wchar_t* valueName)
{
    DWORD value{};
    DWORD size = sizeof(value);
    const auto status = RegGetValueW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        valueName,
        RRF_RT_REG_DWORD | RRF_SUBKEY_WOW6464KEY,
        nullptr,
        &value,
        &size);
    if (status != ERROR_SUCCESS) {
        return 0;
    }
    return value;
}

quint32 rtlBuildNumber()
{
    const auto module = GetModuleHandleW(L"ntdll.dll");
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

} // namespace

class WindowsSystemProfileProviderTest final : public QObject
{
    Q_OBJECT

private slots:
    void readsNativeWindowsProfile()
    {
        const auto expectedBuild = rtlBuildNumber();
        QVERIFY2(expectedBuild > 0, "RtlGetVersion недоступен");

        const auto expectedUbr = readRegistryDword(L"UBR");
        QVERIFY2(expectedUbr > 0, "UBR отсутствует в реестре");

        const auto actual = WindowsSystemProfileProvider{}.current();

        QCOMPARE(actual.build, expectedBuild);
        QCOMPARE(actual.ubr, expectedUbr);
        QCOMPARE(actual.architecture, CpuArchitecture::X64);
        QVERIFY(actual.family == WindowsFamily::Windows10 || actual.family == WindowsFamily::Windows11);
        QVERIFY(!actual.edition.isEmpty());
    }

    void classifiesFamilyFromUnvirtualizedBuild()
    {
        const auto actual = WindowsSystemProfileProvider{}.current();

        if (actual.build >= 22000) {
            QCOMPARE(actual.family, WindowsFamily::Windows11);
        } else {
            QCOMPARE(actual.family, WindowsFamily::Windows10);
        }
    }
};

QTEST_APPLESS_MAIN(WindowsSystemProfileProviderTest)

#include "WindowsSystemProfileProviderTest.moc"
