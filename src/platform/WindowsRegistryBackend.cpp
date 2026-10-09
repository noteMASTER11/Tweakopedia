#include "platform/WindowsRegistryBackend.h"

#include <windows.h>

#include <algorithm>

namespace tweakopedia::platform {
namespace {

HKEY nativeHive(domain::RegistryHive hive)
{
    switch (hive) {
    case domain::RegistryHive::CurrentUser:
        return HKEY_CURRENT_USER;
    case domain::RegistryHive::LocalMachine:
        return HKEY_LOCAL_MACHINE;
    case domain::RegistryHive::ClassesRoot:
        return HKEY_CLASSES_ROOT;
    case domain::RegistryHive::Users:
        return HKEY_USERS;
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
    case REG_QWORD:
        return RegistryValueType::Qword;
    case REG_SZ:
        return RegistryValueType::String;
    case REG_EXPAND_SZ:
        return RegistryValueType::ExpandString;
    case REG_MULTI_SZ:
        return RegistryValueType::MultiString;
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

LPCWSTR nativeValueName(const QString& valueName)
{
    return valueName == u"(default)" ? nullptr
                                      : reinterpret_cast<LPCWSTR>(valueName.utf16());
}

struct TreeReadContext {
    domain::RegistryTreeLimits limits;
    qsizetype nodes{};
    qsizetype bytes{};
    bool limitExceeded{};
    std::error_code error;
};

bool addTreeBytes(TreeReadContext& context, qsizetype bytes)
{
    if (bytes < 0 || context.bytes > context.limits.maxBytes - bytes) {
        context.limitExceeded = true;
        return false;
    }
    context.bytes += bytes;
    return true;
}

bool readTreeNode(
    HKEY key,
    const QString& relativePath,
    TreeReadContext& context,
    QVector<domain::RegistryTreeNode>& nodes)
{
    if (++context.nodes > context.limits.maxNodes
        || !addTreeBytes(context, relativePath.size() * qsizetype{2})) {
        context.limitExceeded = true;
        return false;
    }

    DWORD valueCount{};
    DWORD maxValueNameLength{};
    DWORD maxValueDataLength{};
    DWORD subkeyCount{};
    DWORD maxSubkeyLength{};
    const auto infoStatus = RegQueryInfoKeyW(
        key, nullptr, nullptr, nullptr,
        &subkeyCount, &maxSubkeyLength, nullptr,
        &valueCount, &maxValueNameLength, &maxValueDataLength,
        nullptr, nullptr);
    if (infoStatus != ERROR_SUCCESS) {
        context.error = win32Error(infoStatus);
        return false;
    }

    domain::RegistryTreeNode node{.relativePath = relativePath};
    QVector<wchar_t> valueName(static_cast<qsizetype>(maxValueNameLength) + 2);
    QByteArray data(static_cast<qsizetype>(maxValueDataLength), Qt::Uninitialized);
    for (DWORD index = 0; index < valueCount; ++index) {
        DWORD nameLength = static_cast<DWORD>(valueName.size() - 1);
        DWORD dataLength = static_cast<DWORD>(data.size());
        DWORD type{};
        auto status = RegEnumValueW(
            key, index, valueName.data(), &nameLength, nullptr, &type,
            reinterpret_cast<BYTE*>(data.data()), &dataLength);
        if (status == ERROR_MORE_DATA) {
            data.resize(static_cast<qsizetype>(dataLength));
            nameLength = static_cast<DWORD>(valueName.size() - 1);
            status = RegEnumValueW(
                key, index, valueName.data(), &nameLength, nullptr, &type,
                reinterpret_cast<BYTE*>(data.data()), &dataLength);
        }
        if (status != ERROR_SUCCESS) {
            context.error = win32Error(status);
            return false;
        }
        const auto name = QString::fromWCharArray(
            valueName.data(), static_cast<qsizetype>(nameLength));
        if (!addTreeBytes(context, name.size() * qsizetype{2} + dataLength)) return false;
        node.values.append({
            .name = name,
            .value = {
                .nativeType = type,
                .rawValue = QByteArray(data.constData(), static_cast<qsizetype>(dataLength)),
            },
        });
    }
    std::sort(node.values.begin(), node.values.end(), [](const auto& left, const auto& right) {
        return left.name < right.name;
    });
    nodes.append(std::move(node));

    QVector<wchar_t> subkeyName(static_cast<qsizetype>(maxSubkeyLength) + 2);
    for (DWORD index = 0; index < subkeyCount; ++index) {
        DWORD nameLength = static_cast<DWORD>(subkeyName.size() - 1);
        const auto enumStatus = RegEnumKeyExW(
            key, index, subkeyName.data(), &nameLength, nullptr, nullptr, nullptr, nullptr);
        if (enumStatus != ERROR_SUCCESS) {
            context.error = win32Error(enumStatus);
            return false;
        }
        const auto name = QString::fromWCharArray(
            subkeyName.data(), static_cast<qsizetype>(nameLength));
        HKEY child{};
        const auto openStatus = RegOpenKeyExW(
            key, subkeyName.data(), REG_OPTION_OPEN_LINK, KEY_READ, &child);
        if (openStatus != ERROR_SUCCESS) {
            context.error = win32Error(openStatus);
            return false;
        }
        const auto childPath = relativePath.isEmpty() ? name : relativePath + u'\\' + name;
        const auto success = readTreeNode(child, childPath, context, nodes);
        RegCloseKey(child);
        if (!success) return false;
    }
    return true;
}

QString parentPath(const QString& key)
{
    const auto separator = key.lastIndexOf(u'\\');
    return separator < 0 ? QString{} : key.left(separator);
}

QString leafName(const QString& key)
{
    const auto separator = key.lastIndexOf(u'\\');
    return separator < 0 ? key : key.mid(separator + 1);
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
        nativeValueName(location.valueName),
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
        nativeValueName(location.valueName),
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
    return writeValue(location, domain::RegistryValueSpec::dword(value));
}

RegistryWriteResult WindowsRegistryBackend::writeValue(
    const domain::RegistryLocation& location,
    const domain::RegistryValueSpec& value)
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
        nativeValueName(location.valueName),
        0,
        value.nativeType,
        reinterpret_cast<const BYTE*>(value.rawValue.constData()),
        static_cast<DWORD>(value.rawValue.size()));
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
    return writeValue(location, domain::RegistryValueSpec{
        .nativeType = nativeType,
        .rawValue = rawValue,
    });
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

    const auto status = RegDeleteValueW(key, nativeValueName(location.valueName));
    RegCloseKey(key);
    if (status != ERROR_SUCCESS && !isMissing(status)) {
        return RegistryWriteResult::failed(win32Error(status));
    }
    return RegistryWriteResult::succeeded();
}

RegistryTreeReadResult WindowsRegistryBackend::readTree(
    const domain::RegistryKeyLocation& location,
    const domain::RegistryTreeLimits& limits) const
{
    if (location.key.isEmpty() || limits.maxNodes <= 0 || limits.maxBytes < 0) {
        return RegistryTreeReadResult::failed(QStringLiteral("registry.tree_invalid"));
    }
    HKEY key{};
    const auto status = RegOpenKeyExW(
        nativeHive(location.hive),
        reinterpret_cast<LPCWSTR>(location.key.utf16()),
        REG_OPTION_OPEN_LINK,
        KEY_READ | nativeView(location.view),
        &key);
    if (isMissing(status)) {
        return RegistryTreeReadResult::succeeded({.location = location, .existed = false});
    }
    if (status != ERROR_SUCCESS) {
        return RegistryTreeReadResult::failed(QStringLiteral("registry.tree_read_failed"),
                                              win32Error(status));
    }

    TreeReadContext context{.limits = limits};
    domain::RegistryTreeSnapshot snapshot{.location = location, .existed = true};
    const auto success = readTreeNode(key, {}, context, snapshot.nodes);
    RegCloseKey(key);
    if (!success) {
        return RegistryTreeReadResult::failed(
            context.limitExceeded ? QStringLiteral("registry.tree_limit")
                                  : QStringLiteral("registry.tree_read_failed"),
            context.error);
    }
    std::sort(snapshot.nodes.begin(), snapshot.nodes.end(), [](const auto& left, const auto& right) {
        return left.relativePath < right.relativePath;
    });
    return RegistryTreeReadResult::succeeded(std::move(snapshot));
}

RegistryWriteResult WindowsRegistryBackend::createKey(
    const domain::RegistryKeyLocation& location)
{
    if (location.key.isEmpty()) {
        return RegistryWriteResult::failed(std::make_error_code(std::errc::invalid_argument));
    }
    HKEY key{};
    const auto status = RegCreateKeyExW(
        nativeHive(location.hive),
        reinterpret_cast<LPCWSTR>(location.key.utf16()),
        0, nullptr, 0,
        KEY_SET_VALUE | nativeView(location.view),
        nullptr, &key, nullptr);
    if (key) RegCloseKey(key);
    return status == ERROR_SUCCESS
        ? RegistryWriteResult::succeeded()
        : RegistryWriteResult::failed(win32Error(status));
}

RegistryWriteResult WindowsRegistryBackend::deleteTree(
    const domain::RegistryKeyLocation& location)
{
    if (location.key.isEmpty()) {
        return RegistryWriteResult::failed(std::make_error_code(std::errc::invalid_argument));
    }
    const auto parent = parentPath(location.key);
    const auto leaf = leafName(location.key);
    HKEY parentKey{};
    const auto openStatus = RegOpenKeyExW(
        nativeHive(location.hive),
        parent.isEmpty() ? nullptr : reinterpret_cast<LPCWSTR>(parent.utf16()),
        0,
        DELETE | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE | nativeView(location.view),
        &parentKey);
    if (isMissing(openStatus)) return RegistryWriteResult::succeeded();
    if (openStatus != ERROR_SUCCESS) return RegistryWriteResult::failed(win32Error(openStatus));
    const auto status = RegDeleteTreeW(parentKey, reinterpret_cast<LPCWSTR>(leaf.utf16()));
    RegCloseKey(parentKey);
    return status == ERROR_SUCCESS || isMissing(status)
        ? RegistryWriteResult::succeeded()
        : RegistryWriteResult::failed(win32Error(status));
}

RegistryWriteResult WindowsRegistryBackend::restoreTree(
    const domain::RegistryTreeSnapshot& snapshot)
{
    const auto removed = deleteTree(snapshot.location);
    if (!removed.success || !snapshot.existed) return removed;

    auto nodes = snapshot.nodes;
    std::sort(nodes.begin(), nodes.end(), [](const auto& left, const auto& right) {
        const auto leftDepth = left.relativePath.count(u'\\');
        const auto rightDepth = right.relativePath.count(u'\\');
        return leftDepth == rightDepth
            ? left.relativePath < right.relativePath : leftDepth < rightDepth;
    });
    for (const auto& node : nodes) {
        const auto fullPath = node.relativePath.isEmpty()
            ? snapshot.location.key
            : snapshot.location.key + u'\\' + node.relativePath;
        HKEY key{};
        const auto createStatus = RegCreateKeyExW(
            nativeHive(snapshot.location.hive),
            reinterpret_cast<LPCWSTR>(fullPath.utf16()),
            0, nullptr, 0,
            KEY_SET_VALUE | nativeView(snapshot.location.view),
            nullptr, &key, nullptr);
        if (createStatus != ERROR_SUCCESS) {
            return RegistryWriteResult::failed(win32Error(createStatus));
        }
        for (const auto& value : node.values) {
            const auto setStatus = RegSetValueExW(
                key,
                reinterpret_cast<LPCWSTR>(value.name.utf16()),
                0,
                value.value.nativeType,
                reinterpret_cast<const BYTE*>(value.value.rawValue.constData()),
                static_cast<DWORD>(value.value.rawValue.size()));
            if (setStatus != ERROR_SUCCESS) {
                RegCloseKey(key);
                return RegistryWriteResult::failed(win32Error(setStatus));
            }
        }
        RegCloseKey(key);
    }
    if (nodes.isEmpty()) return createKey(snapshot.location);
    return RegistryWriteResult::succeeded();
}

} // namespace tweakopedia::platform
