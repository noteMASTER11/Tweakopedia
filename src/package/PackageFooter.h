#pragma once

#include "package/PackageTypes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

namespace tweakopedia::package {

struct PackageFooter {
    static constexpr std::size_t fixedSize = 64;
    static constexpr std::uint32_t schemaVersionCurrent = 1;

    std::uint32_t schemaVersion{schemaVersionCurrent};
    std::uint32_t flags{};
    std::uint64_t payloadOffset{};
    std::uint64_t payloadSize{};
    Digest payloadDigest{};

    [[nodiscard]] std::array<std::byte, fixedSize> encode() const;
    static Result<PackageFooter> decode(std::span<const std::byte> bytes);
};

struct PackageInfo {
    std::filesystem::path containerPath;
    std::uint64_t containerSize{};
    PackageFooter footer;
};

Result<PackageInfo> inspectContainer(const std::filesystem::path& path);

}
