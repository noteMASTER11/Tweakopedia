#pragma once

#include "package/PackageTypes.h"

#include <cstdint>
#include <filesystem>
#include <optional>

namespace tweakopedia::package {

Result<Digest> sha256File(
    const std::filesystem::path& path,
    std::uint64_t offset = 0,
    std::optional<std::uint64_t> length = {});

}
