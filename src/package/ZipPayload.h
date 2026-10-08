#pragma once

#include "package/PackageFooter.h"
#include "package/PayloadManifest.h"

#include <cstdint>
#include <filesystem>

namespace tweakopedia::package {

class ZipPayload final
{
public:
    static constexpr std::uint64_t maxExpandedSize = 2ULL * 1024 * 1024 * 1024;

    static Result<void> create(
        const std::filesystem::path& sourceRoot,
        const std::filesystem::path& outputZip,
        const PayloadManifest& manifest);

    static Result<void> extract(
        const std::filesystem::path& containerPath,
        const PackageInfo& package,
        const std::filesystem::path& destination);
};

}
