#include <QtTest/QTest>

class BuildSmokeTest final : public QObject
{
    Q_OBJECT

private slots:
    void qtTestRuntimeStarts()
    {
        QVERIFY(true);
    }
};

QTEST_MAIN(BuildSmokeTest)

#include "BuildSmokeTest.moc"
