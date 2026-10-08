#include "package/PackageFooter.h"

#include "package/Sha256.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <string_view>

namespace tweakopedia::package {
namespace {

constexpr std::string_view magic{"TWPKG001"};

template<typename Integer>
void writeLittleEndian(std::span<std::byte> destination, Integer value)
{
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        destination[index] = static_cast<std::byte>((value >> (index * 8)) & 0xffU);
    }
}

template<typename Integer>
Integer readLittleEndian(std::span<const std::byte> source)
{
    Integer value{};
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        value |= static_cast<Integer>(std::to_integer<unsigned char>(source[index]))
            << (index * 8);
    }
    return value;
}

}

std::array<std::byte, PackageFooter::fixedSize> PackageFooter::encode() const
{
    std::array<std::byte, fixedSize> encoded{};
    std::copy(magic.begin(), magic.end(), reinterpret_cast<char*>(encoded.data()));
    writeLittleEndian<std::uint32_t>(std::span(encoded).subspan<8, 4>(), schemaVersion);
    writeLittleEndian<std::uint32_t>(std::span(encoded).subspan<12, 4>(), flags);
    writeLittleEndian<std::uint64_t>(std::span(encoded).subspan<16, 8>(), payloadOffset);
    writeLittleEndian<std::uint64_t>(std::span(encoded).subspan<24, 8>(), payloadSize);
    std::copy(payloadDigest.begin(), payloadDigest.end(), encoded.begin() + 32);
    return encoded;
}

Result<PackageFooter> PackageFooter::decode(std::span<const std::byte> bytes)
{
    if (bytes.size() != fixedSize) {
        return Result<PackageFooter>::failure(ErrorCode::InvalidFooter, "Invalid footer size");
    }
    if (!std::equal(magic.begin(), magic.end(), reinterpret_cast<const char*>(bytes.data()))) {
        return Result<PackageFooter>::failure(ErrorCode::InvalidFooter, "Invalid footer magic");
    }

    PackageFooter footer;
    footer.schemaVersion = readLittleEndian<std::uint32_t>(bytes.subspan(8, 4));
    if (footer.schemaVersion != schemaVersionCurrent) {
        return Result<PackageFooter>::failure(
            ErrorCode::UnsupportedVersion, "Unsupported footer schema version");
    }
    footer.flags = readLittleEndian<std::uint32_t>(bytes.subspan(12, 4));
    footer.payloadOffset = readLittleEndian<std::uint64_t>(bytes.subspan(16, 8));
    footer.payloadSize = readLittleEndian<std::uint64_t>(bytes.subspan(24, 8));
    std::copy(bytes.begin() + 32, bytes.end(), footer.payloadDigest.begin());
    return Result<PackageFooter>::success(footer);
}

Result<PackageInfo> inspectContainer(const std::filesystem::path& path)
{
    std::error_code sizeError;
    const auto fileSize = std::filesystem::file_size(path, sizeError);
    if (sizeError) return Result<PackageInfo>::failure(ErrorCode::Io, "Cannot read container size");
    if (fileSize < PackageFooter::fixedSize) {
        return Result<PackageInfo>::failure(ErrorCode::InvalidFooter, "Container is too small");
    }

    const auto footerOffset = fileSize - PackageFooter::fixedSize;
    if (footerOffset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())) {
        return Result<PackageInfo>::failure(ErrorCode::OutOfBounds, "Footer is not seekable");
    }
    std::ifstream stream(path, std::ios::binary);
    std::array<std::byte, PackageFooter::fixedSize> bytes{};
    if (!stream.seekg(static_cast<std::streamoff>(footerOffset))
        || !stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) {
        return Result<PackageInfo>::failure(ErrorCode::Io, "Cannot read package footer");
    }

    const auto decoded = PackageFooter::decode(bytes);
    if (!decoded.ok()) return Result<PackageInfo>::failure(decoded.error.code, decoded.error.message);
    const auto& footer = *decoded.value;
    if (footer.payloadOffset > footerOffset
        || footer.payloadSize > footerOffset - footer.payloadOffset
        || footer.payloadOffset + footer.payloadSize != footerOffset) {
        return Result<PackageInfo>::failure(ErrorCode::OutOfBounds, "Payload slice is invalid");
    }

    const auto digest = sha256File(path, footer.payloadOffset, footer.payloadSize);
    if (!digest.ok()) return Result<PackageInfo>::failure(digest.error.code, digest.error.message);
    if (*digest.value != footer.payloadDigest) {
        return Result<PackageInfo>::failure(ErrorCode::DigestMismatch, "Payload digest mismatch");
    }

    return Result<PackageInfo>::success({path, fileSize, footer});
}

}
