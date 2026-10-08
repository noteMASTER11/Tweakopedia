#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace tweakopedia::package {

using Digest = std::array<std::byte, 32>;

enum class ErrorCode {
    None,
    Io,
    InvalidArgument,
    InvalidFooter,
    UnsupportedVersion,
    OutOfBounds,
    DigestMismatch,
    CryptoFailure,
    InvalidManifest,
    InvalidArchive,
    CacheFailure,
};

struct Error {
    ErrorCode code{ErrorCode::None};
    std::string message;
};

template<typename T>
struct Result {
    std::optional<T> value;
    Error error;

    [[nodiscard]] bool ok() const { return value.has_value(); }

    static Result success(T result)
    {
        return {.value = std::move(result), .error = {}};
    }

    static Result failure(ErrorCode code, std::string message)
    {
        return {.value = std::nullopt, .error = {code, std::move(message)}};
    }
};

}
