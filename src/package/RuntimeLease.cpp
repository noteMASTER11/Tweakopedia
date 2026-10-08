#include "package/RuntimeLease.h"

#include <windows.h>

namespace tweakopedia::package {

Result<RuntimeLease> RuntimeLease::acquire(
    const std::filesystem::path& runtimeRoot,
    unsigned long desiredAccess,
    unsigned long shareMode)
{
    const auto lockPath = runtimeRoot / ".runtime.lock";
    const auto handle = CreateFileW(
        lockPath.c_str(), desiredAccess, shareMode, nullptr, OPEN_ALWAYS,
        FILE_ATTRIBUTE_HIDDEN, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return Result<RuntimeLease>::failure(ErrorCode::CacheFailure, "Runtime lease is unavailable");
    }
    return Result<RuntimeLease>::success(RuntimeLease(handle));
}

RuntimeLease::~RuntimeLease()
{
    reset();
}

RuntimeLease::RuntimeLease(RuntimeLease&& other) noexcept
    : handle_(other.handle_)
{
    other.handle_ = nullptr;
}

RuntimeLease& RuntimeLease::operator=(RuntimeLease&& other) noexcept
{
    if (this == &other) return *this;
    reset();
    handle_ = other.handle_;
    other.handle_ = nullptr;
    return *this;
}

bool RuntimeLease::valid() const
{
    return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
}

void RuntimeLease::reset()
{
    if (valid()) CloseHandle(static_cast<HANDLE>(handle_));
    handle_ = nullptr;
}

Result<RuntimeLease> RuntimeLease::acquireShared(const std::filesystem::path& runtimeRoot)
{
    return acquire(runtimeRoot, GENERIC_READ, FILE_SHARE_READ);
}

Result<RuntimeLease> RuntimeLease::tryAcquireCleanup(const std::filesystem::path& runtimeRoot)
{
    return acquire(runtimeRoot, DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE);
}

}
