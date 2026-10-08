#include "package/LegacyDataMigrator.h"

#include <QTemporaryDir>
#include <QTest>

#include <filesystem>
#include <fstream>
#include <string_view>

using namespace tweakopedia;

namespace {

void writeFile(const std::filesystem::path& path, std::string_view contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << contents;
}

}

class LegacyDataMigratorTest final : public QObject
{
    Q_OBJECT

private slots:
    void completesEmptyLegacyRootAndWritesMarker()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto product = std::filesystem::path(directory.path().toStdWString()) / "Tweakopedia";
        const auto data = product / "Data";

        const auto report = package::LegacyDataMigrator::migrate(product, data);
        QVERIFY(report.complete);
        QVERIFY(report.moved.empty());
        QVERIFY(report.conflicts.empty());
        QVERIFY(std::filesystem::exists(data / ".layout-v1-complete"));
    }

    void movesDatabaseLogsAndTransactions()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto product = std::filesystem::path(directory.path().toStdWString()) / "Tweakopedia";
        const auto data = product / "Data";
        writeFile(product / "tweakopedia.db", "database");
        writeFile(product / "logs/app.log", "log");
        writeFile(product / "transactions/one/result.json", "result");

        const auto report = package::LegacyDataMigrator::migrate(product, data);
        QVERIFY(report.complete);
        QCOMPARE(report.moved.size(), 3);
        QVERIFY(std::filesystem::exists(data / "tweakopedia.db"));
        QVERIFY(std::filesystem::exists(data / "logs/app.log"));
        QVERIFY(std::filesystem::exists(data / "transactions/one/result.json"));
        QVERIFY(!std::filesystem::exists(product / "tweakopedia.db"));
        QVERIFY(!std::filesystem::exists(product / "logs"));
        QVERIFY(!std::filesystem::exists(product / "transactions"));
        QVERIFY(std::filesystem::exists(data / ".layout-v1-complete"));
    }

    void resumesInterruptedPartialMigration()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto product = std::filesystem::path(directory.path().toStdWString()) / "Tweakopedia";
        const auto data = product / "Data";
        writeFile(data / "tweakopedia.db", "already moved");
        writeFile(product / "logs/app.log", "pending log");
        writeFile(product / "transactions/one/result.json", "pending transaction");

        const auto report = package::LegacyDataMigrator::migrate(product, data);
        QVERIFY(report.complete);
        QCOMPARE(report.moved.size(), 2);
        QVERIFY(report.conflicts.empty());
        QVERIFY(std::filesystem::exists(data / "logs/app.log"));
        QVERIFY(std::filesystem::exists(data / "transactions/one/result.json"));
        QVERIFY(std::filesystem::exists(data / ".layout-v1-complete"));
    }

    void neverOverwritesDestinationConflict()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto product = std::filesystem::path(directory.path().toStdWString()) / "Tweakopedia";
        const auto data = product / "Data";
        writeFile(product / "tweakopedia.db", "legacy");
        writeFile(data / "tweakopedia.db", "current");

        const auto report = package::LegacyDataMigrator::migrate(product, data);
        QVERIFY(!report.complete);
        QCOMPARE(report.conflicts.size(), 1);
        QVERIFY(std::filesystem::exists(product / "tweakopedia.db"));
        std::ifstream current(data / "tweakopedia.db");
        std::string contents;
        current >> contents;
        QCOMPARE(contents, std::string("current"));
        QVERIFY(!std::filesystem::exists(data / ".layout-v1-complete"));
    }
};

QTEST_MAIN(LegacyDataMigratorTest)
#include "LegacyDataMigratorTest.moc"
