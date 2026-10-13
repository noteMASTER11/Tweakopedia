#include "platform/WindowsLocalVolumeProvider.h"

#include <QRegularExpression>

#include <windows.h>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace tweakopedia::platform {
namespace {

class NativeWindowsVolumeApi final : public IWindowsVolumeApi
{
public:
    QStringList logicalDriveRoots() const override
    {
        const auto required = GetLogicalDriveStringsW(0, nullptr);
        if (required == 0) return {};
        std::wstring buffer(static_cast<size_t>(required) + 1, L'\0');
        if (GetLogicalDriveStringsW(required, buffer.data()) == 0) return {};

        QStringList roots;
        const wchar_t* cursor = buffer.c_str();
        while (*cursor != L'\0') {
            roots.append(QString::fromWCharArray(cursor));
            cursor += std::wcslen(cursor) + 1;
        }
        return roots;
    }

    std::optional<WindowsVolumeSnapshot> query(const QString& rootPath) const override
    {
        const auto nativeRoot = reinterpret_cast<const wchar_t*>(rootPath.utf16());
        const auto driveType = mapDriveType(GetDriveTypeW(nativeRoot));
        if (driveType == LocalVolumeDriveType::Other) return std::nullopt;

        wchar_t label[MAX_PATH + 1]{};
        wchar_t fileSystem[MAX_PATH + 1]{};
        if (!GetVolumeInformationW(nativeRoot, label, MAX_PATH + 1, nullptr, nullptr,
                                   nullptr, fileSystem, MAX_PATH + 1)) {
            return std::nullopt;
        }

        ULARGE_INTEGER available{};
        ULARGE_INTEGER total{};
        ULARGE_INTEGER free{};
        if (!GetDiskFreeSpaceExW(nativeRoot, &available, &total, &free)) {
            return std::nullopt;
        }
        return WindowsVolumeSnapshot{
            .rootPath = rootPath,
            .label = QString::fromWCharArray(label),
            .fileSystem = QString::fromWCharArray(fileSystem),
            .totalBytes = total.QuadPart,
            .freeBytes = free.QuadPart,
            .driveType = driveType,
        };
    }

    QString systemDrive() const override
    {
        std::wstring buffer(MAX_PATH + 1, L'\0');
        const auto length = GetWindowsDirectoryW(buffer.data(), MAX_PATH + 1);
        if (length < 2 || length > MAX_PATH) return {};
        return QString::fromWCharArray(buffer.data(), 2).toUpper();
    }

private:
    static LocalVolumeDriveType mapDriveType(UINT value)
    {
        switch (value) {
        case DRIVE_FIXED: return LocalVolumeDriveType::Fixed;
        case DRIVE_REMOVABLE: return LocalVolumeDriveType::Removable;
        case DRIVE_REMOTE: return LocalVolumeDriveType::Remote;
        case DRIVE_CDROM: return LocalVolumeDriveType::Optical;
        default: return LocalVolumeDriveType::Other;
        }
    }
};

} // namespace

WindowsLocalVolumeProvider::WindowsLocalVolumeProvider()
    : ownedApi_(std::make_unique<NativeWindowsVolumeApi>()), api_(ownedApi_.get())
{
}

WindowsLocalVolumeProvider::WindowsLocalVolumeProvider(IWindowsVolumeApi& api)
    : api_(&api)
{
}

WindowsLocalVolumeProvider::~WindowsLocalVolumeProvider() = default;

QVector<LocalVolume> WindowsLocalVolumeProvider::volumes() const
{
    QVector<LocalVolume> result;
    if (!api_) return result;

    static const QRegularExpression driveRoot(u"^[A-Za-z]:\\\\$"_s);
    const auto systemDrive = api_->systemDrive().toUpper();
    for (const auto& root : api_->logicalDriveRoots()) {
        if (!driveRoot.match(root).hasMatch()) continue;
        const auto snapshot = api_->query(root);
        if (!snapshot) continue;
        if (snapshot->driveType != LocalVolumeDriveType::Fixed
            && snapshot->driveType != LocalVolumeDriveType::Removable) {
            continue;
        }
        if (snapshot->fileSystem.compare(u"NTFS"_s, Qt::CaseInsensitive) != 0) continue;

        const auto target = root.left(2).toUpper();
        result.append({
            .rootPath = target + u"\\"_s,
            .target = target,
            .label = snapshot->label,
            .fileSystem = u"NTFS"_s,
            .totalBytes = snapshot->totalBytes,
            .freeBytes = snapshot->freeBytes,
            .systemVolume = target == systemDrive,
        });
    }
    std::sort(result.begin(), result.end(), [](const LocalVolume& left, const LocalVolume& right) {
        if (left.systemVolume != right.systemVolume) return left.systemVolume;
        return left.target.compare(right.target, Qt::CaseInsensitive) < 0;
    });
    return result;
}

} // namespace tweakopedia::platform

