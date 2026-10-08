#include <QtTest>

#include <QImage>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class BrandingAssetsTest final : public QObject
{
    Q_OBJECT

private slots:
    void qtResourcesExposeBrandImages();
    void executablesContainWindowsIcons();
};

void BrandingAssetsTest::qtResourcesExposeBrandImages()
{
    const QImage logo(QStringLiteral(":/images/tweakopedia-logo.png"));
    QVERIFY2(!logo.isNull(), "The horizontal Tweakopedia logo is missing from Qt resources");
    QCOMPARE(logo.size(), QSize(2172, 724));

    const QImage icon(QStringLiteral(":/images/tweakopedia-icon.png"));
    QVERIFY2(!icon.isNull(), "The square Tweakopedia icon is missing from Qt resources");
    QCOMPARE(icon.size(), QSize(1254, 1254));
    QVERIFY(icon.hasAlphaChannel());
}

void BrandingAssetsTest::executablesContainWindowsIcons()
{
#ifdef Q_OS_WIN
    const QStringList executablePaths{
        QString::fromUtf8(TWEAKOPEDIA_TEST_CONTAINER),
        QString::fromUtf8(TWEAKOPEDIA_TEST_GUI),
        QString::fromUtf8(TWEAKOPEDIA_TEST_EXECUTOR),
    };

    for (const auto& executablePath : executablePaths) {
        const auto module = LoadLibraryExW(
            reinterpret_cast<LPCWSTR>(executablePath.utf16()),
            nullptr,
            LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
        QVERIFY2(module != nullptr, qPrintable(QStringLiteral("Cannot inspect %1").arg(executablePath)));
        const auto icon = FindResourceW(module, MAKEINTRESOURCEW(1), RT_GROUP_ICON);
        const bool found = icon != nullptr;
        FreeLibrary(module);
        QVERIFY2(found, qPrintable(QStringLiteral("Windows icon resource 1 is missing from %1").arg(executablePath)));
    }
#else
    QSKIP("Windows PE resources are only available on Windows");
#endif
}

QTEST_MAIN(BrandingAssetsTest)
#include "BrandingAssetsTest.moc"
