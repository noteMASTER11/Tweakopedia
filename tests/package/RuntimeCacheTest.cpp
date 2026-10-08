#include "package/PackageFooter.h"
#include "package/PayloadManifest.h"
#include "package/RuntimeCache.h"
#include "package/RuntimeLease.h"
#include "package/Sha256.h"
#include "package/ZipPayload.h"

#include <QDir>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

#include <windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

using namespace tweakopedia;

namespace {

void writeFile(const std::filesystem::path& path, std::string_view contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
}

std::string readText(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::filesystem::path createContainer(const std::filesystem::path& base)
{
    const auto source = base / "source";
    writeFile(source / "Tweakopedia.App.exe", "app");
    writeFile(source / "Tweakopedia.Executor.exe", "executor");
    writeFile(source / "content/categories.yaml", "categories: []\n");
    const auto manifest = package::PayloadManifest::fromDirectory(source);
    Q_ASSERT(manifest.ok());
    const auto zip = base / "payload.zip";
    Q_ASSERT(package::ZipPayload::create(source, zip, *manifest.value).ok());

    const auto container = base / "Tweakopedia.exe";
    writeFile(container, "MZ-bootstrap");
    const auto payloadOffset = std::filesystem::file_size(container);
    std::ifstream input(zip, std::ios::binary);
    std::ofstream output(container, std::ios::binary | std::ios::app);
    output << input.rdbuf();
    output.close();
    const auto payloadSize = std::filesystem::file_size(zip);
    const auto digest = package::sha256File(container, payloadOffset, payloadSize);
    Q_ASSERT(digest.ok());
    const package::PackageFooter footer{
        .payloadOffset = payloadOffset,
        .payloadSize = payloadSize,
        .payloadDigest = *digest.value,
    };
    const auto encoded = footer.encode();
    std::ofstream footerOutput(container, std::ios::binary | std::ios::app);
    footerOutput.write(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    return container;
}

std::size_t stagingCount(const std::filesystem::path& runtimeRoot)
{
    std::size_t count{};
    if (!std::filesystem::exists(runtimeRoot)) return 0;
    for (const auto& item : std::filesystem::directory_iterator(runtimeRoot)) {
        const auto name = item.path().filename().wstring();
        if (name.starts_with(L".staging-") || name.starts_with(L".invalid-")) ++count;
    }
    return count;
}

std::string differentDigest(char value)
{
    return std::string(64, value);
}

}

class RuntimeCacheTest final : public QObject
{
    Q_OBJECT

private slots:
    void preparesReusesAndRecoversRuntime()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto base = std::filesystem::path(directory.path().toStdWString());
        const auto container = createContainer(base);
        const auto info = package::inspectContainer(container);
        QVERIFY(info.ok());
        const auto runtimeRoot = base / L"Кэш с пробелом" / "Runtime";
        package::RuntimeCache cache(runtimeRoot);

        auto first = cache.prepare(container, *info.value);
        QVERIFY(first.ok());
        QVERIFY(!first.value->reused);
        QVERIFY(first.value->lease.valid());
        QVERIFY(std::filesystem::exists(first.value->root / "Tweakopedia.App.exe"));
        const auto marker = first.value->root / ".ready.json";
        const auto oldTime = std::filesystem::file_time_type::clock::now()
            - std::chrono::hours(24);
        std::filesystem::last_write_time(marker, oldTime);
        const auto persistedMarkerTime = std::filesystem::last_write_time(marker);

        auto second = cache.prepare(container, *info.value);
        QVERIFY(second.ok());
        QVERIFY(second.value->reused);
        QCOMPARE(std::filesystem::last_write_time(marker), persistedMarkerTime);

        writeFile(runtimeRoot / ".staging-999-stale" / "partial", "x");
        writeFile(second.value->root / "Tweakopedia.App.exe", "bad");
        writeFile(marker, "{\"schema_version\":1,\"payload_sha256\":\""
            + differentDigest('0') + "\"}");
        first.value.reset();
        second.value.reset();

        auto repaired = cache.prepare(container, *info.value);
        QVERIFY(repaired.ok());
        QVERIFY(!repaired.value->reused);
        QCOMPARE(readText(repaired.value->root / "Tweakopedia.App.exe"), std::string("app"));
        QCOMPARE(stagingCount(runtimeRoot), 0);

        repaired.value.reset();
        std::filesystem::remove(runtimeRoot / package::digestToHex(info.value->footer.payloadDigest)
                                / "Tweakopedia.Executor.exe");
        auto restored = cache.prepare(container, *info.value);
        QVERIFY(restored.ok());
        QVERIFY(!restored.value->reused);
        QVERIFY(std::filesystem::exists(restored.value->root / "Tweakopedia.Executor.exe"));
    }

    void serializesTwoConcurrentProcesses()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto base = std::filesystem::path(directory.path().toStdWString());
        const auto container = createContainer(base);
        const auto runtimeRoot = base / L"Параллельный кэш";
        const QStringList arguments{
            QString::fromStdWString(container.wstring()),
            QString::fromStdWString(runtimeRoot.wstring()),
        };
        QProcess first;
        QProcess second;
        first.start(QStringLiteral(TWEAKOPEDIA_RUNTIME_CACHE_PROBE), arguments);
        second.start(QStringLiteral(TWEAKOPEDIA_RUNTIME_CACHE_PROBE), arguments);
        QVERIFY(first.waitForFinished(10000));
        QVERIFY(second.waitForFinished(10000));
        QCOMPARE(first.exitCode(), 0);
        QCOMPARE(second.exitCode(), 0);
        QCOMPARE(stagingCount(runtimeRoot), 0);
        std::size_t digestDirectories{};
        for (const auto& item : std::filesystem::directory_iterator(runtimeRoot))
            if (item.is_directory() && item.path().filename().wstring().size() == 64) ++digestDirectories;
        QCOMPARE(digestDirectories, 1);
    }

    void retriesQuarantineWhileRuntimeFileIsTemporarilyLocked()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto base = std::filesystem::path(directory.path().toStdWString());
        const auto container = createContainer(base);
        const auto info = package::inspectContainer(container);
        QVERIFY(info.ok());
        const auto runtimeRoot = base / "Runtime";
        package::RuntimeCache cache(runtimeRoot);

        auto prepared = cache.prepare(container, *info.value);
        QVERIFY(prepared.ok());
        const auto runtime = prepared.value->root;
        const auto lockedFile = runtime / "Tweakopedia.App.exe";
        prepared.value.reset();
        writeFile(lockedFile, "corrupted");

        const auto handle = CreateFileW(
            lockedFile.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        QVERIFY(handle != INVALID_HANDLE_VALUE);
        std::thread unlocker([handle] {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            CloseHandle(handle);
        });

        const auto repaired = cache.prepare(container, *info.value);
        unlocker.join();
        QVERIFY2(repaired.ok(), repaired.error.message.c_str());
        QCOMPARE(readText(repaired.value->root / "Tweakopedia.App.exe"), std::string("app"));
        QCOMPARE(stagingCount(runtimeRoot), 0);
    }

    void retriesValidationWhileValidRuntimeFileIsTemporarilyLocked()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto base = std::filesystem::path(directory.path().toStdWString());
        const auto container = createContainer(base);
        const auto info = package::inspectContainer(container);
        QVERIFY(info.ok());
        const auto runtimeRoot = base / "Runtime";
        package::RuntimeCache cache(runtimeRoot);

        auto prepared = cache.prepare(container, *info.value);
        QVERIFY(prepared.ok());
        const auto lockedFile = prepared.value->root / "Tweakopedia.App.exe";
        prepared.value.reset();

        const auto handle = CreateFileW(
            lockedFile.c_str(), GENERIC_READ, 0,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        QVERIFY(handle != INVALID_HANDLE_VALUE);
        std::thread unlocker([handle] {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            CloseHandle(handle);
        });

        const auto reused = cache.prepare(container, *info.value);
        unlocker.join();
        QVERIFY2(reused.ok(), reused.error.message.c_str());
        QVERIFY(reused.value->reused);
        QCOMPARE(readText(reused.value->root / "Tweakopedia.App.exe"), std::string("app"));
        QCOMPARE(stagingCount(runtimeRoot), 0);
    }

    void cleansOnlyFreeOldDigestRuntimes()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto runtimeRoot = std::filesystem::path(directory.path().toStdWString()) / "Runtime";
        const auto current = differentDigest('a');
        const auto freeOld = runtimeRoot / std::filesystem::path(differentDigest('b'));
        const auto leasedOld = runtimeRoot / std::filesystem::path(differentDigest('c'));
        writeFile(runtimeRoot / std::filesystem::path(current) / ".runtime.lock", "");
        writeFile(freeOld / ".runtime.lock", "");
        writeFile(leasedOld / ".runtime.lock", "");
        writeFile(runtimeRoot / "not-a-runtime" / "keep", "x");
        auto lease = package::RuntimeLease::acquireShared(leasedOld);
        QVERIFY(lease.ok());

        package::RuntimeCache cache(runtimeRoot);
        const auto report = cache.cleanupOld(current);
        QVERIFY(!std::filesystem::exists(freeOld));
        QVERIFY(std::filesystem::exists(leasedOld));
        QVERIFY(std::filesystem::exists(runtimeRoot / std::filesystem::path(current)));
        QVERIFY(std::filesystem::exists(runtimeRoot / "not-a-runtime"));
        QCOMPARE(report.removed.size(), 1);
        QCOMPARE(report.retained.size(), 1);

        lease.value.reset();
        cache.cleanupOld(current);
        QVERIFY(!std::filesystem::exists(leasedOld));
    }

    void reportsSimulatedFilesystemFailures_data()
    {
        QTest::addColumn<int>("fault");
        QTest::newRow("create") << static_cast<int>(package::RuntimeCacheFault::CreateStaging);
        QTest::newRow("marker") << static_cast<int>(package::RuntimeCacheFault::WriteMarker);
        QTest::newRow("publish") << static_cast<int>(package::RuntimeCacheFault::Publish);
    }

    void reportsSimulatedFilesystemFailures()
    {
        QFETCH(int, fault);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto base = std::filesystem::path(directory.path().toStdWString());
        const auto container = createContainer(base);
        const auto info = package::inspectContainer(container);
        QVERIFY(info.ok());
        const auto runtimeRoot = base / "Runtime";
        package::RuntimeCache cache(
            runtimeRoot, static_cast<package::RuntimeCacheFault>(fault));
        QVERIFY(!cache.prepare(container, *info.value).ok());
        QCOMPARE(stagingCount(runtimeRoot), 0);
    }
};

QTEST_MAIN(RuntimeCacheTest)
#include "RuntimeCacheTest.moc"
