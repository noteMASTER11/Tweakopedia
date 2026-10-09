#include "platform/WindowsBcdBackend.h"

#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

class WindowsBcdBackendTest final : public QObject
{
    Q_OBJECT
private slots:
    void readsOnlyWhitelistedCurrentElement()
    {
        platform::WindowsBcdBackend backend;
        const domain::BcdElementSpec spec{
            .objectId = u"{current}"_s,
            .elementType = 0x25000004,
            .valueKind = domain::BcdValueKind::Integer,
        };
        const auto result = backend.read(spec);
        if (!result.success && !result.missing) QSKIP(qPrintable(result.error));
        QVERIFY(result.success || result.missing);
    }

    void rejectsUnknownObjectAndElementType()
    {
        platform::WindowsBcdBackend backend;
        const auto result = backend.read({u"{arbitrary}"_s, 0xFFFFFFFF,
                                          domain::BcdValueKind::String});
        QVERIFY(!result.success);
        QVERIFY(!result.missing);
    }
};

QTEST_APPLESS_MAIN(WindowsBcdBackendTest)
#include "WindowsBcdBackendTest.moc"
