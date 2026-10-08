#include "package/Sha256.h"

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <vector>

namespace tweakopedia::package {
namespace {

Result<Digest> cryptoFailure(const char* operation)
{
    return Result<Digest>::failure(ErrorCode::CryptoFailure, operation);
}

}

Result<Digest> sha256File(
    const std::filesystem::path& path,
    std::uint64_t offset,
    std::optional<std::uint64_t> requestedLength)
{
    std::error_code sizeError;
    const auto fileSize = std::filesystem::file_size(path, sizeError);
    if (sizeError) return Result<Digest>::failure(ErrorCode::Io, "Cannot read file size");
    if (offset > fileSize) {
        return Result<Digest>::failure(ErrorCode::OutOfBounds, "Hash offset exceeds file size");
    }
    const auto available = fileSize - offset;
    const auto length = requestedLength.value_or(available);
    if (length > available) {
        return Result<Digest>::failure(ErrorCode::OutOfBounds, "Hash length exceeds file size");
    }
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())) {
        return Result<Digest>::failure(ErrorCode::OutOfBounds, "Hash offset is not seekable");
    }

    BCRYPT_ALG_HANDLE algorithm{};
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(
            &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
        return cryptoFailure("BCryptOpenAlgorithmProvider failed");
    }

    DWORD objectSize{};
    DWORD bytesWritten{};
    if (!BCRYPT_SUCCESS(BCryptGetProperty(
            algorithm, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &bytesWritten, 0))) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return cryptoFailure("BCryptGetProperty failed");
    }

    std::vector<UCHAR> hashObject(objectSize);
    BCRYPT_HASH_HANDLE hash{};
    if (!BCRYPT_SUCCESS(BCryptCreateHash(
            algorithm, &hash, hashObject.data(), objectSize, nullptr, 0, 0))) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return cryptoFailure("BCryptCreateHash failed");
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream || !stream.seekg(static_cast<std::streamoff>(offset))) {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return Result<Digest>::failure(ErrorCode::Io, "Cannot open or seek file");
    }

    std::array<char, 64 * 1024> buffer{};
    std::uint64_t remaining = length;
    while (remaining > 0) {
        const auto chunk = static_cast<std::streamsize>(
            std::min<std::uint64_t>(remaining, buffer.size()));
        stream.read(buffer.data(), chunk);
        if (stream.gcount() != chunk
            || !BCRYPT_SUCCESS(BCryptHashData(
                hash, reinterpret_cast<PUCHAR>(buffer.data()),
                static_cast<ULONG>(chunk), 0))) {
            BCryptDestroyHash(hash);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            return Result<Digest>::failure(ErrorCode::Io, "Cannot hash requested file slice");
        }
        remaining -= static_cast<std::uint64_t>(chunk);
    }

    Digest digest{};
    const auto finalStatus = BCryptFinishHash(
        hash, reinterpret_cast<PUCHAR>(digest.data()), static_cast<ULONG>(digest.size()), 0);
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!BCRYPT_SUCCESS(finalStatus)) return cryptoFailure("BCryptFinishHash failed");
    return Result<Digest>::success(digest);
}

}
