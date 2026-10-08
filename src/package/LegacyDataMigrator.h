#pragma once

#include <filesystem>
#include <vector>

namespace tweakopedia::package {

struct MigrationReport {
    bool complete{};
    std::vector<std::filesystem::path> moved;
    std::vector<std::filesystem::path> conflicts;
    std::vector<std::filesystem::path> failures;
};

class LegacyDataMigrator final
{
public:
    static MigrationReport migrate(
        const std::filesystem::path& productRoot,
        const std::filesystem::path& dataRoot);
};

} // namespace tweakopedia::package
