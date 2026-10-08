#include "package/PayloadManifest.h"

#include "package/Sha256.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <set>
#include <system_error>

namespace tweakopedia::package {
namespace {

using Json = nlohmann::json;

constexpr std::array requiredPaths{
    std::string_view{"Tweakopedia.App.exe"},
    std::string_view{"Tweakopedia.Executor.exe"},
    std::string_view{"content/categories.yaml"},
};

template<std::size_t Size>
bool hasExactKeys(const Json& object, const std::array<std::string_view, Size>& expected)
{
    if (!object.is_object() || object.size() != expected.size()) return false;
    return std::all_of(expected.begin(), expected.end(), [&](std::string_view key) {
        return object.contains(std::string(key));
    });
}

bool validPath(std::string_view path)
{
    if (path.empty() || path == "payload-manifest.json" || path.front() == '/'
        || path.find('\\') != std::string_view::npos || path.find(':') != std::string_view::npos) {
        return false;
    }
    std::size_t start{};
    while (start <= path.size()) {
        const auto end = path.find('/', start);
        const auto segment = path.substr(start, end == std::string_view::npos
            ? path.size() - start : end - start);
        if (segment.empty() || segment == "." || segment == "..") return false;
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return true;
}

bool decodeDigest(std::string_view text, Digest& digest)
{
    if (text.size() != digest.size() * 2) return false;
    auto nibble = [](char value) -> int {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return 10 + value - 'a';
        return -1;
    };
    for (std::size_t index = 0; index < digest.size(); ++index) {
        const auto high = nibble(text[index * 2]);
        const auto low = nibble(text[index * 2 + 1]);
        if (high < 0 || low < 0) return false;
        digest[index] = static_cast<std::byte>((high << 4) | low);
    }
    return true;
}

std::string encodeDigest(const Digest& digest)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string encoded(digest.size() * 2, '0');
    for (std::size_t index = 0; index < digest.size(); ++index) {
        const auto value = std::to_integer<unsigned char>(digest[index]);
        encoded[index * 2] = digits[value >> 4];
        encoded[index * 2 + 1] = digits[value & 0x0f];
    }
    return encoded;
}

bool hasRequiredEntries(const std::vector<ManifestEntry>& entries)
{
    return std::all_of(requiredPaths.begin(), requiredPaths.end(), [&](std::string_view required) {
        return std::any_of(entries.begin(), entries.end(), [&](const ManifestEntry& entry) {
            return entry.path == required;
        });
    });
}

Result<PayloadManifest> invalidManifest(std::string message)
{
    return Result<PayloadManifest>::failure(ErrorCode::InvalidManifest, std::move(message));
}

}

Result<PayloadManifest> PayloadManifest::parse(std::string_view jsonBytes)
{
    const auto root = Json::parse(jsonBytes.begin(), jsonBytes.end(), nullptr, false, true);
    if (root.is_discarded()) return invalidManifest("Manifest is not valid UTF-8 JSON");
    if (!hasExactKeys(root, std::array{std::string_view{"schema_version"},
                                      std::string_view{"files"}})) {
        return invalidManifest("Manifest root schema is invalid");
    }
    if (!root["schema_version"].is_number_unsigned()
        || root["schema_version"].get<std::uint64_t>() != schemaVersionCurrent
        || !root["files"].is_array()) {
        return invalidManifest("Manifest schema version or files array is invalid");
    }

    std::vector<ManifestEntry> entries;
    std::set<std::string> paths;
    for (const auto& item : root["files"]) {
        if (!hasExactKeys(item, std::array{std::string_view{"path"},
                                           std::string_view{"size"},
                                           std::string_view{"sha256"}})
            || !item["path"].is_string() || !item["size"].is_number_unsigned()
            || !item["sha256"].is_string()) {
            return invalidManifest("Manifest entry schema is invalid");
        }

        ManifestEntry entry;
        entry.path = item["path"].get<std::string>();
        entry.size = item["size"].get<std::uint64_t>();
        const auto digest = item["sha256"].get<std::string>();
        if (!validPath(entry.path) || !decodeDigest(digest, entry.sha256)) {
            return invalidManifest("Manifest entry path or digest is invalid");
        }
        if (!paths.insert(entry.path).second) return invalidManifest("Duplicate manifest path");
        entries.push_back(std::move(entry));
    }
    if (!hasRequiredEntries(entries)) return invalidManifest("Required payload entry is missing");
    std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
        return left.path < right.path;
    });
    return Result<PayloadManifest>::success(PayloadManifest(std::move(entries)));
}

Result<PayloadManifest> PayloadManifest::fromDirectory(const std::filesystem::path& root)
{
    std::error_code error;
    if (!std::filesystem::is_directory(root, error) || error) {
        return invalidManifest("Payload root is not a directory");
    }

    std::vector<ManifestEntry> entries;
    std::filesystem::recursive_directory_iterator iterator(root, error), end;
    if (error) return invalidManifest("Cannot enumerate payload root");
    for (; iterator != end; iterator.increment(error)) {
        if (error) return invalidManifest("Cannot enumerate payload root");
        const auto& item = *iterator;
        if (item.is_directory(error)) continue;
        if (error || !item.is_regular_file(error) || error) {
            return invalidManifest("Payload contains a non-regular file");
        }
        const auto relative = std::filesystem::relative(item.path(), root, error);
        if (error) return invalidManifest("Cannot make payload path relative");
        const auto utf8 = relative.generic_u8string();
        std::string path(reinterpret_cast<const char*>(utf8.data()), utf8.size());
        if (path == "payload-manifest.json") continue;
        if (!validPath(path)) return invalidManifest("Payload path is invalid");

        const auto size = item.file_size(error);
        if (error) return invalidManifest("Cannot read payload file size");
        const auto digest = sha256File(item.path());
        if (!digest.ok()) return invalidManifest(digest.error.message);
        entries.push_back({std::move(path), size, *digest.value});
    }

    std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
        return left.path < right.path;
    });
    if (!hasRequiredEntries(entries)) return invalidManifest("Required payload entry is missing");
    return Result<PayloadManifest>::success(PayloadManifest(std::move(entries)));
}

std::string PayloadManifest::toCanonicalJson() const
{
    Json files = Json::array();
    for (const auto& entry : entries_) {
        files.push_back({
            {"path", entry.path},
            {"size", entry.size},
            {"sha256", encodeDigest(entry.sha256)},
        });
    }
    return Json{{"schema_version", schemaVersionCurrent}, {"files", std::move(files)}}.dump();
}

const ManifestEntry* PayloadManifest::find(std::string_view path) const
{
    const auto iterator = std::lower_bound(
        entries_.begin(), entries_.end(), path,
        [](const ManifestEntry& entry, std::string_view value) { return entry.path < value; });
    return iterator != entries_.end() && iterator->path == path ? &*iterator : nullptr;
}

}
