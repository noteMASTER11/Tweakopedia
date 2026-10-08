#pragma once

#include "package/PackageTypes.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace tweakopedia::package {

Result<Digest> sha256File(
    const std::filesystem::path& path,
    std::uint64_t offset = 0,
    std::optional<std::uint64_t> length = {});

std::string digestToHex(const Digest& digest);
std::optional<Digest> digestFromHex(std::string_view encoded);

}
