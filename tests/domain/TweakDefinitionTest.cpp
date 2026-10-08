#include "domain/TweakDefinition.h"

#include <QtTest/QTest>

using namespace tweakopedia::domain;
using namespace Qt::StringLiterals;

namespace {

TweakDefinition validDefinition()
{
    TweakDefinition definition;
    definition.id = *TweakId::parse(u"filesystem.win32-long-paths");
    definition.title = u"Поддержка длинных путей Win32"_s;
    definition.category = u"filesystem"_s;
    definition.subcategory = u"paths"_s;
    definition.kind = TweakKind::Setting;
    definition.summary = u"Разрешает приложениям с поддержкой longPathAware работать с длинными путями."_s;
    definition.explanation = {
        .purpose = u"Снимает историческое ограничение длины пути для совместимых приложений."_s,
        .mechanism = u"Windows проверяет системный параметр и manifest приложения."_s,
        .effect = u"Совместимые программы смогут открывать более длинные пути."_s,
        .tradeoffs = u"Старые программы сохраняют прежнее поведение."_s,
        .recommendation = u"Полезно при работе с глубокими деревьями каталогов."_s,
        .technicalDetails = u"Изменяется DWORD LongPathsEnabled."_s,
    };
    definition.states = {
        TweakStateDefinition{.id = u"disabled"_s, .title = u"Выключено"_s},
        TweakStateDefinition{.id = u"enabled"_s, .title = u"Включено"_s},
    };
    definition.impact = Impact::Low;
    definition.reversibility = Reversibility::Reversible;
    definition.restart = RestartRequirement::None;
    return definition;
}

} // namespace

class TweakDefinitionTest final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsStableLowercaseId()
    {
        const auto id = TweakId::parse(u"filesystem.win32-long-paths");

        QVERIFY(id.has_value());
        QCOMPARE(id->toString(), u"filesystem.win32-long-paths"_s);
    }

    void rejectsMalformedId_data()
    {
        QTest::addColumn<QString>("candidate");

        QTest::newRow("empty") << QString{};
        QTest::newRow("space") << u"filesystem.long paths"_s;
        QTest::newRow("uppercase") << u"Filesystem.long-paths"_s;
        QTest::newRow("double-dot") << u"filesystem..long-paths"_s;
    }

    void rejectsMalformedId()
    {
        QFETCH(QString, candidate);

        QVERIFY(!TweakId::parse(candidate).has_value());
    }

    void acceptsOnlyDeclaredTargetState()
    {
        const auto definition = validDefinition();

        QVERIFY(definition.supportsTargetState(u"enabled"));
        QVERIFY(definition.supportsTargetState(u"disabled"));
        QVERIFY(!definition.supportsTargetState(u"custom"));
    }

    void requiresRussianTitle()
    {
        auto definition = validDefinition();
        QVERIFY(definition.isValid());

        definition.title = u"Enable long paths"_s;
        QVERIFY(!definition.isValid());

        definition.title.clear();
        QVERIFY(!definition.isValid());
    }

    void requiresEveryExplanationSection_data()
    {
        QTest::addColumn<QString>("section");

        QTest::newRow("purpose") << u"purpose"_s;
        QTest::newRow("mechanism") << u"mechanism"_s;
        QTest::newRow("effect") << u"effect"_s;
        QTest::newRow("tradeoffs") << u"tradeoffs"_s;
        QTest::newRow("recommendation") << u"recommendation"_s;
        QTest::newRow("technical-details") << u"technical-details"_s;
    }

    void requiresEveryExplanationSection()
    {
        QFETCH(QString, section);
        auto definition = validDefinition();

        if (section == u"purpose") definition.explanation.purpose.clear();
        if (section == u"mechanism") definition.explanation.mechanism.clear();
        if (section == u"effect") definition.explanation.effect.clear();
        if (section == u"tradeoffs") definition.explanation.tradeoffs.clear();
        if (section == u"recommendation") definition.explanation.recommendation.clear();
        if (section == u"technical-details") definition.explanation.technicalDetails.clear();

        QVERIFY(!definition.isValid());
    }
};

QTEST_APPLESS_MAIN(TweakDefinitionTest)

#include "TweakDefinitionTest.moc"
