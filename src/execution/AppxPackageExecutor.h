#pragma once

#include "platform/WindowsAppxPackageProvider.h"

#include <QByteArray>
#include <QJsonObject>

namespace tweakopedia::execution {

struct AppxPackageSnapshot {
    QString packageName;
    QVector<platform::AppxPackageIdentity> packages;

    [[nodiscard]] QByteArray fingerprint() const;
    [[nodiscard]] QJsonObject toJson() const;
};

struct AppxExecutionResult {
    bool success{};
    QString code;
    QString message;
    AppxPackageSnapshot snapshot;
};

class AppxPackageExecutor final
{
public:
    explicit AppxPackageExecutor(platform::IAppxPackageBackend& backend);

    [[nodiscard]] AppxExecutionResult capture(QStringView packageName) const;
    [[nodiscard]] AppxExecutionResult compareBefore(
        const AppxPackageSnapshot& snapshot,
        const QByteArray& expectedFingerprint) const;
    [[nodiscard]] AppxExecutionResult apply(const AppxPackageSnapshot& snapshot);

private:
    platform::IAppxPackageBackend* backend_{};
};

} // namespace tweakopedia::execution
