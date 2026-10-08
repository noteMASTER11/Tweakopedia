#include "bootstrap/BootstrapApplication.h"

#include "package/LegacyDataMigrator.h"
#include "package/PackageFooter.h"
#include "package/RuntimeCache.h"
#include "package/Sha256.h"

#include <windows.h>

#include <vector>

namespace tweakopedia::bootstrap {
namespace {

std::wstring widen(std::string_view text)
{
    if (text.empty()) return {};
    const auto size = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) return L"Неизвестная ошибка";
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
        result.data(), size);
    return result;
}

}

int BootstrapApplication::fail(int exitCode, std::string_view message)
{
    platform_.showError(L"Tweakopedia", widen(message));
    return exitCode;
}

int BootstrapApplication::run(
    const std::filesystem::path& self,
    std::span<const std::wstring> arguments)
{
    const auto packageInfo = package::inspectContainer(self);
    if (!packageInfo.ok()) return fail(inspectFailure, packageInfo.error.message);

    const auto productRoot = platform_.localAppDataPath() / "Tweakopedia";
    const auto dataRoot = productRoot / "Data";
    const auto migration = package::LegacyDataMigrator::migrate(productRoot, dataRoot);
    if (!migration.complete) {
        return fail(migrationFailure, "Cannot migrate the existing Tweakopedia data layout");
    }

    package::RuntimeCache cache(productRoot / "Runtime");
    auto prepared = cache.prepare(self, *packageInfo.value);
    if (!prepared.ok()) return fail(prepareFailure, prepared.error.message);

    std::vector<std::wstring> childArguments(arguments.begin(), arguments.end());
    childArguments.emplace_back(L"--runtime-root");
    childArguments.push_back(prepared.value->root.wstring());
    childArguments.emplace_back(L"--container-path");
    childArguments.push_back(self.wstring());

    const auto result = platform_.launchAndWait(
        prepared.value->root / "Tweakopedia.App.exe",
        childArguments,
        prepared.value->root);
    if (!result.ok()) return fail(launchFailure, result.error.message);

    cache.cleanupOld(package::digestToHex(packageInfo.value->footer.payloadDigest));
    return static_cast<int>(*result.value);
}

} // namespace tweakopedia::bootstrap
