#pragma once

#include "package/PackageTypes.h"

#include <filesystem>

namespace tweakopedia::package {

class RuntimeLease final
{
public:
    RuntimeLease() = default;
    ~RuntimeLease();
    RuntimeLease(const RuntimeLease&) = delete;
    RuntimeLease& operator=(const RuntimeLease&) = delete;
    RuntimeLease(RuntimeLease&& other) noexcept;
    RuntimeLease& operator=(RuntimeLease&& other) noexcept;

    [[nodiscard]] bool valid() const;
    void reset();

    static Result<RuntimeLease> acquireShared(const std::filesystem::path& runtimeRoot);
    static Result<RuntimeLease> tryAcquireCleanup(const std::filesystem::path& runtimeRoot);

private:
    explicit RuntimeLease(void* handle) : handle_(handle) {}
    static Result<RuntimeLease> acquire(
        const std::filesystem::path& runtimeRoot,
        unsigned long desiredAccess,
        unsigned long shareMode);
    void* handle_{};
};

}
