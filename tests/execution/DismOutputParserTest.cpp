#include "execution/DismOutputParser.h"

#include <QtTest/QTest>

using namespace tweakopedia::execution;

class DismOutputParserTest final : public QObject
{
    Q_OBJECT

private slots:
    void recognizesHealthStates_data()
    {
        QTest::addColumn<QByteArray>("output");
        QTest::addColumn<SystemToolOutcome>("expected");
        QTest::newRow("healthy") << QByteArray("No component store corruption detected.\r\n")
                                  << SystemToolOutcome::Healthy;
        QTest::newRow("repairable") << QByteArray("The component store is repairable.\r\n")
                                     << SystemToolOutcome::Repairable;
        QTest::newRow("non-repairable") << QByteArray("The component store cannot be repaired.\r\n")
                                         << SystemToolOutcome::NonRepairable;
    }

    void recognizesHealthStates()
    {
        QFETCH(QByteArray, output);
        QFETCH(SystemToolOutcome, expected);
        DismOutputParser parser;
        parser.reset(SystemToolStage::CheckHealth);
        parser.consume(output.first(output.size() / 2));
        parser.consume(output.sliced(output.size() / 2));
        QCOMPARE(parser.health(), expected);
    }

    void successPhraseAloneDoesNotClaimHealth()
    {
        DismOutputParser parser;
        parser.reset(SystemToolStage::CheckHealth);
        parser.consume("The operation completed successfully.\r\n");
        QCOMPARE(parser.health(), SystemToolOutcome::Unknown);
    }

    void extractsSplitMonotonicProgressAndResetsPerStage()
    {
        DismOutputParser parser;
        parser.reset(SystemToolStage::ScanHealth);
        parser.consume("[=====  43.");
        QCOMPARE(parser.progress(), -1);
        parser.consume("7%  =====]\r[ 12.0% ]");
        QCOMPARE(parser.progress(), 43);
        parser.consume("[ 100.0% ]");
        QCOMPARE(parser.progress(), 100);

        parser.reset(SystemToolStage::CheckHealth);
        QCOMPARE(parser.progress(), -1);
        QCOMPARE(parser.health(), SystemToolOutcome::Unknown);
    }
};

QTEST_GUILESS_MAIN(DismOutputParserTest)
#include "DismOutputParserTest.moc"
