#pragma once

#include "startup/StartupStage.h"

#include <windows.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace tweakopedia::bootstrap {

class StartupWindowModel final
{
public:
    void advance(startup::StartupStage stage);
    [[nodiscard]] std::vector<startup::StartupStage> visibleStages() const;
    [[nodiscard]] bool complete() const noexcept { return complete_; }

private:
    std::vector<startup::StartupStage> pipeline_ = startup::startupPipeline(false);
    startup::StartupStage current_{startup::StartupStage::ContainerIntegrity};
    bool started_{};
    bool complete_{};
};

class StartupWindow final
{
public:
    StartupWindow();
    ~StartupWindow();

    StartupWindow(const StartupWindow&) = delete;
    StartupWindow& operator=(const StartupWindow&) = delete;

    [[nodiscard]] bool create(HINSTANCE instance);
    void show();
    void report(startup::StartupStage stage) const;
    void hideSynchronously() const;
    void messageLoopUntil(HANDLE workerCompleted) const;
    [[nodiscard]] std::uintptr_t nativeHandle() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tweakopedia::bootstrap
