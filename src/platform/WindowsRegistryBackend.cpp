#include "platform/WindowsRegistryBackend.h"

#include <windows.h>

namespace tweakopedia::platform {
namespace {

HKEY nativeHive(domain::RegistryHive hive)
{
    switch (hive) {
    case domain::RegistryHive::CurrentUser:
        return HKEY_CURRENT_USER;
    case domain::RegistryHive::LocalMachine:
        return HKEY_LOCAL_MACHINE;
    }
    return nullptr;
}

REGSAM nativeView(domain::RegistryView view)
{
    switch (view) {
    case domain::RegistryView::Registry32:
        return KEY_WOW64_32KEY;
    case domain::RegistryView::Registry64:
        return KEY_WOW64_64KEY;
    case domain::RegistryView::Default:
        return 0;
    }
    return 0;
}

RegistryValueType valueType(DWORD type)
{
    switch (type) {
    case REG_DWORD:
        return RegistryValueType::Dword;
    case REG_SZ:
    case REG_EXPAND_SZ:
        return RegistryValueType::String;
    case REG_BINARY:
        return RegistryValueType::Binary;
    default:
        return RegistryValueType::Unknown;
    }
}

std::error_code win32Error(LSTATUS status)
{
    return {static_cast<int>(status), std::system_category()};
}

bool isMissing(LSTATUS status)
{
    return status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND;
}

} // namespace

RegistryReadResult WindowsRegistryBackend::read(const domain::RegistryLocation& location) const
{
    HKEY key{};
    auto status = RegOpenKeyExW(
        nativeHive(location.hive),
        reinterpret_cast<LPCWSTR>(location.key.utf16()),
        0,
        KEY_QUERY_VALUE | nativeView(location.view),
        &key);
    if (isMissing(status)) {
        return RegistryReadResult::missing();
    }
    if (status != ERROR_SUCCESS) {
        return RegistryReadResult::failed(win32Error(status));
    }

    DWORD type{};
    DWORD byteCount{};
    status = RegQueryValueExW(
        key,
        reinterpret_cast<LPCWSTR>(location.valueName.utf16()),
        nullptr,
        &type,
        nullptr,
        &byteCount);
    if (isMissing(status)) {
        RegCloseKey(key);
        return RegistryReadResult::missing();
    }
    if (status != ERROR_SUCCESS) {
        RegCloseKey(key);
        return RegistryReadResult::failed(win32Error(status));
    }

    QByteArray bytes(static_cast<qsizetype>(byteCount), Qt::Uninitialized);
    status = RegQueryValueExW(
        key,
        reinterpret_cast<LPCWSTR>(location.valueName.utf16()),
        nullptr,
        &type,
        reinterpret_cast<BYTE*>(bytes.data()),
        &byteCount);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) {
        return RegistryReadResult::failed(win32Error(status));
    }
    bytes.resize(static_cast<qsizetype>(byteCount));
    return RegistryReadResult::present(valueType(type), std::move(bytes), type);
}

RegistryWriteResult WindowsRegistryBackend::writeDword(
    const domain::RegistryLocation& location,
    quint32 value)
{
    HKEY key{};
    const auto createStatus = RegCreateKeyExW(
        nativeHive(location.hive),
        reinterpret_cast<LPCWSTR>(location.key.utf16()),
        0,
        nullptr,
        0,
        KEY_SET_VALUE | nativeView(location.view),
        nullptr,
        &key,
        nullptr);
    if (createStatus != ERROR_SUCCESS) {
        return RegistryWriteResult::failed(win32Error(createStatus));
    }

    const auto status = RegSetValueExW(
        key,
        reinterpret_cast<LPCWSTR>(location.valueName.utf16()),
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&value),
        sizeof(value));
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) {
        return RegistryWriteResult::failed(win32Error(status));
    }
    return RegistryWriteResult::succeeded();
}

RegistryWriteResult WindowsRegistryBackend::writeRaw(
    const domain::RegistryLocation& location,
    quint32 nativeType,
    const QByteArray& rawValue)
{
    HKEY key{};
    const auto createStatus = RegCreateKeyExW(
        nativeHive(location.hive),
        reinterpret_cast<LPCWSTR>(location.key.utf16()),
        0,
        nullptr,
        0,
        KEY_SET_VALUE | nativeView(location.view),
        nullptr,
        &key,
        nullptr);
    if (createStatus != ERROR_SUCCESS) {
        return RegistryWriteResult::failed(win32Error(createStatus));
    }

    const auto status = RegSetValueExW(
        key,
        reinterpret_cast<LPCWSTR>(location.valueName.utf16()),
        0,
        nativeType,
        reinterpret_cast<const BYTE*>(rawValue.constData()),
        static_cast<DWORD>(rawValue.size()));
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) {
        return RegistryWriteResult::failed(win32Error(status));
    }
    return RegistryWriteResult::succeeded();
}

RegistryWriteResult WindowsRegistryBackend::deleteValue(const domain::RegistryLocation& location)
{
    HKEY key{};
    const auto openStatus = RegOpenKeyExW(
        nativeHive(location.hive),
        reinterpret_cast<LPCWSTR>(location.key.utf16()),
        0,
        KEY_SET_VALUE | nativeView(location.view),
        &key);
    if (isMissing(openStatus)) {
        return RegistryWriteResult::succeeded();
    }
    if (openStatus != ERROR_SUCCESS) {
        return RegistryWriteResult::failed(win32Error(openStatus));
    }

    const auto status = RegDeleteValueW(key, reinterpret_cast<LPCWSTR>(location.valueName.utf16()));
    RegCloseKey(key);
    if (status != ERROR_SUCCESS && !isMissing(status)) {
        return RegistryWriteResult::failed(win32Error(status));
    }
    return RegistryWriteResult::succeeded();
}

} // namespace tweakopedia::platform
