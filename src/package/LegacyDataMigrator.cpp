#include "package/LegacyDataMigrator.h"

#include "package/Sha256.h"

#include <array>
#include <fstream>
#include <system_error>

namespace tweakopedia::package {
namespace {

constexpr std::string_view markerName = ".layout-v1-complete";
constexpr std::array<std::string_view, 3> legacyEntries{
    "tweakopedia.db",
    "logs",
    "transactions",
};

bool sameFile(const std::filesystem::path& source, const std::filesystem::path& destination)
{
    std::error_code error;
    if (std::filesystem::file_size(source, error) != std::filesystem::file_size(destination, error) || error) {
        return false;
    }

    const auto sourceDigest = sha256File(source);
    const auto destinationDigest = sha256File(destination);
    return sourceDigest.ok() && destinationDigest.ok()
        && *sourceDigest.value == *destinationDigest.value;
}

bool verifyCopy(const std::filesystem::path& source, const std::filesystem::path& destination)
{
    std::error_code error;
    const auto sourceStatus = std::filesystem::symlink_status(source, error);
    if (error || std::filesystem::is_symlink(sourceStatus)) {
        return false;
    }
    if (std::filesystem::is_regular_file(sourceStatus)) {
        return std::filesystem::is_regular_file(destination, error) && !error
            && sameFile(source, destination);
    }
    if (!std::filesystem::is_directory(sourceStatus)
        || !std::filesystem::is_directory(destination, error) || error) {
        return false;
    }

    for (std::filesystem::recursive_directory_iterator iterator(source, error), end;
         !error && iterator != end; iterator.increment(error)) {
        const auto relative = std::filesystem::relative(iterator->path(), source, error);
        if (error) {
            return false;
        }
        const auto destinationEntry = destination / relative;
        const auto status = iterator->symlink_status(error);
        if (error || std::filesystem::is_symlink(status)) {
            return false;
        }
        if (std::filesystem::is_directory(status)) {
            if (!std::filesystem::is_directory(destinationEntry, error) || error) {
                return false;
            }
        } else if (!std::filesystem::is_regular_file(status)
            || !sameFile(iterator->path(), destinationEntry)) {
            return false;
        }
    }
    if (error) {
        return false;
    }

    for (std::filesystem::recursive_directory_iterator iterator(destination, error), end;
         !error && iterator != end; iterator.increment(error)) {
        const auto relative = std::filesystem::relative(iterator->path(), destination, error);
        if (error || !std::filesystem::exists(source / relative)) {
            return false;
        }
    }
    return !error;
}

bool copyThenRemove(const std::filesystem::path& source, const std::filesystem::path& destination)
{
    std::error_code error;
    const auto status = std::filesystem::symlink_status(source, error);
    if (error || std::filesystem::is_symlink(status)) {
        return false;
    }

    if (std::filesystem::is_directory(status)) {
        std::filesystem::copy(source, destination, std::filesystem::copy_options::recursive, error);
    } else if (std::filesystem::is_regular_file(status)) {
        std::filesystem::copy_file(source, destination, error);
    } else {
        return false;
    }
    if (error || !verifyCopy(source, destination)) {
        std::filesystem::remove_all(destination, error);
        return false;
    }

    std::filesystem::remove_all(source, error);
    return !error;
}

bool moveEntry(const std::filesystem::path& source, const std::filesystem::path& destination)
{
    std::error_code error;
    std::filesystem::rename(source, destination, error);
    return !error || copyThenRemove(source, destination);
}

} // namespace

MigrationReport LegacyDataMigrator::migrate(
    const std::filesystem::path& productRoot,
    const std::filesystem::path& dataRoot)
{
    MigrationReport report;
    std::error_code error;
    std::filesystem::create_directories(dataRoot, error);
    if (error) {
        report.failures.push_back(dataRoot);
        return report;
    }

    for (const auto name : legacyEntries) {
        const auto source = productRoot / name;
        const auto destination = dataRoot / name;
        if (!std::filesystem::exists(source, error)) {
            if (error) {
                report.failures.push_back(source);
                error.clear();
            }
            continue;
        }
        if (std::filesystem::exists(destination, error)) {
            report.conflicts.push_back(source);
            error.clear();
            continue;
        }
        if (!moveEntry(source, destination)) {
            report.failures.push_back(source);
            continue;
        }
        report.moved.push_back(source);
    }

    report.complete = report.conflicts.empty() && report.failures.empty();
    if (report.complete) {
        for (const auto name : legacyEntries) {
            if (std::filesystem::exists(productRoot / name, error) || error) {
                report.complete = false;
                break;
            }
        }
    }

    const auto marker = dataRoot / markerName;
    if (report.complete) {
        std::ofstream markerStream(marker, std::ios::binary | std::ios::trunc);
        markerStream << "1\n";
        if (!markerStream) {
            report.complete = false;
            report.failures.push_back(marker);
        }
    }
    if (!report.complete) {
        std::filesystem::remove(marker, error);
    }
    return report;
}

} // namespace tweakopedia::package
