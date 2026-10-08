#include "app/SystemOverviewPresenter.h"

#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

class SystemOverviewPresenterTest final : public QObject
{
    Q_OBJECT

private slots:
    void selectsWindowsLogoFromBuild()
    {
        domain::SystemOverviewSnapshot snapshot;
        snapshot.os.buildNumber = 19045;
        QCOMPARE(app::SystemOverviewPresenter::present(snapshot).value(u"logo"_s), u"windows10"_s);

        snapshot.os.buildNumber = 22631;
        QCOMPARE(app::SystemOverviewPresenter::present(snapshot).value(u"logo"_s), u"windows11"_s);

        snapshot.os.buildNumber = 0;
        QCOMPARE(app::SystemOverviewPresenter::present(snapshot).value(u"logo"_s), u"windows"_s);
    }

    void formatsHardwareAndDriveStateForQml()
    {
        domain::SystemOverviewSnapshot snapshot;
        snapshot.displayName = u"Иван"_s;
        snapshot.computerName = u"DESKTOP-TEST"_s;
        snapshot.manufacturer = u"ASUSTeK COMPUTER INC."_s;
        snapshot.model = u"System Product Name"_s;
        snapshot.baseboard = u"ROG STRIX B650E-F"_s;
        snapshot.biosVersion = u"3202"_s;
        snapshot.biosMode = u"UEFI"_s;
        snapshot.uptimeSeconds = 3 * 3600 + 42 * 60;
        snapshot.os.caption = u"Windows 11 Pro"_s;
        snapshot.os.version = u"10.0.26200"_s;
        snapshot.os.buildNumber = 26200;
        snapshot.os.architecture = u"x64"_s;
        snapshot.processor = {
            .name = u"AMD Ryzen 9 7950X"_s,
            .coreCount = 16,
            .logicalProcessorCount = 32,
            .currentClockMHz = 4500,
        };
        snapshot.memory = {
            .totalBytes = 64ULL * 1024 * 1024 * 1024,
            .availableBytes = 48ULL * 1024 * 1024 * 1024,
            .speedMTs = 6000,
            .moduleCount = 2,
        };
        snapshot.graphics = {{
            .name = u"NVIDIA GeForce RTX 4090"_s,
            .adapterRamBytes = 24ULL * 1024 * 1024 * 1024,
            .sharedSystemMemoryBytes = 16ULL * 1024 * 1024 * 1024,
            .driverVersion = u"591.12"_s,
            .vendorId = 0x10DE,
            .deviceId = 0x2D04,
        }};
        snapshot.disks = {{
            .name = u"Samsung SSD 990 PRO"_s,
            .sizeBytes = 2ULL * 1000 * 1000 * 1000 * 1000,
            .mediaType = u"SSD"_s,
            .busType = u"NVMe"_s,
            .health = domain::DiskHealth::Healthy,
        }, {
            .name = u"Archive"_s,
            .sizeBytes = 4ULL * 1000 * 1000 * 1000 * 1000,
            .mediaType = u"HDD"_s,
            .busType = u"SATA"_s,
            .health = domain::DiskHealth::Warning,
        }};

        const auto result = app::SystemOverviewPresenter::present(snapshot);
        QCOMPARE(result.value(u"greetingName"_s), u"Иван"_s);
        QCOMPARE(result.value(u"computerName"_s), u"DESKTOP-TEST"_s);
        QCOMPARE(result.value(u"uptime"_s), u"3 ч 42 мин"_s);
        QCOMPARE(result.value(u"osSummary"_s), u"10.0.26200 · сборка 26200 · x64"_s);
        QCOMPARE(result.value(u"processor"_s).toMap().value(u"details"_s),
                 u"16 ядер · 32 потока · 4,50 ГГц"_s);
        QCOMPARE(result.value(u"memory"_s).toMap().value(u"title"_s), u"64 ГБ"_s);
        QCOMPARE(result.value(u"memory"_s).toMap().value(u"usedPercent"_s), 25);

        const auto graphics = result.value(u"graphics"_s).toList();
        QCOMPARE(graphics.size(), 1);
        QCOMPARE(graphics.first().toMap().value(u"details"_s),
                 u"24 ГБ выделено · 16 ГБ разделяемой · драйвер 591.12"_s);
        QCOMPARE(graphics.first().toMap().value(u"technical"_s), u"PCI 10DE:2D04"_s);

        const auto disks = result.value(u"disks"_s).toList();
        QCOMPARE(disks.size(), 2);
        QCOMPARE(disks.at(0).toMap().value(u"details"_s), u"2 ТБ · SSD · NVMe"_s);
        QCOMPARE(disks.at(0).toMap().value(u"healthText"_s), u"Исправен"_s);
        QCOMPARE(disks.at(0).toMap().value(u"healthTone"_s), u"good"_s);
        QCOMPARE(disks.at(1).toMap().value(u"healthText"_s), u"Требует внимания"_s);
        QCOMPARE(disks.at(1).toMap().value(u"healthTone"_s), u"warning"_s);
    }
};

QTEST_APPLESS_MAIN(SystemOverviewPresenterTest)

#include "SystemOverviewPresenterTest.moc"
