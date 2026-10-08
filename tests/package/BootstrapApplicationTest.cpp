#include "bootstrap/BootstrapApplication.h"
#include "bootstrap/WindowsBootstrapPlatform.h"
#include "package/PackageFooter.h"
#include "package/PayloadManifest.h"
#include "package/Sha256.h"
#include "package/ZipPayload.h"

#include <QTemporaryDir>
#include <QTest>

#include <filesystem>
#include <fstream>

using namespace tweakopedia;

namespace {

void writeFile(const std::filesystem::path& path, std::string_view contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
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

class FakePlatform final : public bootstrap::IBootstrapPlatform
{
public:
    std::filesystem::path localAppData;
    package::Result<std::uint32_t> launchResult = package::Result<std::uint32_t>::success(0);
    int launchCount{};
    int errorCount{};
    std::filesystem::path executable;
    std::filesystem::path workingDirectory;
    std::vector<std::wstring> arguments;
    std::wstring errorMessage;

    std::filesystem::path localAppDataPath() const override { return localAppData; }
    void showError(std::wstring_view, std::wstring_view message) override
    {
        ++errorCount;
        errorMessage = message;
    }
    package::Result<std::uint32_t> launchAndWait(
        const std::filesystem::path& exe,
        std::span<const std::wstring> args,
        const std::filesystem::path& cwd) override
    {
        ++launchCount;
        executable = exe;
        arguments.assign(args.begin(), args.end());
        workingDirectory = cwd;
        return launchResult;
    }
};

}

class BootstrapApplicationTest final : public QObject
{
    Q_OBJECT

private slots:
    void reportsInvalidContainer()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = std::filesystem::path(directory.path().toStdWString());
        const auto invalid = root / "invalid.exe";
        writeFile(invalid, "not a container");
        FakePlatform platform;
        platform.localAppData = root / "Local";

        bootstrap::BootstrapApplication app(platform);
        QCOMPARE(app.run(invalid, {}), bootstrap::BootstrapApplication::inspectFailure);
        QCOMPARE(platform.errorCount, 1);
        QCOMPARE(platform.launchCount, 0);
    }

    void reportsPrepareFailure()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = std::filesystem::path(directory.path().toStdWString());
        const auto container = createContainer(root);
        FakePlatform platform;
        platform.localAppData = root / "Local";
        writeFile(platform.localAppData / "Tweakopedia/Runtime", "x");

        bootstrap::BootstrapApplication app(platform);
        QCOMPARE(app.run(container, {}), bootstrap::BootstrapApplication::prepareFailure);
        QCOMPARE(platform.launchCount, 0);
        QCOMPARE(platform.errorCount, 1);
    }

    void refusesIncompleteLegacyMigration()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = std::filesystem::path(directory.path().toStdWString());
        const auto container = createContainer(root / "package");
        FakePlatform platform;
        platform.localAppData = root / "Local";
        const auto product = platform.localAppData / "Tweakopedia";
        writeFile(product / "tweakopedia.db", "legacy");
        writeFile(product / "Data/tweakopedia.db", "current");

        bootstrap::BootstrapApplication app(platform);
        QCOMPARE(app.run(container, {}), bootstrap::BootstrapApplication::migrationFailure);
        QCOMPARE(platform.launchCount, 0);
        QVERIFY(std::filesystem::exists(product / "tweakopedia.db"));
    }

    void forwardsArgumentsPropagatesExitAndCleansOldRuntime()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = std::filesystem::path(directory.path().toStdWString());
        const auto container = createContainer(root / L"пакет с пробелом");
        FakePlatform platform;
        platform.localAppData = root / L"Локальные данные";
        platform.launchResult = package::Result<std::uint32_t>::success(37);
        const auto oldRuntime = platform.localAppData / "Tweakopedia/Runtime"
            / std::filesystem::path(std::string(64, 'a'));
        writeFile(oldRuntime / ".runtime.lock", "");
        const std::vector<std::wstring> incoming{L"--name", L"значение с \"кавычкой\""};

        bootstrap::BootstrapApplication app(platform);
        QCOMPARE(app.run(container, incoming), 37);
        QCOMPARE(platform.launchCount, 1);
        QVERIFY(platform.executable == platform.workingDirectory / "Tweakopedia.App.exe");
        const std::vector<std::wstring> expected{
            incoming[0], incoming[1], L"--runtime-root", platform.workingDirectory.wstring(),
            L"--container-path", container.wstring()};
        QVERIFY(platform.arguments == expected);
        QVERIFY(!std::filesystem::exists(oldRuntime));
    }

    void doesNotCleanWhenProcessCreationFails()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = std::filesystem::path(directory.path().toStdWString());
        const auto container = createContainer(root / "package");
        FakePlatform platform;
        platform.localAppData = root / "Local";
        platform.launchResult = package::Result<std::uint32_t>::failure(
            package::ErrorCode::Io, "launch failed");
        const auto oldRuntime = platform.localAppData / "Tweakopedia/Runtime"
            / std::filesystem::path(std::string(64, 'b'));
        writeFile(oldRuntime / ".runtime.lock", "");

        bootstrap::BootstrapApplication app(platform);
        QCOMPARE(app.run(container, {}), bootstrap::BootstrapApplication::launchFailure);
        QVERIFY(std::filesystem::exists(oldRuntime));
        QCOMPARE(platform.errorCount, 1);
    }

    void windowsPlatformPreservesWideArguments()
    {
        bootstrap::WindowsBootstrapPlatform platform;
        const std::vector<std::wstring> arguments{
            L"обычный", L"два слова", L"кавычка \" внутри", L"хвост\\"};
        const auto result = platform.launchAndWait(
            std::filesystem::path(QStringLiteral(TWEAKOPEDIA_BOOTSTRAP_ARGUMENT_PROBE).toStdWString()),
            arguments,
            std::filesystem::current_path());
        QVERIFY2(result.ok(), result.error.message.c_str());
        QCOMPARE(*result.value, std::uint32_t{0});
    }
};

QTEST_MAIN(BootstrapApplicationTest)
#include "BootstrapApplicationTest.moc"
