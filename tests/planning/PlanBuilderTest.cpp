#include "planning/PlanBuilder.h"

#include <QtTest/QTest>

using namespace tweakopedia::content;
using namespace tweakopedia::domain;
using namespace tweakopedia::planning;
using namespace Qt::StringLiterals;

namespace {

TweakDefinition definition(QStringView id, QStringView valueName)
{
    const RegistryLocation location{
        .hive = RegistryHive::LocalMachine,
        .key = u"SOFTWARE\\Tweakopedia"_s,
        .valueName = valueName.toString(),
        .view = RegistryView::Registry64,
    };
    TweakDefinition tweak;
    tweak.id = *TweakId::parse(id);
    tweak.title = u"Тестовый параметр"_s;
    tweak.compatibility.architectures = {CpuArchitecture::X64};
    tweak.compatibility.operatingSystems = {WindowsFamily::Windows11};
    tweak.compatibility.minimumBuild = 22000;
    tweak.states = {
        {
            .id = u"disabled"_s,
            .title = u"Выключено"_s,
            .operations = {SetRegistryDwordOperation{.location = location, .value = 0}},
        },
        {
            .id = u"enabled"_s,
            .title = u"Включено"_s,
            .operations = {SetRegistryDwordOperation{.location = location, .value = 1}},
        },
    };
    return tweak;
}

SystemProfile profile()
{
    return {
        .family = WindowsFamily::Windows11,
        .build = 26200,
        .ubr = 9457,
        .edition = u"Professional"_s,
        .architecture = CpuArchitecture::X64,
    };
}

DetectedState state(QStringView stateId, QByteArray fingerprint = "before-hash")
{
    return {
        .status = DetectionStatus::Named,
        .stateId = stateId.toString(),
        .details = u"Тест"_s,
        .fingerprint = std::move(fingerprint),
    };
}

} // namespace

class PlanBuilderTest final : public QObject
{
    Q_OBJECT

private slots:
    void omitsNoOpWhenTargetMatchesCurrentState()
    {
        const auto tweak = definition(u"filesystem.alpha", u"Alpha");
        const TweakCatalog catalog({tweak});
        TweakQueue queue;
        QVERIFY(queue.setTarget(tweak, u"disabled").accepted);
        const QHash<TweakId, DetectedState> detected{{tweak.id, state(u"disabled")}};

        const auto result = PlanBuilder{}.build(catalog, queue, detected, profile());

        QVERIFY(result.plan.has_value());
        QVERIFY(result.issues.isEmpty());
        QVERIFY(result.plan->operations.isEmpty());
        QCOMPARE(result.plan->summary, u"Изменения не требуются."_s);
    }

    void blocksUnsupportedAndUnknownState_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<QString>("issueCode");
        QTest::newRow("unsupported") << static_cast<int>(DetectionStatus::Unsupported) << u"state.unsupported"_s;
        QTest::newRow("unknown") << static_cast<int>(DetectionStatus::Unknown) << u"state.unknown"_s;
    }

    void blocksUnsupportedAndUnknownState()
    {
        QFETCH(int, status);
        QFETCH(QString, issueCode);
        const auto tweak = definition(u"filesystem.alpha", u"Alpha");
        const TweakCatalog catalog({tweak});
        TweakQueue queue;
        QVERIFY(queue.setTarget(tweak, u"enabled").accepted);
        const QHash<TweakId, DetectedState> detected{{
            tweak.id,
            DetectedState{.status = static_cast<DetectionStatus>(status), .stateId = u"unknown"_s},
        }};

        const auto result = PlanBuilder{}.build(catalog, queue, detected, profile());

        QVERIFY(!result.plan.has_value());
        QVERIFY(std::any_of(result.issues.cbegin(), result.issues.cend(), [&](const PlanIssue& issue) {
            return issue.code == issueCode;
        }));
    }

    void buildsTypedDwordOperationWithBeforeFingerprint()
    {
        const auto tweak = definition(u"filesystem.alpha", u"Alpha");
        const TweakCatalog catalog({tweak});
        TweakQueue queue;
        QVERIFY(queue.setTarget(tweak, u"enabled").accepted);
        const QHash<TweakId, DetectedState> detected{{tweak.id, state(u"disabled", "actual-before")}};

        const auto result = PlanBuilder{}.build(catalog, queue, detected, profile());

        QVERIFY(result.plan.has_value());
        QCOMPARE(result.plan->schemaVersion, 1);
        QCOMPARE(result.plan->operations.size(), 1);
        QVERIFY(std::holds_alternative<PlannedRegistryDwordChange>(result.plan->operations.first()));
        const auto& operation = std::get<PlannedRegistryDwordChange>(result.plan->operations.first());
        QCOMPARE(operation.change.value, quint32{1});
        QCOMPARE(operation.beforeFingerprint, QByteArray("actual-before"));
        QCOMPARE(operation.tweakId, tweak.id);
        QVERIFY(!result.plan->transactionId.isNull());
    }

    void sortsOperationsByStableTweakId()
    {
        const auto alpha = definition(u"filesystem.alpha", u"Alpha");
        const auto beta = definition(u"filesystem.beta", u"Beta");
        const TweakCatalog catalog({beta, alpha});
        TweakQueue queue;
        QVERIFY(queue.setTarget(beta, u"enabled").accepted);
        QVERIFY(queue.setTarget(alpha, u"enabled").accepted);
        const QHash<TweakId, DetectedState> detected{
            {beta.id, state(u"disabled", "beta")},
            {alpha.id, state(u"disabled", "alpha")},
        };

        const auto result = PlanBuilder{}.build(catalog, queue, detected, profile());

        QVERIFY(result.plan.has_value());
        QCOMPARE(result.plan->operations.size(), 2);
        const auto& first = std::get<PlannedRegistryDwordChange>(result.plan->operations.at(0));
        const auto& second = std::get<PlannedRegistryDwordChange>(result.plan->operations.at(1));
        QCOMPARE(first.tweakId.toString(), u"filesystem.alpha"_s);
        QCOMPARE(second.tweakId.toString(), u"filesystem.beta"_s);
    }

    void writesRussianSummary()
    {
        const auto tweak = definition(u"filesystem.alpha", u"Alpha");
        const TweakCatalog catalog({tweak});
        TweakQueue queue;
        QVERIFY(queue.setTarget(tweak, u"enabled").accepted);
        const QHash<TweakId, DetectedState> detected{{tweak.id, state(u"disabled")}};

        const auto result = PlanBuilder{}.build(catalog, queue, detected, profile());

        QVERIFY(result.plan.has_value());
        QVERIFY(result.plan->summary.contains(QRegularExpression(u"[А-Яа-яЁё]"_s)));
    }
};

QTEST_APPLESS_MAIN(PlanBuilderTest)

#include "PlanBuilderTest.moc"
