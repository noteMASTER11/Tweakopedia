#include "execution/SystemToolLogWriter.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QTest>

using namespace tweakopedia::execution;
using namespace Qt::StringLiterals;

class SystemToolLogWriterTest final : public QObject
{
    Q_OBJECT

private slots:
    void writesHeaderCommandsAndCompleteRawOutput()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        SystemToolLogWriter writer;
        QVERIFY(writer.open(root.path(), SystemToolOperation::Scan,
                            {u"Windows: test"_s, u"DISM: test"_s}));
        writer.appendCommand(u"DISM.exe"_s,
                             {u"/Online"_s, u"/Cleanup-Image"_s, u"/ScanHealth"_s, u"/English"_s});
        writer.appendOutput("first\rsecond\n");
        writer.close();

        QVERIFY(writer.path().contains(u"Tools/SystemIntegrity"_s));
        QFile file(writer.path());
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto bytes = file.readAll();
        QVERIFY(bytes.contains("Operation: scan"));
        QVERIFY(bytes.contains("DISM.exe /Online /Cleanup-Image /ScanHealth /English"));
        QVERIFY(bytes.contains("first\rsecond\n"));
    }

    void reportsDirectoryCreationFailure()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QFile blocker(root.filePath(u"blocked"_s));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();

        SystemToolLogWriter writer;
        QVERIFY(!writer.open(blocker.fileName(), SystemToolOperation::Scan, {}));
        QVERIFY(!writer.errorString().isEmpty());
    }
};

QTEST_GUILESS_MAIN(SystemToolLogWriterTest)
#include "SystemToolLogWriterTest.moc"
