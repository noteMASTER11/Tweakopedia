#include "UiFontLoader.h"

#include <QFontDatabase>
#include <QTest>

using namespace tweakopedia;

class UiFontLoaderTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadsBundledSfPro()
    {
        const auto family = ui::loadBundledUiFont();
        QVERIFY2(!family.isEmpty(), "Bundled SF Pro font could not be loaded");
        QVERIFY(family.startsWith(QStringLiteral("SF Pro"), Qt::CaseInsensitive));
        QVERIFY(QFontDatabase::families().contains(family));
    }
};

QTEST_MAIN(UiFontLoaderTest)
#include "UiFontLoaderTest.moc"
