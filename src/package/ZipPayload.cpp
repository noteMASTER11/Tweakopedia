#include "package/ZipPayload.h"

#include "package/Sha256.h"

#include <windows.h>

#include <miniz.h>

#include <algorithm>
#include <fstream>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace tweakopedia::package {
namespace {

constexpr MZ_TIME_T fixedZipTime = 315532800;
constexpr std::uint64_t maxManifestSize = 16ULL * 1024 * 1024;

Result<void> zipFailure(std::string message)
{
    return Result<void>::failure(ErrorCode::InvalidArchive, std::move(message));
}

std::string narrowPath(const std::filesystem::path& path)
{
    const auto utf8 = path.generic_u8string();
    return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}

std::filesystem::path pathFromUtf8(std::string_view value)
{
    return std::filesystem::path(std::u8string(
        reinterpret_cast<const char8_t*>(value.data()), value.size()));
}

std::string archiveName(mz_zip_archive& archive, mz_uint index)
{
    const auto length = mz_zip_reader_get_filename(&archive, index, nullptr, 0);
    if (length < 2) return {};
    std::vector<char> buffer(length);
    if (mz_zip_reader_get_filename(&archive, index, buffer.data(), length) != length) return {};
    return {buffer.data(), length - 1};
}

bool isNormalFile(const mz_zip_archive_file_stat& stat)
{
    if (stat.m_is_directory || stat.m_is_encrypted || !stat.m_is_supported) return false;
    const auto sourceSystem = static_cast<unsigned>((stat.m_version_made_by >> 8) & 0xffU);
    if (sourceSystem != 3) return true;
    const auto fileType = (stat.m_external_attr >> 16) & 0170000U;
    return fileType == 0 || fileType == 0100000U;
}

struct FileWriter {
    HANDLE handle{INVALID_HANDLE_VALUE};
    bool failed{};
};

size_t writeExtracted(void* opaque, mz_uint64 offset, const void* buffer, size_t size)
{
    auto& writer = *static_cast<FileWriter*>(opaque);
    LARGE_INTEGER position;
    position.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(writer.handle, position, nullptr, FILE_BEGIN)) {
        writer.failed = true;
        return 0;
    }

    const auto* source = static_cast<const std::byte*>(buffer);
    size_t writtenTotal{};
    while (writtenTotal < size) {
        const auto chunk = static_cast<DWORD>(std::min<std::size_t>(
            size - writtenTotal, std::numeric_limits<DWORD>::max()));
        DWORD written{};
        if (!WriteFile(writer.handle, source + writtenTotal, chunk, &written, nullptr)
            || written != chunk) {
            writer.failed = true;
            return writtenTotal;
        }
        writtenTotal += written;
    }
    return writtenTotal;
}

Result<void> extractNewFile(
    mz_zip_archive& archive,
    mz_uint index,
    const std::filesystem::path& output)
{
    std::error_code error;
    std::filesystem::create_directories(output.parent_path(), error);
    if (error) return zipFailure("Cannot create extraction directory");

    FileWriter writer;
    writer.handle = CreateFileW(
        output.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (writer.handle == INVALID_HANDLE_VALUE) return zipFailure("Cannot create extracted file");
    const auto extracted = mz_zip_reader_extract_to_callback(
        &archive, index, writeExtracted, &writer, 0);
    CloseHandle(writer.handle);
    if (!extracted || writer.failed) return zipFailure("Cannot extract archive entry");
    return Result<void>::success();
}

void removeFailedDestination(const std::filesystem::path& destination)
{
    std::error_code ignored;
    std::filesystem::remove_all(destination, ignored);
}

}

Result<void> ZipPayload::create(
    const std::filesystem::path& sourceRoot,
    const std::filesystem::path& outputZip,
    const PayloadManifest& manifest)
{
    std::error_code error;
    if (!std::filesystem::is_directory(sourceRoot, error) || error
        || std::filesystem::exists(outputZip, error)) {
        return zipFailure("ZIP source or output path is invalid");
    }

    mz_zip_archive archive{};
    const auto outputName = narrowPath(outputZip);
    if (!mz_zip_writer_init_file(&archive, outputName.c_str(), 0)) {
        return zipFailure("Cannot initialize ZIP writer");
    }
    auto fail = [&](std::string message) {
        mz_zip_writer_end(&archive);
        std::filesystem::remove(outputZip, error);
        return zipFailure(std::move(message));
    };

    const auto manifestJson = manifest.toCanonicalJson();
    auto timestamp = fixedZipTime;
    if (!mz_zip_writer_add_mem_ex_v2(
            &archive, "payload-manifest.json", manifestJson.data(), manifestJson.size(),
            nullptr, 0, MZ_BEST_COMPRESSION, 0, 0, &timestamp, nullptr, 0, nullptr, 0)) {
        return fail("Cannot add payload manifest to ZIP");
    }

    for (const auto& entry : manifest.entries()) {
        const auto source = sourceRoot / pathFromUtf8(entry.path);
        if (!std::filesystem::is_regular_file(source, error) || error) {
            return fail("Manifest file is missing from payload source");
        }
        const auto size = std::filesystem::file_size(source, error);
        if (error || size != entry.size || size > std::numeric_limits<std::size_t>::max()) {
            return fail("Payload source file size does not match manifest");
        }
        const auto digest = sha256File(source);
        if (!digest.ok() || *digest.value != entry.sha256) {
            return fail("Payload source digest does not match manifest");
        }

        std::ifstream stream(source, std::ios::binary | std::ios::ate);
        if (!stream) return fail("Cannot read payload source file");
        std::vector<std::byte> bytes(static_cast<std::size_t>(size));
        stream.seekg(0);
        if (size > 0 && !stream.read(
                reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
            return fail("Cannot read payload source file");
        }
        timestamp = fixedZipTime;
        if (!mz_zip_writer_add_mem_ex_v2(
                &archive, entry.path.c_str(), bytes.data(), bytes.size(), nullptr, 0,
                MZ_BEST_COMPRESSION, 0, 0, &timestamp, nullptr, 0, nullptr, 0)) {
            return fail("Cannot add payload file to ZIP");
        }
    }

    if (!mz_zip_writer_finalize_archive(&archive) || !mz_zip_writer_end(&archive)) {
        std::filesystem::remove(outputZip, error);
        return zipFailure("Cannot finalize ZIP payload");
    }
    return Result<void>::success();
}

Result<void> ZipPayload::extract(
    const std::filesystem::path& containerPath,
    const PackageInfo& package,
    const std::filesystem::path& destination)
{
    std::error_code error;
    const auto containerSize = std::filesystem::file_size(containerPath, error);
    if (error || package.footer.payloadOffset > containerSize
        || package.footer.payloadSize > containerSize - package.footer.payloadOffset
        || std::filesystem::exists(destination, error)) {
        return zipFailure("Container slice or destination is invalid");
    }

    mz_zip_archive archive{};
    const auto containerName = narrowPath(containerPath);
    if (!mz_zip_reader_init_file_v2(
            &archive, containerName.c_str(), 0,
            package.footer.payloadOffset, package.footer.payloadSize)) {
        return zipFailure("Cannot open ZIP payload");
    }
    auto closeWith = [&](Result<void> result) {
        mz_zip_reader_end(&archive);
        return result;
    };

    const auto fileCount = mz_zip_reader_get_num_files(&archive);
    if (fileCount == 0 || archiveName(archive, 0) != "payload-manifest.json") {
        return closeWith(zipFailure("Payload manifest is not the first ZIP entry"));
    }
    mz_zip_archive_file_stat manifestStat{};
    if (!mz_zip_reader_file_stat(&archive, 0, &manifestStat)
        || !isNormalFile(manifestStat) || manifestStat.m_uncomp_size > maxManifestSize) {
        return closeWith(zipFailure("Payload manifest ZIP entry is invalid"));
    }
    size_t manifestSize{};
    void* manifestBytes = mz_zip_reader_extract_to_heap(&archive, 0, &manifestSize, 0);
    if (!manifestBytes) return closeWith(zipFailure("Cannot read payload manifest"));
    const auto parsed = PayloadManifest::parse(
        std::string_view(static_cast<const char*>(manifestBytes), manifestSize));
    mz_free(manifestBytes);
    if (!parsed.ok()) return closeWith(zipFailure(parsed.error.message));
    const auto& manifest = *parsed.value;
    if (fileCount != manifest.entries().size() + 1) {
        return closeWith(zipFailure("ZIP entry count does not match manifest"));
    }

    std::set<std::string> seen;
    std::uint64_t expanded = manifestStat.m_uncomp_size;
    for (mz_uint index = 1; index < fileCount; ++index) {
        mz_zip_archive_file_stat stat{};
        const auto name = archiveName(archive, index);
        if (name.empty() || !mz_zip_reader_file_stat(&archive, index, &stat)
            || !isNormalFile(stat) || !seen.insert(name).second) {
            return closeWith(zipFailure("ZIP entry is invalid or duplicated"));
        }
        const auto* declared = manifest.find(name);
        if (!declared || declared->size != stat.m_uncomp_size) {
            return closeWith(zipFailure("ZIP entry is not declared or has the wrong size"));
        }
        if (stat.m_uncomp_size > maxExpandedSize - expanded) {
            return closeWith(zipFailure("Expanded payload exceeds configured limit"));
        }
        expanded += stat.m_uncomp_size;
    }

    if (!std::filesystem::create_directories(destination, error) || error) {
        return closeWith(zipFailure("Cannot create extraction root"));
    }
    auto extractionFailure = [&](std::string message) {
        mz_zip_reader_end(&archive);
        removeFailedDestination(destination);
        return zipFailure(std::move(message));
    };

    auto result = extractNewFile(archive, 0, destination / "payload-manifest.json");
    if (!result.ok()) return extractionFailure(result.error.message);
    for (mz_uint index = 1; index < fileCount; ++index) {
        const auto name = archiveName(archive, index);
        const auto* declared = manifest.find(name);
        const auto output = destination / pathFromUtf8(name);
        result = extractNewFile(archive, index, output);
        if (!result.ok()) return extractionFailure(result.error.message);
        const auto digest = sha256File(output);
        if (!digest.ok() || *digest.value != declared->sha256) {
            return extractionFailure("Extracted file digest does not match manifest");
        }
    }

    mz_zip_reader_end(&archive);
    return Result<void>::success();
}

}
