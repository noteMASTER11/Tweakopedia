#pragma once

#include <QSet>
#include <QString>
#include <QVector>

namespace tweakopedia::platform {

struct AppxPackageIdentity {
    QString name;
    QString fullName;
};

struct AppxPackageQueryResult {
    QVector<AppxPackageIdentity> packages;
    QString error;

    [[nodiscard]] QSet<QString> packageNames() const;
};

struct AppxPackageMutationResult {
    bool success{};
    QString error;
};

class IAppxPackageBackend
{
public:
    virtual ~IAppxPackageBackend() = default;
    [[nodiscard]] virtual AppxPackageQueryResult installedForCurrentUser() const = 0;
    [[nodiscard]] virtual AppxPackageMutationResult removeCurrentUser(const QString& fullName) = 0;
};

class WindowsAppxPackageProvider final : public IAppxPackageBackend
{
public:
    [[nodiscard]] AppxPackageQueryResult installedForCurrentUser() const override;
    [[nodiscard]] AppxPackageMutationResult removeCurrentUser(const QString& fullName) override;
    [[nodiscard]] static AppxPackageQueryResult parse(const QByteArray& json);
};

} // namespace tweakopedia::platform
