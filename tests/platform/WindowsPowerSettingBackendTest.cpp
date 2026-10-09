#include "platform/WindowsPowerSettingBackend.h"

#include <QScopeGuard>
#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

class WindowsPowerSettingBackendTest final : public QObject
{
    Q_OBJECT
private slots:
    void changesOnlyTemporaryDuplicateScheme()
    {
        platform::WindowsPowerSettingBackend backend;
        const auto duplicate = backend.duplicateActiveScheme();
        if (!duplicate.scheme) QSKIP(qPrintable(duplicate.error));
        const auto cleanup = qScopeGuard([&] { (void)backend.deleteScheme(*duplicate.scheme); });
        domain::PowerSettingLocation location{
            *duplicate.scheme,
            u"54533251-82be-4824-96c1-47b60b740d00"_s,
            u"893dee8e-2bef-41e0-89c6-b55d0929964c"_s,
            domain::PowerSource::Ac};
        const auto before = backend.read(location);
        if (!before.success) QSKIP(qPrintable(before.error));
        const auto changed = before.index == 50 ? 51U : 50U;
        QVERIFY2(backend.write(location, changed).success, "write failed");
        QCOMPARE(backend.read(location).index, changed);
        QVERIFY(backend.write(location, before.index).success);
    }
};
QTEST_APPLESS_MAIN(WindowsPowerSettingBackendTest)
#include "WindowsPowerSettingBackendTest.moc"
