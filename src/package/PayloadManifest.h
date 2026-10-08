#pragma once

#include "package/PackageTypes.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tweakopedia::package {

struct ManifestEntry {
    std::string path;
    std::uint64_t size{};
    Digest sha256{};
};

class PayloadManifest final
{
public:
    static constexpr std::uint32_t schemaVersionCurrent = 1;

    static Result<PayloadManifest> parse(std::string_view jsonBytes);
    static Result<PayloadManifest> fromDirectory(const std::filesystem::path& root);

    [[nodiscard]] std::string toCanonicalJson() const;
    [[nodiscard]] const ManifestEntry* find(std::string_view path) const;
    [[nodiscard]] const std::vector<ManifestEntry>& entries() const { return entries_; }

private:
    explicit PayloadManifest(std::vector<ManifestEntry> entries)
        : entries_(std::move(entries))
    {
    }

    std::vector<ManifestEntry> entries_;
};

}
