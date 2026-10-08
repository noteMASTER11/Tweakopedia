#include "execution/AppxPackageExecutor.h"

#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

class FakeAppxBackend final : public platform::IAppxPackageBackend
{
public:
    platform::AppxPackageQueryResult installedForCurrentUser() const override
    {
        return {.packages = packages};
    }

    platform::AppxPackageMutationResult removeCurrentUser(const QString& fullName) override
    {
        removed.append(fullName);
        for (qsizetype index = packages.size() - 1; index >= 0; --index) {
            if (packages.at(index).fullName == fullName) packages.removeAt(index);
        }
        return {.success = true};
    }

    mutable QVector<platform::AppxPackageIdentity> packages;
    QVector<QString> removed;
};

} // namespace

class AppxPackageExecutorTest final : public QObject
{
    Q_OBJECT

private slots:
    void capturesAndRemovesExactMatchingPackages()
    {
        FakeAppxBackend backend;
        backend.packages = {
            {u"Clipchamp.Clipchamp"_s, u"Clipchamp.Clipchamp_3.0_x64__abc"_s},
            {u"Microsoft.WindowsStore"_s, u"Microsoft.WindowsStore_1.0_x64__abc"_s},
        };
        execution::AppxPackageExecutor executor(backend);

        const auto captured = executor.capture(u"clipchamp.clipchamp"_s);
        QVERIFY(captured.success);
        QCOMPARE(captured.snapshot.packages.size(), 1);
        QCOMPARE(captured.snapshot.fingerprint().size(), 64);

        const auto applied = executor.apply(captured.snapshot);

        QVERIFY(applied.success);
        QCOMPARE(backend.removed, QVector<QString>{u"Clipchamp.Clipchamp_3.0_x64__abc"_s});
        QCOMPARE(backend.packages.size(), 1);
    }

    void rejectsChangedPackageSet()
    {
        FakeAppxBackend backend;
        backend.packages = {{u"Clipchamp.Clipchamp"_s, u"Clipchamp.Clipchamp_3.0_x64__abc"_s}};
        execution::AppxPackageExecutor executor(backend);
        const auto captured = executor.capture(u"Clipchamp.Clipchamp"_s);
        QVERIFY(captured.success);

        const auto compared = executor.compareBefore(captured.snapshot, QByteArray(64, 'a'));

        QVERIFY(!compared.success);
        QCOMPARE(compared.code, u"state.changed"_s);
        QVERIFY(backend.removed.isEmpty());
    }
};

QTEST_APPLESS_MAIN(AppxPackageExecutorTest)

#include "AppxPackageExecutorTest.moc"
