#include "persistence/AppPaths.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QTest>

using namespace tweakopedia::persistence;
using namespace Qt::StringLiterals;

class AppPathsTest final : public QObject
{
    Q_OBJECT

private slots:
    void keepsContentUnderApplicationDirectory()
    {
        QTemporaryDir applicationDirectory;
        QTemporaryDir dataDirectory;
        QVERIFY(applicationDirectory.isValid());
        QVERIFY(dataDirectory.isValid());

        const AppPaths paths(applicationDirectory.path(), dataDirectory.path());

        QCOMPARE(paths.contentRoot(), QDir(applicationDirectory.path()).filePath(u"content"_s));
    }

    void usesLocalAppDataForDefaultWritableRoot()
    {
        const AppPaths paths(u"D:\\Portable\\Tweakopedia"_s);
        const auto localAppData = QDir::cleanPath(qEnvironmentVariable("LOCALAPPDATA"));

        QVERIFY(!localAppData.isEmpty());
        QCOMPARE(paths.productRoot(), QDir(localAppData).filePath(u"Tweakopedia"_s));
        QCOMPARE(paths.dataRoot(), QDir(paths.productRoot()).filePath(u"Data"_s));
        QCOMPARE(paths.runtimeRoot(), QDir(paths.productRoot()).filePath(u"Runtime"_s));
    }

    void acceptsIsolatedWritableRootForTests()
    {
        QTemporaryDir applicationDirectory;
        QTemporaryDir dataDirectory;
        const AppPaths paths(applicationDirectory.path(), dataDirectory.path());

        QVERIFY(paths.ensureDataDirectories());
        QVERIFY(QFileInfo::exists(paths.transactionsRoot()));
        QVERIFY(QFileInfo::exists(paths.logsRoot()));
        QCOMPARE(QFileInfo(paths.databasePath()).absolutePath(), QDir::cleanPath(dataDirectory.path()));
        QCOMPARE(paths.dataRoot(), QDir::cleanPath(dataDirectory.path()));
    }

    void neverCreatesWritableDataBesideExecutable()
    {
        QTemporaryDir applicationDirectory;
        QTemporaryDir dataDirectory;
        const AppPaths paths(applicationDirectory.path(), dataDirectory.path());

        QVERIFY(paths.ensureDataDirectories());

        const auto applicationEntries = QDir(applicationDirectory.path()).entryList(
            QDir::AllEntries | QDir::NoDotAndDotDot);
        QVERIFY(applicationEntries.isEmpty());
    }
};

QTEST_APPLESS_MAIN(AppPathsTest)

#include "AppPathsTest.moc"
