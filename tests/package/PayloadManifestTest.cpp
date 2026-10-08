#include "package/PayloadManifest.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <string>
#include <string_view>

using namespace tweakopedia;

namespace {

constexpr std::string_view zeroDigest =
    "0000000000000000000000000000000000000000000000000000000000000000";

std::string entry(std::string_view path, std::string_view digest = zeroDigest,
                  std::string_view size = "1")
{
    return "{\"path\":\"" + std::string(path) + "\",\"size\":" + std::string(size)
        + ",\"sha256\":\"" + std::string(digest) + "\"}";
}

std::string document(std::string files, std::string_view extraRoot = {})
{
    return "{\"schema_version\":1,\"files\":[" + std::move(files) + "]"
        + std::string(extraRoot) + "}";
}

std::string requiredEntries(std::string extra = {})
{
    auto files = entry("content/categories.yaml") + ","
        + entry("Tweakopedia.Executor.exe") + "," + entry("Tweakopedia.App.exe");
    if (!extra.empty()) files += "," + std::move(extra);
    return files;
}

void writeFile(const QString& path, const QByteArray& contents)
{
    QDir{}.mkpath(QFileInfo(path).path());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(contents), contents.size());
}

}

class PayloadManifestTest final : public QObject
{
    Q_OBJECT

private slots:
    void canonicalizesOrderAndLowercaseDigests()
    {
        const auto parsed = package::PayloadManifest::parse(document(requiredEntries(
            entry("plugins/z.dll", "abcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcd", "7"))));
        QVERIFY(parsed.ok());
        const auto json = parsed.value->toCanonicalJson();
        const auto appPosition = json.find("Tweakopedia.App.exe");
        const auto executorPosition = json.find("Tweakopedia.Executor.exe");
        const auto categoryPosition = json.find("content/categories.yaml");
        const auto pluginPosition = json.find("plugins/z.dll");
        QVERIFY(appPosition < executorPosition);
        QVERIFY(executorPosition < categoryPosition);
        QVERIFY(categoryPosition < pluginPosition);
        QVERIFY(json.find("ABCDEF") == std::string::npos);

        const auto roundTrip = package::PayloadManifest::parse(json);
        QVERIFY(roundTrip.ok());
        QCOMPARE(roundTrip.value->toCanonicalJson(), json);
    }

    void rejectsInvalidSchema_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::newRow("unknown-root") << QByteArray::fromStdString(
            document(requiredEntries(), ",\"extra\":true"));
        QTest::newRow("unknown-entry") << QByteArray::fromStdString(document(
            requiredEntries("{\"path\":\"x\",\"size\":1,\"sha256\":\"" +
                std::string(zeroDigest) + "\",\"extra\":0}")));
        QTest::newRow("duplicate") << QByteArray::fromStdString(document(
            requiredEntries(entry("Tweakopedia.App.exe"))));
        QTest::newRow("absolute") << QByteArray::fromStdString(document(requiredEntries(entry("/x"))));
        QTest::newRow("drive") << QByteArray::fromStdString(document(requiredEntries(entry("C:/x"))));
        QTest::newRow("unc") << QByteArray::fromStdString(document(requiredEntries(entry("//server/x"))));
        QTest::newRow("ads") << QByteArray::fromStdString(document(requiredEntries(entry("content/x:y"))));
        QTest::newRow("dot") << QByteArray::fromStdString(document(requiredEntries(entry("content/./x"))));
        QTest::newRow("dotdot") << QByteArray::fromStdString(document(requiredEntries(entry("content/../x"))));
        QTest::newRow("empty-segment") << QByteArray::fromStdString(document(requiredEntries(entry("content//x"))));
        QTest::newRow("backslash") << QByteArray::fromStdString(document(requiredEntries(entry("content\\\\x"))));
        QTest::newRow("manifest-self") << QByteArray::fromStdString(document(requiredEntries(entry("payload-manifest.json"))));
        QTest::newRow("digest-length") << QByteArray::fromStdString(document(requiredEntries(entry("x", std::string(63, '0')))));
        QTest::newRow("digest-uppercase") << QByteArray::fromStdString(document(requiredEntries(entry(
            "x", "ABCDEFabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcd"))));
        QTest::newRow("size-overflow") << QByteArray::fromStdString(document(requiredEntries(entry(
            "x", zeroDigest, "18446744073709551616"))));
        QTest::newRow("missing-required") << QByteArray::fromStdString(document(
            entry("Tweakopedia.App.exe") + "," + entry("Tweakopedia.Executor.exe")));

        QByteArray invalidUtf8 = QByteArray::fromStdString(document(requiredEntries()));
        const auto insertion = invalidUtf8.indexOf("content/categories.yaml");
        invalidUtf8[insertion] = static_cast<char>(0xff);
        QTest::newRow("invalid-utf8") << invalidUtf8;
    }

    void rejectsInvalidSchema()
    {
        QFETCH(QByteArray, json);
        QVERIFY(!package::PayloadManifest::parse(
            std::string_view(json.constData(), static_cast<std::size_t>(json.size()))).ok());
    }

    void buildsManifestFromDirectory()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        writeFile(directory.filePath("Tweakopedia.App.exe"), "app");
        writeFile(directory.filePath("Tweakopedia.Executor.exe"), "executor");
        writeFile(directory.filePath("content/categories.yaml"), "categories: []\n");
        writeFile(directory.filePath("plugins/example.dll"), "plugin");
        writeFile(directory.filePath("payload-manifest.json"), "ignored");

        const auto manifest = package::PayloadManifest::fromDirectory(
            std::filesystem::path(directory.path().toStdWString()));
        QVERIFY(manifest.ok());
        QCOMPARE(manifest.value->entries().size(), 4);
        QVERIFY(manifest.value->find("Tweakopedia.App.exe"));
        QVERIFY(manifest.value->find("plugins/example.dll"));
        QVERIFY(!manifest.value->find("payload-manifest.json"));
        QCOMPARE(manifest.value->find("Tweakopedia.App.exe")->size, 3);
        QVERIFY(package::PayloadManifest::parse(manifest.value->toCanonicalJson()).ok());
    }
};

QTEST_MAIN(PayloadManifestTest)
#include "PayloadManifestTest.moc"
