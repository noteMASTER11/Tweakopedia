#include "platform/WindowsLocalVolumeProvider.h"

#include <QtTest/QTest>

using namespace Qt::StringLiterals;
using namespace tweakopedia::platform;

namespace {

class FakeWindowsVolumeApi final : public IWindowsVolumeApi
{
public:
    QStringList logicalDriveRoots() const override { return roots; }
    std::optional<WindowsVolumeSnapshot> query(const QString& rootPath) const override
    {
        const auto iterator = snapshots.constFind(rootPath);
        return iterator == snapshots.constEnd()
            ? std::nullopt : std::optional<WindowsVolumeSnapshot>(*iterator);
    }
    QString systemDrive() const override { return system; }

    QStringList roots;
    QHash<QString, WindowsVolumeSnapshot> snapshots;
    QString system{u"C:"_s};
};

WindowsVolumeSnapshot volume(
    QString root,
    QString fileSystem,
    LocalVolumeDriveType type,
    quint64 total,
    quint64 free,
    QString label = {})
{
    return {.rootPath = std::move(root),
            .label = std::move(label),
            .fileSystem = std::move(fileSystem),
            .totalBytes = total,
            .freeBytes = free,
            .driveType = type};
}

} // namespace

class WindowsLocalVolumeProviderTest final : public QObject
{
    Q_OBJECT

private slots:
    void returnsReadyLocalNtfsVolumesWithSystemVolumeFirst()
    {
        FakeWindowsVolumeApi api;
        api.roots = {u"d:\\"_s, u"C:\\"_s, u"E:\\"_s,
                     u"Z:\\"_s, u"F:\\"_s, u"invalid"_s};
        api.snapshots.insert(u"d:\\"_s,
                             volume(u"d:\\"_s, u"NTFS"_s,
                                    LocalVolumeDriveType::Fixed,
                                    2'000, 800, u"Data"_s));
        api.snapshots.insert(u"C:\\"_s,
                             volume(u"C:\\"_s, u"ntfs"_s,
                                    LocalVolumeDriveType::Fixed,
                                    1'000, 250, u"Windows"_s));
        api.snapshots.insert(u"E:\\"_s,
                             volume(u"E:\\"_s, u"FAT32"_s,
                                    LocalVolumeDriveType::Removable, 500, 200));
        api.snapshots.insert(u"Z:\\"_s,
                             volume(u"Z:\\"_s, u"NTFS"_s,
                                    LocalVolumeDriveType::Remote, 3'000, 1'000));
        api.snapshots.insert(u"F:\\"_s,
                             volume(u"F:\\"_s, u"NTFS"_s,
                                    LocalVolumeDriveType::Removable, 700, 300));
        api.snapshots.insert(u"invalid"_s,
                             volume(u"invalid"_s, u"NTFS"_s,
                                    LocalVolumeDriveType::Fixed, 900, 400));

        const WindowsLocalVolumeProvider provider(api);
        const auto volumes = provider.volumes();

        QCOMPARE(volumes.size(), 3);
        QCOMPARE(volumes.at(0).rootPath, u"C:\\"_s);
        QCOMPARE(volumes.at(0).target, u"C:"_s);
        QCOMPARE(volumes.at(0).label, u"Windows"_s);
        QCOMPARE(volumes.at(0).fileSystem, u"NTFS"_s);
        QCOMPARE(volumes.at(0).totalBytes, 1'000);
        QCOMPARE(volumes.at(0).freeBytes, 250);
        QVERIFY(volumes.at(0).systemVolume);
        QCOMPARE(volumes.at(1).target, u"D:"_s);
        QCOMPARE(volumes.at(2).target, u"F:"_s);
        QVERIFY(!volumes.at(1).systemVolume);
    }

    void ignoresUnavailableVolumes()
    {
        FakeWindowsVolumeApi api;
        api.roots = {u"C:\\"_s};
        const WindowsLocalVolumeProvider provider(api);
        QVERIFY(provider.volumes().isEmpty());
    }
};

QTEST_GUILESS_MAIN(WindowsLocalVolumeProviderTest)
#include "WindowsLocalVolumeProviderTest.moc"

