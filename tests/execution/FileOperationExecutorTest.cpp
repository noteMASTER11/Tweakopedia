#include "execution/FileOperationExecutor.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QTest>

using namespace tweakopedia::domain;
using namespace tweakopedia::execution;
using namespace Qt::StringLiterals;

namespace {

void writeFile(const QString& path, const QByteArray& bytes)
{
    QVERIFY(QDir{}.mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(bytes), bytes.size());
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

InputArtifact artifact(const QString& relativePath, const QByteArray& bytes)
{
    return {
        .id = u"logo"_s,
        .storageId = u"artifact-1"_s,
        .managedPath = relativePath,
        .size = static_cast<quint64>(bytes.size()),
        .sha256 = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex(),
    };
}

} // namespace

class FileOperationExecutorTest final : public QObject
{
    Q_OBJECT

private slots:
    void replacesAndRestoresExistingDestination()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto transaction = root.filePath(u"transaction"_s);
        const auto input = QDir(transaction).filePath(u"inputs/artifact-1/logo.bmp"_s);
        const auto destination = root.filePath(u"target/logo.bmp"_s);
        writeFile(input, "new");
        writeFile(destination, "old");
        FileOperation operation{
            .kind = FileOperationKind::Replace,
            .artifact = artifact(u"inputs/artifact-1/logo.bmp"_s, "new"),
            .destination = destination,
        };
        FileOperationExecutor executor(transaction);

        const auto captured = executor.capture(operation);
        QVERIFY(captured.success);
        QVERIFY(executor.apply(operation).success);
        QCOMPARE(readFile(destination), QByteArray("new"));
        QVERIFY(executor.restore(captured.snapshot).success);
        QCOMPARE(readFile(destination), QByteArray("old"));
    }

    void deleteRestoresBackupAndCopyRollsBackToMissing()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto transaction = root.filePath(u"transaction"_s);
        const auto destination = root.filePath(u"target/file.bin"_s);
        writeFile(destination, "old");
        FileOperationExecutor executor(transaction);
        FileOperation remove{.kind = FileOperationKind::Delete, .destination = destination};
        const auto removed = executor.capture(remove);
        QVERIFY(removed.success);
        QVERIFY(executor.apply(remove).success);
        QVERIFY(!QFileInfo::exists(destination));
        QVERIFY(executor.restore(removed.snapshot).success);
        QCOMPARE(readFile(destination), QByteArray("old"));

        const auto input = QDir(transaction).filePath(u"inputs/artifact-1/file.bin"_s);
        const auto created = root.filePath(u"target/created.bin"_s);
        writeFile(input, "new");
        FileOperation copy{
            .kind = FileOperationKind::Copy,
            .artifact = artifact(u"inputs/artifact-1/file.bin"_s, "new"),
            .destination = created,
        };
        const auto absent = executor.capture(copy);
        QVERIFY(absent.success);
        QVERIFY(executor.apply(copy).success);
        QVERIFY(executor.restore(absent.snapshot).success);
        QVERIFY(!QFileInfo::exists(created));
    }

    void rejectsTraversalHashMismatchAndCopyOverExistingFile()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto destination = root.filePath(u"target.bin"_s);
        writeFile(destination, "old");
        FileOperationExecutor executor(root.filePath(u"transaction"_s));

        FileOperation traversal{
            .kind = FileOperationKind::Replace,
            .artifact = artifact(u"../outside.bin"_s, "new"),
            .destination = destination,
        };
        QCOMPARE(executor.apply(traversal).code, u"file.artifact_path_invalid"_s);

        const auto input = root.filePath(u"transaction/inputs/artifact-1/file.bin"_s);
        writeFile(input, "tampered");
        FileOperation mismatch{
            .kind = FileOperationKind::Replace,
            .artifact = artifact(u"inputs/artifact-1/file.bin"_s, "expected"),
            .destination = destination,
        };
        QCOMPARE(executor.apply(mismatch).code, u"file.artifact_hash_mismatch"_s);

        writeFile(input, "new");
        FileOperation copy{
            .kind = FileOperationKind::Copy,
            .artifact = artifact(u"inputs/artifact-1/file.bin"_s, "new"),
            .destination = destination,
        };
        QCOMPARE(executor.apply(copy).code, u"file.destination_exists"_s);
    }
};

QTEST_APPLESS_MAIN(FileOperationExecutorTest)

#include "FileOperationExecutorTest.moc"
