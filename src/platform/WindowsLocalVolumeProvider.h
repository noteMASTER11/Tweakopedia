#pragma once

#include "platform/ILocalVolumeProvider.h"

#include <QHash>
#include <QStringList>

#include <memory>
#include <optional>

namespace tweakopedia::platform {

enum class LocalVolumeDriveType { Fixed, Removable, Remote, Optical, Other };

struct WindowsVolumeSnapshot {
    QString rootPath;
    QString label;
    QString fileSystem;
    quint64 totalBytes{};
    quint64 freeBytes{};
    LocalVolumeDriveType driveType{LocalVolumeDriveType::Other};
};

class IWindowsVolumeApi
{
public:
    virtual ~IWindowsVolumeApi() = default;
    [[nodiscard]] virtual QStringList logicalDriveRoots() const = 0;
    [[nodiscard]] virtual std::optional<WindowsVolumeSnapshot> query(
        const QString& rootPath) const = 0;
    [[nodiscard]] virtual QString systemDrive() const = 0;
};

class WindowsLocalVolumeProvider final : public ILocalVolumeProvider
{
public:
    WindowsLocalVolumeProvider();
    explicit WindowsLocalVolumeProvider(IWindowsVolumeApi& api);
    ~WindowsLocalVolumeProvider() override;

    [[nodiscard]] QVector<LocalVolume> volumes() const override;

private:
    std::unique_ptr<IWindowsVolumeApi> ownedApi_;
    IWindowsVolumeApi* api_{};
};

} // namespace tweakopedia::platform
