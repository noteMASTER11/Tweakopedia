#include "platform/WindowsPowerSettingBackend.h"

#include <windows.h>
#include <powrprof.h>

using namespace Qt::StringLiterals;

namespace tweakopedia::platform {
namespace {

QString errorText(DWORD error) { return u"PowrProf error %1"_s.arg(error); }

std::optional<GUID> parseGuid(QStringView value)
{
    GUID result{};
    const auto withBraces = value.startsWith(u'{') ? value.toString()
        : u"{"_s + value.toString() + u"}"_s;
    return SUCCEEDED(IIDFromString(
        reinterpret_cast<LPCOLESTR>(withBraces.utf16()), &result))
        ? std::optional<GUID>{result} : std::nullopt;
}

QString guidText(const GUID& value)
{
    wchar_t buffer[39]{};
    StringFromGUID2(value, buffer, 39);
    auto result = QString::fromWCharArray(buffer).toLower();
    if (result.startsWith(u'{') && result.endsWith(u'}')) result = result.mid(1, 36);
    return result;
}

struct ResolvedLocation {
    GUID scheme;
    GUID subgroup;
    GUID setting;
    QString schemeText;
};

std::optional<ResolvedLocation> resolve(
    const domain::PowerSettingLocation& location, QString* error)
{
    if (!domain::isValidPowerLocation(location)) {
        *error = u"Некорректный GUID параметра питания."_s;
        return std::nullopt;
    }
    GUID scheme{};
    if (location.scheme == u"active") {
        GUID* active{};
        const auto status = PowerGetActiveScheme(nullptr, &active);
        if (status != ERROR_SUCCESS || !active) {
            *error = errorText(status);
            return std::nullopt;
        }
        scheme = *active;
        LocalFree(active);
    } else {
        scheme = *parseGuid(location.scheme);
    }
    return ResolvedLocation{
        .scheme = scheme,
        .subgroup = *parseGuid(location.subgroup),
        .setting = *parseGuid(location.setting),
        .schemeText = guidText(scheme),
    };
}

} // namespace

PowerSettingReadResult WindowsPowerSettingBackend::read(
    const domain::PowerSettingLocation& location) const
{
    QString error;
    const auto resolved = resolve(location, &error);
    if (!resolved) return PowerSettingReadResult::failed(error);
    DWORD index{};
    const auto status = location.source == domain::PowerSource::Ac
        ? PowerReadACValueIndex(nullptr, &resolved->scheme, &resolved->subgroup,
                                &resolved->setting, &index)
        : PowerReadDCValueIndex(nullptr, &resolved->scheme, &resolved->subgroup,
                                &resolved->setting, &index);
    if (status == ERROR_FILE_NOT_FOUND) return {};
    if (status != ERROR_SUCCESS) return PowerSettingReadResult::failed(errorText(status));
    return PowerSettingReadResult::present(index, resolved->schemeText);
}

PowerSettingMutationResult WindowsPowerSettingBackend::write(
    const domain::PowerSettingLocation& location, quint32 index)
{
    QString error;
    const auto resolved = resolve(location, &error);
    if (!resolved) return {.error = error};
    const auto status = location.source == domain::PowerSource::Ac
        ? PowerWriteACValueIndex(nullptr, &resolved->scheme, &resolved->subgroup,
                                 &resolved->setting, index)
        : PowerWriteDCValueIndex(nullptr, &resolved->scheme, &resolved->subgroup,
                                 &resolved->setting, index);
    return status == ERROR_SUCCESS ? PowerSettingMutationResult{.success = true}
                                   : PowerSettingMutationResult{.error = errorText(status)};
}

PowerSettingMutationResult WindowsPowerSettingBackend::activateScheme(QStringView scheme)
{
    const auto parsed = parseGuid(scheme);
    if (!parsed) return {.error = u"Некорректный GUID схемы питания."_s};
    const auto status = PowerSetActiveScheme(nullptr, &*parsed);
    return status == ERROR_SUCCESS ? PowerSettingMutationResult{.success = true}
                                   : PowerSettingMutationResult{.error = errorText(status)};
}

PowerSchemeResult WindowsPowerSettingBackend::duplicateActiveScheme()
{
    GUID* active{};
    auto status = PowerGetActiveScheme(nullptr, &active);
    if (status != ERROR_SUCCESS || !active) return {.error = errorText(status)};
    GUID* duplicate{};
    status = PowerDuplicateScheme(nullptr, active, &duplicate);
    LocalFree(active);
    if (status != ERROR_SUCCESS || !duplicate) return {.error = errorText(status)};
    const auto value = guidText(*duplicate);
    LocalFree(duplicate);
    return {.scheme = value};
}

PowerSettingMutationResult WindowsPowerSettingBackend::deleteScheme(QStringView scheme)
{
    const auto parsed = parseGuid(scheme);
    if (!parsed) return {.error = u"Некорректный GUID схемы питания."_s};
    const auto status = PowerDeleteScheme(nullptr, &*parsed);
    return status == ERROR_SUCCESS ? PowerSettingMutationResult{.success = true}
                                   : PowerSettingMutationResult{.error = errorText(status)};
}

} // namespace tweakopedia::platform
