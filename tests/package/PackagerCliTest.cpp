#include "package/PackageFooter.h"
#include "package/ZipPayload.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
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

void createPayload(const std::filesystem::path& root)
{
    writeFile(root / "Tweakopedia.App.exe", "gui");
    writeFile(root / "Tweakopedia.Executor.exe", "executor");
    writeFile(root / "content/categories.yaml", "categories: []\n");
    writeFile(root / "content/tweaks/filesystem/example.yaml", "id: example\n");
    writeFile(root / "licenses/Qt-LGPLv3.txt", "Qt license");
    writeFile(root / "licenses/THIRD-PARTY-NOTICES.txt", "notices");
}

struct ProcessResult {
    int exitCode{};
    QByteArray standardOutput;
    QByteArray standardError;
};

ProcessResult runPackager(const QStringList& arguments)
{
    QProcess process;
    process.setProgram(QStringLiteral(TWEAKOPEDIA_TEST_PACKAGER));
    process.setArguments(arguments);
    process.start();
    if (!process.waitForFinished(15000)) return {-999, {}, process.errorString().toUtf8()};
    return {process.exitCode(), process.readAllStandardOutput(), process.readAllStandardError()};
}

}

class PackagerCliTest final : public QObject
{
    Q_OBJECT

private slots:
    void createsDeterministicContainerAndInspectsManifest()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto base = std::filesystem::path(directory.path().toStdWString());
        const auto bootstrap = base / "bootstrap.exe";
        const auto payload = base / L"payload с пробелом";
        writeFile(bootstrap, "MZ-native-bootstrap");
        createPayload(payload);
        std::filesystem::create_directories(base / "one");
        std::filesystem::create_directories(base / "two");
        const auto first = base / "one/Tweakopedia.exe";
        const auto second = base / "two/Tweakopedia.exe";

        for (const auto& output : {first, second}) {
            const auto result = runPackager({QStringLiteral("create"),
                QStringLiteral("--bootstrap"), QString::fromStdWString(bootstrap.wstring()),
                QStringLiteral("--payload-root"), QString::fromStdWString(payload.wstring()),
                QStringLiteral("--output"), QString::fromStdWString(output.wstring())});
            QVERIFY2(result.exitCode == 0, result.standardError.constData());
        }
        const auto firstBytes = QByteArray::fromRawData(nullptr, 0);
        Q_UNUSED(firstBytes);
        std::ifstream a(first, std::ios::binary), b(second, std::ios::binary);
        const std::string aBytes{std::istreambuf_iterator<char>(a), {}};
        const std::string bBytes{std::istreambuf_iterator<char>(b), {}};
        QCOMPARE(aBytes, bBytes);

        const auto info = package::inspectContainer(first);
        QVERIFY(info.ok());
        const auto manifest = package::ZipPayload::readManifest(first, *info.value);
        QVERIFY(manifest.ok());
        QVERIFY(manifest.value->find("Tweakopedia.App.exe"));
        QVERIFY(manifest.value->find("Tweakopedia.Executor.exe"));
        QVERIFY(manifest.value->find("content/categories.yaml"));
        QVERIFY(manifest.value->find("licenses/Qt-LGPLv3.txt"));
        QVERIFY(manifest.value->find("licenses/THIRD-PARTY-NOTICES.txt"));

        const auto inspected = runPackager({QStringLiteral("inspect"),
            QStringLiteral("--container"), QString::fromStdWString(first.wstring()),
            QStringLiteral("--json")});
        QCOMPARE(inspected.exitCode, 0);
        const auto json = QJsonDocument::fromJson(inspected.standardOutput).object();
        QCOMPARE(json.value(QStringLiteral("schema_version")).toInt(), 1);
        QCOMPARE(json.value(QStringLiteral("payload_sha256")).toString().size(), 64);
        QVERIFY(json.value(QStringLiteral("files")).toArray().size() >= 6);
    }

    void rejectsInvalidOrDestructiveOutputs()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto base = std::filesystem::path(directory.path().toStdWString());
        const auto bootstrap = base / "bootstrap.exe";
        const auto payload = base / "payload";
        writeFile(bootstrap, "MZ-native-bootstrap");
        createPayload(payload);

        auto create = [&](const std::filesystem::path& output) {
            return runPackager({QStringLiteral("create"),
                QStringLiteral("--bootstrap"), QString::fromStdWString(bootstrap.wstring()),
                QStringLiteral("--payload-root"), QString::fromStdWString(payload.wstring()),
                QStringLiteral("--output"), QString::fromStdWString(output.wstring())}).exitCode;
        };
        QVERIFY(create(bootstrap) != 0);
        QVERIFY(create(payload / "Tweakopedia.exe") != 0);
        QVERIFY(create(base / "wrong-name.exe") != 0);

        const auto existing = base / "Tweakopedia.exe";
        writeFile(existing, "keep");
        QVERIFY(create(existing) != 0);
        std::ifstream stream(existing, std::ios::binary);
        const std::string contents{std::istreambuf_iterator<char>(stream), {}};
        QCOMPARE(contents, std::string("keep"));
    }

    void requiresLicensePayloadFiles()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto base = std::filesystem::path(directory.path().toStdWString());
        const auto bootstrap = base / "bootstrap.exe";
        const auto payload = base / "payload";
        writeFile(bootstrap, "MZ-native-bootstrap");
        createPayload(payload);
        std::filesystem::remove(payload / "licenses/Qt-LGPLv3.txt");

        const auto result = runPackager({QStringLiteral("create"),
            QStringLiteral("--bootstrap"), QString::fromStdWString(bootstrap.wstring()),
            QStringLiteral("--payload-root"), QString::fromStdWString(payload.wstring()),
            QStringLiteral("--output"), QString::fromStdWString((base / "Tweakopedia.exe").wstring())});
        QVERIFY(result.exitCode != 0);
    }
};

QTEST_APPLESS_MAIN(PackagerCliTest)
#include "PackagerCliTest.moc"
