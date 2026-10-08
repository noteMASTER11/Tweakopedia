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

    void buildsTypedAppxRemovalWithBeforeFingerprint()
    {
        TweakDefinition tweak;
        tweak.id = *TweakId::parse(u"apps.remove.clipchamp.clipchamp"_s);
        tweak.title = u"Удалить Clipchamp"_s;
        tweak.compatibility.architectures = {CpuArchitecture::X64};
        tweak.compatibility.operatingSystems = {WindowsFamily::Windows11};
        tweak.compatibility.minimumBuild = 22000;
        tweak.states = {{
            .id = u"remove"_s,
            .title = u"Удалить"_s,
            .operations = {RemoveAppxPackageOperation{u"Clipchamp.Clipchamp"_s}},
        }};
        const TweakCatalog catalog({tweak});
        TweakQueue queue;
        QVERIFY(queue.setTarget(tweak, u"remove").accepted);

        const auto result = PlanBuilder{}.build(
            catalog, queue, {{tweak.id, state(u"installed", QByteArray(64, 'c'))}}, profile());

        QVERIFY(result.plan.has_value());
        QCOMPARE(result.plan->operations.size(), 1);
        QVERIFY(std::holds_alternative<PlannedAppxRemoval>(result.plan->operations.first()));
        const auto& operation = std::get<PlannedAppxRemoval>(result.plan->operations.first());
        QCOMPARE(operation.change.packageName, u"Clipchamp.Clipchamp"_s);
        QCOMPARE(operation.beforeFingerprint, QByteArray(64, 'c'));
        QCOMPARE(operation.tweakId, tweak.id);
    }

    void usesFingerprintOfEachRegistryLocationForCompositeState()
    {
        auto tweak = definition(u"filesystem.composite", u"Primary");
        const RegistryLocation secondary{
            .hive = RegistryHive::LocalMachine,
            .key = u"SOFTWARE\\Tweakopedia"_s,
            .valueName = u"Secondary"_s,
            .view = RegistryView::Registry64,
        };
        tweak.states[1].operations.append(SetRegistryDwordOperation{
            .location = secondary,
            .value = 1,
        });
        const auto primary = std::get<SetRegistryDwordOperation>(
            tweak.states[1].operations.first()).location;
        auto detectedState = state(u"disabled", "primary-before");
        detectedState.registryFingerprints = {
            {.location = primary, .fingerprint = "primary-before"},
            {.location = secondary, .fingerprint = "secondary-before"},
        };
        const TweakCatalog catalog({tweak});
        TweakQueue queue;
        QVERIFY(queue.setTarget(tweak, u"enabled").accepted);

        const auto result = PlanBuilder{}.build(
            catalog, queue, {{tweak.id, detectedState}}, profile());

        QVERIFY(result.plan.has_value());
        QCOMPARE(result.plan->operations.size(), 2);
        QCOMPARE(std::get<PlannedRegistryDwordChange>(result.plan->operations[0]).beforeFingerprint,
                 QByteArray("primary-before"));
        QCOMPARE(std::get<PlannedRegistryDwordChange>(result.plan->operations[1]).beforeFingerprint,
                 QByteArray("secondary-before"));
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

    void placesConditionalAppxRemovalAfterRegistryChanges()
    {
        const auto registryTweak = definition(u"filesystem.alpha"_s, u"Alpha"_s);
        TweakDefinition appxTweak;
        appxTweak.id = *TweakId::parse(u"apps.remove.clipchamp.clipchamp"_s);
        appxTweak.title = u"Удалить Clipchamp"_s;
        appxTweak.compatibility.architectures = {CpuArchitecture::X64};
        appxTweak.compatibility.operatingSystems = {WindowsFamily::Windows11};
        appxTweak.compatibility.minimumBuild = 22000;
        appxTweak.states = {{
            .id = u"remove"_s,
            .title = u"Удалить"_s,
            .operations = {RemoveAppxPackageOperation{u"Clipchamp.Clipchamp"_s}},
        }};
        const TweakCatalog catalog({appxTweak, registryTweak});
        TweakQueue queue;
        QVERIFY(queue.setTarget(appxTweak, u"remove"_s).accepted);
        QVERIFY(queue.setTarget(registryTweak, u"enabled"_s).accepted);

        const auto result = PlanBuilder{}.build(catalog, queue, {
            {appxTweak.id, state(u"installed"_s, QByteArray(64, 'c'))},
            {registryTweak.id, state(u"disabled"_s, QByteArray(64, 'd'))},
        }, profile());

        QVERIFY(result.plan.has_value());
        QCOMPARE(result.plan->operations.size(), 2);
        QVERIFY(std::holds_alternative<PlannedRegistryDwordChange>(result.plan->operations.first()));
        QVERIFY(std::holds_alternative<PlannedAppxRemoval>(result.plan->operations.last()));
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
