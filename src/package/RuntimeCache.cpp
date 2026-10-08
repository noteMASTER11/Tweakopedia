#include "package/RuntimeCache.h"

#include "package/PayloadManifest.h"
#include "package/Sha256.h"
#include "package/ZipPayload.h"

#include <windows.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <fstream>
#include <iterator>
#include <set>

namespace tweakopedia::package {
namespace {

using Json = nlohmann::json;
std::atomic_uint64_t uniqueSuffix{};

class NamedMutex final
{
public:
    explicit NamedMutex(std::wstring name)
    {
        handle_ = CreateMutexW(nullptr, FALSE, name.c_str());
        if (handle_ && WaitForSingleObject(handle_, INFINITE) != WAIT_OBJECT_0) {
            CloseHandle(handle_);
            handle_ = nullptr;
        }
    }
    ~NamedMutex()
    {
        if (!handle_) return;
        ReleaseMutex(handle_);
        CloseHandle(handle_);
    }
    [[nodiscard]] bool valid() const { return handle_ != nullptr; }

private:
    HANDLE handle_{};
};

Result<PreparedRuntime> cacheFailure(std::string message)
{
    return Result<PreparedRuntime>::failure(ErrorCode::CacheFailure, std::move(message));
}

bool isDigestName(std::wstring_view name)
{
    return name.size() == 64 && std::all_of(name.begin(), name.end(), [](wchar_t value) {
        return (value >= L'0' && value <= L'9') || (value >= L'a' && value <= L'f');
    });
}

std::filesystem::path uniquePath(
    const std::filesystem::path& root,
    std::wstring_view prefix)
{
    return root / (std::wstring(prefix) + std::to_wstring(GetCurrentProcessId()) + L"-"
        + std::to_wstring(GetTickCount64()) + L"-"
        + std::to_wstring(++uniqueSuffix));
}

bool writeTextNew(const std::filesystem::path& path, std::string_view text)
{
    const auto handle = CreateFileW(
        path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    DWORD written{};
    const auto result = text.size() <= std::numeric_limits<DWORD>::max()
        && WriteFile(handle, text.data(), static_cast<DWORD>(text.size()), &written, nullptr)
        && written == text.size();
    CloseHandle(handle);
    return result;
}

std::optional<std::string> readText(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

}

bool RuntimeCache::validateRuntime(
    const std::filesystem::path& root,
    std::string_view digest) const
{
    const auto markerText = readText(root / ".ready.json");
    const auto manifestText = readText(root / "payload-manifest.json");
    if (!markerText || !manifestText) return false;
    const auto marker = Json::parse(*markerText, nullptr, false);
    if (marker.is_discarded() || !marker.is_object() || marker.size() != 2
        || marker.value("schema_version", 0) != 1
        || marker.value("payload_sha256", std::string{}) != digest) {
        return false;
    }
    const auto manifest = PayloadManifest::parse(*manifestText);
    if (!manifest.ok()) return false;
    for (const auto& entry : manifest.value->entries()) {
        const auto file = root / std::filesystem::path(std::u8string(
            reinterpret_cast<const char8_t*>(entry.path.data()), entry.path.size()));
        std::error_code error;
        if (std::filesystem::symlink_status(file, error).type()
                != std::filesystem::file_type::regular
            || error || std::filesystem::file_size(file, error) != entry.size || error) {
            return false;
        }
        const auto actual = sha256File(file);
        if (!actual.ok() || *actual.value != entry.sha256) return false;
    }
    return true;
}

Result<PreparedRuntime> RuntimeCache::prepare(
    const std::filesystem::path& container,
    const PackageInfo& packageInfo)
{
    const auto digest = digestToHex(packageInfo.footer.payloadDigest);
    const auto mutexName = L"Local\\Tweakopedia.Runtime."
        + std::wstring(digest.begin(), digest.end());
    NamedMutex mutex(mutexName);
    if (!mutex.valid()) return cacheFailure("Cannot acquire runtime preparation mutex");

    std::error_code error;
    std::filesystem::create_directories(runtimeRoot_, error);
    if (error) return cacheFailure("Cannot create runtime cache root");

    for (const auto& item : std::filesystem::directory_iterator(runtimeRoot_, error)) {
        if (error) return cacheFailure("Cannot enumerate runtime cache root");
        const auto name = item.path().filename().wstring();
        if (item.is_directory(error) && name.starts_with(L".staging-")) {
            std::filesystem::remove_all(item.path(), error);
            if (error) return cacheFailure("Cannot remove stale staging directory");
        }
    }

    const auto finalRoot = runtimeRoot_ / std::filesystem::path(digest);
    if (validateRuntime(finalRoot, digest)) {
        auto lease = RuntimeLease::acquireShared(finalRoot);
        if (!lease.ok()) return cacheFailure(lease.error.message);
        return Result<PreparedRuntime>::success(
            {finalRoot, true, std::move(*lease.value)});
    }

    if (std::filesystem::exists(finalRoot, error)) {
        const auto quarantine = uniquePath(runtimeRoot_, L".invalid-");
        std::filesystem::rename(finalRoot, quarantine, error);
        if (error) return cacheFailure("Cannot quarantine invalid runtime");
        std::filesystem::remove_all(quarantine, error);
        if (error) return cacheFailure("Cannot remove invalid runtime quarantine");
    }

    if (fault_ == RuntimeCacheFault::CreateStaging) {
        return cacheFailure("Simulated staging creation failure");
    }
    const auto staging = uniquePath(runtimeRoot_, L".staging-");
    const auto extracted = ZipPayload::extract(container, packageInfo, staging);
    if (!extracted.ok()) return cacheFailure(extracted.error.message);

    auto failStaging = [&](std::string message) {
        std::filesystem::remove_all(staging, error);
        return cacheFailure(std::move(message));
    };
    if (!writeTextNew(staging / ".runtime.lock", {})) {
        return failStaging("Cannot create runtime lease file");
    }
    if (fault_ == RuntimeCacheFault::WriteMarker) {
        return failStaging("Simulated marker write failure");
    }
    const auto marker = Json{{"schema_version", 1}, {"payload_sha256", digest}}.dump();
    if (!writeTextNew(staging / ".ready.json", marker)) {
        return failStaging("Cannot write runtime ready marker");
    }
    if (fault_ == RuntimeCacheFault::Publish) {
        return failStaging("Simulated runtime publish failure");
    }

    std::filesystem::rename(staging, finalRoot, error);
    if (error) return failStaging("Cannot publish prepared runtime");
    auto lease = RuntimeLease::acquireShared(finalRoot);
    if (!lease.ok()) return cacheFailure(lease.error.message);
    return Result<PreparedRuntime>::success({finalRoot, false, std::move(*lease.value)});
}

CleanupReport RuntimeCache::cleanupOld(std::string_view currentDigest)
{
    CleanupReport report;
    std::error_code error;
    std::vector<std::filesystem::path> candidates;
    if (!std::filesystem::exists(runtimeRoot_, error) || error) return report;
    for (const auto& item : std::filesystem::directory_iterator(runtimeRoot_, error)) {
        if (error) break;
        const auto name = item.path().filename().wstring();
        if (item.is_directory(error) && isDigestName(name)
            && std::string(name.begin(), name.end()) != currentDigest) {
            candidates.push_back(item.path());
        }
    }
    std::sort(candidates.begin(), candidates.end());
    for (const auto& candidate : candidates) {
        auto lease = RuntimeLease::tryAcquireCleanup(candidate);
        if (!lease.ok()) {
            report.retained.push_back(candidate);
            continue;
        }
        std::filesystem::remove_all(candidate, error);
        if (error) {
            report.failures.push_back(candidate);
            error.clear();
        } else {
            report.removed.push_back(candidate);
        }
    }
    return report;
}

}
