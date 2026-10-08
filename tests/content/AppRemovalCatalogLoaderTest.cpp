#include "content/AppRemovalCatalogLoader.h"

#include <QDir>
#include <QSet>
#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

class AppRemovalCatalogLoaderTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesOnlyInstalledAppxPackagesAsActions()
    {
        const auto path = QDir(QString::fromUtf8(TWEAKOPEDIA_CONTENT_ROOT))
                              .filePath(u"apps.json"_s);
        const QSet<QString> installed{u"Clipchamp.Clipchamp"_s, u"Microsoft.WindowsStore"_s};

        const auto result = content::AppRemovalCatalogLoader{}.loadFile(path, installed);

        QVERIFY2(result.errors.isEmpty(), qPrintable(result.errors.join(u'\n')));
        QCOMPARE(result.tweaks.size(), 2);

        const auto clipchampId = *domain::TweakId::parse(u"apps.remove.clipchamp.clipchamp"_s);
        const auto clipchamp = std::find_if(
            result.tweaks.cbegin(), result.tweaks.cend(),
            [&](const domain::TweakDefinition& tweak) { return tweak.id == clipchampId; });
        QVERIFY(clipchamp != result.tweaks.cend());
        QCOMPARE(clipchamp->title, u"Удалить Clipchamp"_s);
        QCOMPARE(clipchamp->category, u"app-removal"_s);
        QCOMPARE(clipchamp->subcategory, u"installed"_s);
        QCOMPARE(clipchamp->kind, domain::TweakKind::Action);
        QCOMPARE(clipchamp->states.size(), 1);
        QCOMPARE(clipchamp->states.first().id, u"remove"_s);
        QCOMPARE(clipchamp->impact, domain::Impact::Low);
        QCOMPARE(clipchamp->reversibility, domain::Reversibility::Conditional);
        QVERIFY(clipchamp->appxDetection.has_value());
        QCOMPARE(clipchamp->appxDetection->packageName, u"Clipchamp.Clipchamp"_s);
        const auto operation = std::get_if<domain::RemoveAppxPackageOperation>(
            &clipchamp->states.first().operations.first());
        QVERIFY(operation != nullptr);
        QCOMPARE(operation->packageName, u"Clipchamp.Clipchamp"_s);

        const auto store = std::find_if(
            result.tweaks.cbegin(), result.tweaks.cend(),
            [](const domain::TweakDefinition& tweak) {
                return tweak.title == u"Удалить Microsoft Store";
            });
        QVERIFY(store != result.tweaks.cend());
        QCOMPARE(store->impact, domain::Impact::Critical);
    }

    void loadsCompleteBundledAppxMetadata()
    {
        const auto path = QDir(QString::fromUtf8(TWEAKOPEDIA_CONTENT_ROOT))
                              .filePath(u"apps.json"_s);
        const auto metadata = content::AppRemovalCatalogLoader{}.readMetadata(path);

        QVERIFY2(metadata.errors.isEmpty(), qPrintable(metadata.errors.join(u'\n')));
        QCOMPARE(metadata.apps.size(), 138);
        QVERIFY(std::none_of(metadata.apps.cbegin(), metadata.apps.cend(),
                            [](const auto& app) { return app.packageName.isEmpty(); }));
    }
};

QTEST_APPLESS_MAIN(AppRemovalCatalogLoaderTest)

#include "AppRemovalCatalogLoaderTest.moc"
