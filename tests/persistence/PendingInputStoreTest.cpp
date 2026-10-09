#include "persistence/PendingInputStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QTest>

using namespace tweakopedia::persistence;
using namespace Qt::StringLiterals;

namespace {

void writeFile(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(bytes), bytes.size());
}

} // namespace

class PendingInputStoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void importsValidatedFileWithSizeAndHash()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto source = root.filePath(u"logo.BMP"_s);
        writeFile(source, "BM-test");
        PendingInputStore store(root.filePath(u"pending"_s));

        const auto imported = store.importFile(
            u"logo"_s, source, {u"bmp"_s}, 1024);

        QVERIFY2(imported.artifact.has_value(), qPrintable(imported.code));
        QCOMPARE(imported.artifact->id, u"logo"_s);
        QCOMPARE(imported.artifact->size, quint64{7});
        QCOMPARE(imported.artifact->sha256,
                 QCryptographicHash::hash("BM-test", QCryptographicHash::Sha256).toHex());
        QVERIFY(QFileInfo::exists(imported.artifact->managedPath));
        QVERIFY(QDir::isAbsolutePath(imported.artifact->managedPath));
    }

    void rejectsExtensionSizeAndSourceMutation()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto source = root.filePath(u"logo.bmp"_s);
        writeFile(source, "BM-too-large");
        PendingInputStore store(root.filePath(u"pending"_s));
        QCOMPARE(store.importFile(u"logo"_s, source, {u"png"_s}, 100).code,
                 u"input.file_extension_invalid"_s);
        QCOMPARE(store.importFile(u"logo"_s, source, {u"bmp"_s}, 2).code,
                 u"input.file_too_large"_s);

        PendingInputStore mutatingStore(root.filePath(u"pending-mutating"_s), [&] {
            writeFile(source, "BM-changed");
        });
        QCOMPARE(mutatingStore.importFile(u"logo"_s, source, {u"bmp"_s}, 100).code,
                 u"input.source_changed"_s);
    }

    void removesOnlyUnusedArtifacts()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto first = root.filePath(u"first.bmp"_s);
        const auto second = root.filePath(u"second.bmp"_s);
        writeFile(first, "first");
        writeFile(second, "second");
        PendingInputStore store(root.filePath(u"pending"_s));
        const auto a = store.importFile(u"first"_s, first, {u"bmp"_s}, 100).artifact;
        const auto b = store.importFile(u"second"_s, second, {u"bmp"_s}, 100).artifact;
        QVERIFY(a && b);

        QVERIFY(store.cleanupUnused({a->storageId}));

        QVERIFY(QFileInfo::exists(a->managedPath));
        QVERIFY(!QFileInfo::exists(b->managedPath));
    }
};

QTEST_APPLESS_MAIN(PendingInputStoreTest)

#include "PendingInputStoreTest.moc"
