#include "persistence/TransactionFiles.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QCryptographicHash>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest/QTest>

using namespace tweakopedia::persistence;
using namespace Qt::StringLiterals;

class TransactionFilesTest final : public QObject
{
    Q_OBJECT

private slots:
    void createsDirectoryBeforeExecutorStarts()
    {
        QTemporaryDir root;
        const auto id = QUuid::createUuid();
        TransactionFiles files(root.path());

        QVERIFY(files.create(id));
        QVERIFY(QFileInfo(files.directory(id)).isDir());
        QVERIFY(QFileInfo(QDir(files.directory(id)).filePath(u"operations.log"_s)).isFile());
    }

    void atomicallyWritesAndReadsTransactionJson()
    {
        QTemporaryDir root;
        const auto id = QUuid::createUuid();
        TransactionFiles files(root.path());
        QVERIFY(files.create(id));

        const QJsonObject plan{{u"kind"_s, u"plan"_s}, {u"number"_s, 1}};
        const QJsonObject before{{u"presence"_s, u"missing"_s}};
        const QJsonObject result{{u"status"_s, u"succeeded"_s}};

        QVERIFY(files.writePlan(id, plan));
        QVERIFY(files.writeBefore(id, before));
        QVERIFY(files.writeResult(id, result));
        QCOMPARE(files.readPlan(id), std::optional<QJsonObject>{plan});
        QCOMPARE(files.readBefore(id), std::optional<QJsonObject>{before});
        QCOMPARE(files.readResult(id), std::optional<QJsonObject>{result});

        const auto temporaryFiles = QDir(files.directory(id)).entryList({u"*.tmp"_s}, QDir::Files);
        QVERIFY(temporaryFiles.isEmpty());
    }

    void replacesWholeJsonDocumentOnRewrite()
    {
        QTemporaryDir root;
        const auto id = QUuid::createUuid();
        TransactionFiles files(root.path());
        QVERIFY(files.create(id));
        QVERIFY(files.writeResult(id, QJsonObject{{u"old"_s, true}}));

        const QJsonObject replacement{{u"new"_s, true}};
        QVERIFY(files.writeResult(id, replacement));

        QCOMPARE(files.readResult(id), std::optional<QJsonObject>{replacement});
    }

    void materializesVerifiedArtifactAsTransactionRelativePath()
    {
        QTemporaryDir root;
        const auto id = QUuid::createUuid();
        TransactionFiles files(root.filePath(u"transactions"_s));
        QVERIFY(files.create(id));
        const auto source = root.filePath(u"pending/logo.bmp"_s);
        QVERIFY(QDir{}.mkpath(QFileInfo(source).absolutePath()));
        QFile input(source);
        QVERIFY(input.open(QIODevice::WriteOnly));
        QCOMPARE(input.write("logo"), 4);
        input.close();
        const auto hash = QCryptographicHash::hash("logo", QCryptographicHash::Sha256).toHex();

        const auto result = files.materializeInput(id, {
            .id = u"logo"_s, .storageId = u"artifact-1"_s,
            .managedPath = source, .size = 4, .sha256 = hash,
        });

        QVERIFY(result.artifact.has_value());
        QCOMPARE(result.artifact->managedPath, u"inputs/artifact-1/logo.bmp"_s);
        QVERIFY(QFileInfo::exists(QDir(files.directory(id)).filePath(result.artifact->managedPath)));
    }
};

QTEST_APPLESS_MAIN(TransactionFilesTest)

#include "TransactionFilesTest.moc"
