#pragma once

#include "package/PackageFooter.h"
#include "package/RuntimeLease.h"

#include <filesystem>
#include <string_view>
#include <vector>

namespace tweakopedia::package {

enum class RuntimeCacheFault { None, CreateStaging, WriteMarker, Publish };

struct PreparedRuntime {
    std::filesystem::path root;
    bool reused{};
    RuntimeLease lease;
};

struct CleanupReport {
    std::vector<std::filesystem::path> removed;
    std::vector<std::filesystem::path> retained;
    std::vector<std::filesystem::path> failures;
};

class RuntimeCache final
{
public:
    explicit RuntimeCache(
        std::filesystem::path runtimeRoot,
        RuntimeCacheFault fault = RuntimeCacheFault::None)
        : runtimeRoot_(std::move(runtimeRoot)), fault_(fault)
    {
    }

    Result<PreparedRuntime> prepare(
        const std::filesystem::path& container,
        const PackageInfo& packageInfo);
    CleanupReport cleanupOld(std::string_view currentDigest);

    [[nodiscard]] const std::filesystem::path& root() const { return runtimeRoot_; }

private:
    enum class ValidationResult { Valid, Invalid, Inaccessible };
    ValidationResult validateRuntime(
        const std::filesystem::path& root,
        std::string_view digest) const;

    std::filesystem::path runtimeRoot_;
    RuntimeCacheFault fault_{};
};

}
