#include "content/TweakCatalogLoader.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QTest>

using namespace tweakopedia::content;
using namespace tweakopedia::domain;
using namespace Qt::StringLiterals;

namespace {

QString fixturePath(QStringView relative)
{
    return QDir(QString::fromUtf8(TWEAKOPEDIA_TEST_FIXTURES)).filePath(relative.toString());
}

void copyFixture(const QString& relative, const QString& targetDirectory, const QString& targetName = {})
{
    const auto source = fixturePath(relative);
    const auto destination = QDir(targetDirectory).filePath(
        targetName.isEmpty() ? QFileInfo(source).fileName() : targetName);
    QVERIFY2(QFile::copy(source, destination), qPrintable(u"Не удалось скопировать fixture: "_s + source));
}

bool hasErrorCode(const CatalogLoadResult& result, QStringView code)
{
    return std::any_of(result.errors.cbegin(), result.errors.cend(), [code](const CatalogError& error) {
        return error.code == code;
    });
}

CatalogLoadResult loadSingleFixture(const QString& relative)
{
    QTemporaryDir directory;
    if (!directory.isValid()) {
        return {};
    }
    copyFixture(relative, directory.path());
    return TweakCatalogLoader{}.loadDirectory(directory.path());
}

} // namespace

class TweakCatalogLoaderTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadsValidRegistryDwordTweak()
    {
        const auto result = loadSingleFixture(u"valid/win32-long-paths.yaml"_s);

        QVERIFY(result.errors.isEmpty());
        QVERIFY(result.catalog.has_value());
        QCOMPARE(result.catalog->size(), 1);

        const auto id = *TweakId::parse(u"filesystem.win32-long-paths");
        const auto* tweak = result.catalog->find(id);
        QVERIFY(tweak != nullptr);
        QCOMPARE(tweak->title, u"Поддержка длинных путей Win32"_s);
        QCOMPARE(tweak->detection->location.key, u"SYSTEM\\CurrentControlSet\\Control\\FileSystem"_s);
        QCOMPARE(tweak->detection->location.view, RegistryView::Registry64);
        QCOMPARE(tweak->detection->missingState, u"disabled"_s);
    }

    void rejectsMissingRequiredField()
    {
        const auto result = loadSingleFixture(u"invalid/missing-title.yaml"_s);

        QVERIFY(!result.catalog.has_value());
        QVERIFY(hasErrorCode(result, u"field.required"));
    }

    void rejectsUnknownField()
    {
        const auto result = loadSingleFixture(u"invalid/unknown-field.yaml"_s);

        QVERIFY(!result.catalog.has_value());
        QVERIFY(hasErrorCode(result, u"field.unknown"));
    }

    void rejectsInvalidDwordValue()
    {
        const auto result = loadSingleFixture(u"invalid/bad-dword.yaml"_s);

        QVERIFY(!result.catalog.has_value());
        QVERIFY(hasErrorCode(result, u"value.invalid_dword"));
    }

    void rejectsUnknownStateReference()
    {
        const auto result = loadSingleFixture(u"invalid/unknown-state.yaml"_s);

        QVERIFY(!result.catalog.has_value());
        QVERIFY(hasErrorCode(result, u"state.unknown"));
    }

    void rejectsDuplicateIdsAcrossFiles()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        copyFixture(u"invalid/duplicate-id-a.yaml"_s, directory.path());
        copyFixture(u"invalid/duplicate-id-b.yaml"_s, directory.path());

        const auto result = TweakCatalogLoader{}.loadDirectory(directory.path());

        QVERIFY(!result.catalog.has_value());
        QVERIFY(hasErrorCode(result, u"id.duplicate"));
    }

    void rejectsShellOperation()
    {
        const auto result = loadSingleFixture(u"invalid/unknown-operation.yaml"_s);

        QVERIFY(!result.catalog.has_value());
        QVERIFY(hasErrorCode(result, u"operation.unknown"));
    }

    void reportsFileAndYamlPosition()
    {
        const auto result = loadSingleFixture(u"invalid/unknown-field.yaml"_s);

        QVERIFY(!result.errors.isEmpty());
        QVERIFY(!result.errors.first().filePath.isEmpty());
        QVERIFY(result.errors.first().line > 0);
        QVERIFY(result.errors.first().column > 0);
    }

    void loadsRealCatalog()
    {
        const auto tweaksDirectory = QDir(QString::fromUtf8(TWEAKOPEDIA_CONTENT_ROOT)).filePath(u"tweaks"_s);
        const auto result = TweakCatalogLoader{}.loadDirectory(tweaksDirectory);

        QVERIFY(result.errors.isEmpty());
        QVERIFY(result.catalog.has_value());
        QCOMPARE(result.catalog->size(), 1);
        QVERIFY(result.catalog->find(*TweakId::parse(u"filesystem.win32-long-paths")) != nullptr);
    }
};

QTEST_APPLESS_MAIN(TweakCatalogLoaderTest)

#include "TweakCatalogLoaderTest.moc"
