#pragma once

#include "platform/IWindowsComponentBackend.h"

#include <QByteArray>

namespace tweakopedia::execution {

struct WindowsComponentExecutionResult {
    bool success{};
    bool restartRequired{};
    QString code;
    QString message;
};

struct WindowsComponentCaptureResult : WindowsComponentExecutionResult {
    domain::WindowsComponentSnapshot snapshot;
};

class WindowsComponentExecutor final
{
public:
    explicit WindowsComponentExecutor(platform::IWindowsComponentBackend& backend)
        : backend_(&backend) {}

    [[nodiscard]] WindowsComponentCaptureResult capture(
        const domain::WindowsComponentTarget& target) const;
    [[nodiscard]] WindowsComponentExecutionResult compareBefore(
        const domain::WindowsComponentSnapshot& snapshot,
        const QByteArray& expectedFingerprint) const;
    [[nodiscard]] WindowsComponentExecutionResult apply(
        const domain::SetWindowsComponentStateOperation& operation) const;
    [[nodiscard]] WindowsComponentExecutionResult restore(
        const domain::WindowsComponentSnapshot& snapshot) const;
    [[nodiscard]] static QByteArray fingerprint(
        const domain::WindowsComponentSnapshot& snapshot);

private:
    platform::IWindowsComponentBackend* backend_{};
};

} // namespace tweakopedia::execution
