#include "planning/PlanBuilder.h"

#include <QtTest/QTest>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>

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

    void buildsGenericRegistrySetAndDeleteWithPerLocationFingerprints()
    {
        const RegistryLocation primary{
            .hive = RegistryHive::CurrentUser,
            .key = u"Software\\Tweakopedia"_s,
            .valueName = u"Primary"_s,
            .view = RegistryView::Registry64,
        };
        auto secondary = primary;
        secondary.valueName = u"Secondary"_s;
        TweakDefinition tweak;
        tweak.id = *TweakId::parse(u"devices.generic-registry"_s);
        tweak.title = u"Generic registry"_s;
        tweak.compatibility.architectures = {CpuArchitecture::X64};
        tweak.compatibility.operatingSystems = {WindowsFamily::Windows11};
        tweak.compatibility.minimumBuild = 22000;
        tweak.states = {
            {.id = u"old"_s, .title = u"Old"_s},
            {.id = u"new"_s, .title = u"New"_s,
             .operations = {
                 SetRegistryValueOperation{primary, RegistryValueSpec::qword(42)},
                 DeleteRegistryValueOperation{secondary},
             }},
        };
        auto detectedState = state(u"old"_s, QByteArray(64, 'a'));
        detectedState.registryFingerprints = {
            {.location = primary, .fingerprint = QByteArray(64, 'b')},
            {.location = secondary, .fingerprint = QByteArray(64, 'c')},
        };
        TweakQueue queue;
        QVERIFY(queue.setTarget(tweak, u"new"_s).accepted);

        const auto result = PlanBuilder{}.build(
            TweakCatalog({tweak}), queue, {{tweak.id, detectedState}}, profile());

        QVERIFY(result.plan.has_value());
        QCOMPARE(result.plan->operations.size(), 2);
        const auto& set = std::get<PlannedRegistryValueChange>(result.plan->operations[0]);
        QCOMPARE(set.beforeFingerprint, QByteArray(64, 'b'));
        QCOMPARE(std::get<SetRegistryValueOperation>(set.change).value,
                 RegistryValueSpec::qword(42));
        const auto& remove = std::get<PlannedRegistryValueChange>(result.plan->operations[1]);
        QCOMPARE(remove.beforeFingerprint, QByteArray(64, 'c'));
        QCOMPARE(std::get<DeleteRegistryValueOperation>(remove.change).location, secondary);
    }

    void substitutesTypedInputOnlyIntoOperationPayload()
    {
        const RegistryLocation location{
            .hive = RegistryHive::LocalMachine,
            .key = u"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\OEMInformation"_s,
            .valueName = u"Manufacturer"_s,
            .view = RegistryView::Registry64,
        };
        TweakDefinition tweak;
        tweak.id = *TweakId::parse(u"devices.oem-manufacturer"_s);
        tweak.title = u"Производитель"_s;
        tweak.compatibility.architectures = {CpuArchitecture::X64};
        tweak.compatibility.operatingSystems = {WindowsFamily::Windows11};
        tweak.compatibility.minimumBuild = 22000;
        tweak.inputs = {{
            .id = u"manufacturer"_s, .label = u"Производитель"_s,
            .type = TweakInputType::Text, .required = true,
        }};
        tweak.states = {
            {.id = u"cleared"_s, .title = u"Очищено"_s},
            {.id = u"configured"_s, .title = u"Настроено"_s,
             .operations = {SetRegistryValueOperation{
                 .location = location,
                 .value = RegistryValueSpec::string({}),
                 .valueInput = u"manufacturer"_s,
             }}},
        };
        auto current = state(u"cleared"_s, QByteArray(64, 'o'));
        current.registryFingerprints = {{location, QByteArray(64, 'o')}};
        TweakQueue queue;
        QVERIFY(queue.setTarget(
            tweak, u"configured"_s, {{u"manufacturer"_s, u"  ACME  "_s}}).accepted);

        const auto result = PlanBuilder{}.build(
            TweakCatalog({tweak}), queue, {{tweak.id, current}}, profile());

        QVERIFY(result.plan.has_value());
        const auto& planned = std::get<PlannedRegistryValueChange>(
            result.plan->operations.first());
        const auto& operation = std::get<SetRegistryValueOperation>(planned.change);
        QCOMPARE(operation.location, location);
        QCOMPARE(operation.value, RegistryValueSpec::string(u"ACME"));
        QVERIFY(!operation.valueInput.has_value());
        QCOMPARE(std::get<QString>(planned.inputs.value(u"manufacturer"_s)), u"ACME"_s);
    }

    void buildsFileOperationWithFixedDestinationAndSelectedSource()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto source = root.filePath(u"logo.bmp"_s);
        QFile file(source);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("BM"), 2);
        file.close();
        TweakDefinition tweak;
        tweak.id = *TweakId::parse(u"devices.oem-logo"_s);
        tweak.title = u"Логотип"_s;
        tweak.compatibility.architectures = {CpuArchitecture::X64};
        tweak.compatibility.operatingSystems = {WindowsFamily::Windows11};
        tweak.compatibility.minimumBuild = 22000;
        tweak.inputs = {{.id = u"logo"_s, .label = u"Логотип"_s,
                         .type = TweakInputType::File, .required = true,
                         .allowedExtensions = {u"bmp"_s}, .maximumFileSize = 1024}};
        const auto destination = QDir::cleanPath(root.filePath(u"target/oemlogo.bmp"_s));
        tweak.states = {
            {.id = u"cleared"_s, .title = u"Удалено"_s},
            {.id = u"configured"_s, .title = u"Настроено"_s,
             .operations = {FileOperationDefinition{
                 .kind = FileOperationKind::Replace,
                 .inputId = u"logo"_s,
                 .destination = destination,
             }}},
        };
        TweakQueue queue;
        QVERIFY(queue.setTarget(tweak, u"configured"_s, {{u"logo"_s, source}}).accepted);

        const auto result = PlanBuilder{}.build(
            TweakCatalog({tweak}), queue,
            {{tweak.id, state(u"cleared"_s, QByteArray(64, 'f'))}}, profile());

        QVERIFY(result.plan.has_value());
        const auto& operation = std::get<PlannedFileChange>(result.plan->operations.first());
        QCOMPARE(operation.change.destination, destination);
        QCOMPARE(operation.change.artifact.managedPath, QDir::cleanPath(source));
        QCOMPARE(operation.allowedExtensions, QStringList{u"bmp"_s});
        QCOMPARE(operation.maximumInputSize, quint64{1024});
        QCOMPARE(operation.beforeFingerprint.size(), 64);
    }

    void buildsRegistryTreeOperationAndRejectsOverlappingValueOperation()
    {
        const RegistryKeyLocation tree{
            .hive = RegistryHive::CurrentUser,
            .key = u"Software\\Tweakopedia\\Tree"_s,
            .view = RegistryView::Registry64,
        };
        TweakDefinition treeTweak;
        treeTweak.id = *TweakId::parse(u"devices.tree-delete"_s);
        treeTweak.title = u"Удалить ветвь"_s;
        treeTweak.compatibility.architectures = {CpuArchitecture::X64};
        treeTweak.compatibility.operatingSystems = {WindowsFamily::Windows11};
        treeTweak.compatibility.minimumBuild = 22000;
        treeTweak.states = {
            {.id = u"present"_s, .title = u"Есть"_s},
            {.id = u"absent"_s, .title = u"Нет"_s,
             .operations = {DeleteRegistryTreeOperation{tree}}},
        };

        auto valueTweak = definition(u"devices.value-inside-tree"_s, u"Value"_s);
        auto valueOperation = std::get<SetRegistryDwordOperation>(
            valueTweak.states[1].operations.first());
        valueOperation.location.hive = tree.hive;
        valueOperation.location.view = tree.view;
        valueOperation.location.key = tree.key + u"\\Child"_s;
        valueTweak.states[1].operations = {valueOperation};

        TweakQueue treeOnly;
        QVERIFY(treeOnly.setTarget(treeTweak, u"absent"_s).accepted);
        auto treeState = state(u"present"_s, QByteArray(64, 't'));
        treeState.registryTreeFingerprints = {
            {.location = tree, .fingerprint = QByteArray(64, 't')},
        };
        const auto built = PlanBuilder{}.build(
            TweakCatalog({treeTweak}), treeOnly,
            {{treeTweak.id, treeState}}, profile());
        QVERIFY(built.plan.has_value());
        QVERIFY(std::holds_alternative<PlannedRegistryTreeChange>(
            built.plan->operations.first()));

        TweakQueue overlapping;
        QVERIFY(overlapping.setTarget(treeTweak, u"absent"_s).accepted);
        QVERIFY(overlapping.setTarget(valueTweak, u"enabled"_s).accepted);
        const auto rejected = PlanBuilder{}.build(
            TweakCatalog({treeTweak, valueTweak}), overlapping,
            {{treeTweak.id, treeState},
             {valueTweak.id, state(u"disabled"_s, QByteArray(64, 'v'))}},
            profile());
        QVERIFY(!rejected.plan.has_value());
        QVERIFY(std::any_of(rejected.issues.cbegin(), rejected.issues.cend(),
                            [](const auto& issue) { return issue.code == u"registry.object_conflict"; }));
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

    void buildsTypedFeatureChangeWithBeforeFingerprint()
    {
        TweakDefinition tweak;
        tweak.id = *TweakId::parse(u"experimental.end-task"_s);
        tweak.title = u"Завершение задачи"_s;
        tweak.compatibility.architectures = {CpuArchitecture::X64};
        tweak.compatibility.operatingSystems = {WindowsFamily::Windows11};
        tweak.compatibility.minimumBuild = 22621;
        tweak.states = {{.id = u"enabled"_s, .title = u"Включено"_s,
                         .operations = {SetFeatureStateOperation{
                             42592269, FeatureEnabledState::Enabled}}}};
        TweakQueue queue;
        QVERIFY(queue.setTarget(tweak, u"enabled"_s).accepted);

        const auto result = PlanBuilder{}.build(
            TweakCatalog({tweak}), queue,
            {{tweak.id, state(u"disabled"_s, QByteArray(64, 'f'))}}, profile());

        QVERIFY(result.plan.has_value());
        QVERIFY(std::holds_alternative<PlannedFeatureStateChange>(
            result.plan->operations.first()));
        const auto& operation = std::get<PlannedFeatureStateChange>(
            result.plan->operations.first());
        QCOMPARE(operation.change.featureId, 42592269U);
        QCOMPARE(operation.change.state, FeatureEnabledState::Enabled);
        QCOMPARE(operation.beforeFingerprint, QByteArray(64, 'f'));
    }

    void buildsTypedScheduledTaskChangeWithBeforeFingerprint()
    {
        TweakDefinition tweak;
        tweak.id = *TweakId::parse(u"privacy.disable-telemetry-task"_s);
        tweak.title = u"Задача телеметрии Windows"_s;
        tweak.compatibility.architectures = {CpuArchitecture::X64};
        tweak.compatibility.operatingSystems = {WindowsFamily::Windows11};
        tweak.compatibility.minimumBuild = 22000;
        tweak.states = {{.id = u"disabled"_s, .title = u"Выключено"_s,
                         .operations = {SetScheduledTaskEnabledOperation{
                             {.folder = u"\\Microsoft\\Windows\\Application Experience"_s,
                              .name = u"Microsoft Compatibility Appraiser"_s}, false}}}};
        TweakQueue queue;
        QVERIFY(queue.setTarget(tweak, u"disabled"_s).accepted);

        const auto result = PlanBuilder{}.build(
            TweakCatalog({tweak}), queue,
            {{tweak.id, state(u"enabled"_s, QByteArray(64, 't'))}}, profile());

        QVERIFY(result.plan.has_value());
        const auto& operation = std::get<PlannedScheduledTaskChange>(
            result.plan->operations.first());
        QCOMPARE(operation.change.location.name, u"Microsoft Compatibility Appraiser"_s);
        QVERIFY(!operation.change.enabled);
        QCOMPARE(operation.beforeFingerprint, QByteArray(64, 't'));
    }

    void buildsTypedBcdElementChangeWithBeforeFingerprint()
    {
        TweakDefinition tweak;
        tweak.id = *TweakId::parse(u"boot.dynamic-tick"_s);
        tweak.title = u"Динамический системный таймер"_s;
        tweak.compatibility.architectures = {CpuArchitecture::X64};
        tweak.compatibility.operatingSystems = {WindowsFamily::Windows11};
        tweak.compatibility.minimumBuild = 22000;
        const BcdElementSpec spec{u"{current}"_s, 0x260000A5,
                                  BcdValueKind::Boolean};
        tweak.states = {{.id = u"disabled"_s, .title = u"Выключено"_s,
                         .operations = {SetBcdElementOperation{
                             .spec = spec, .value = BcdValue{true}}}}};
        TweakQueue queue;
        QVERIFY(queue.setTarget(tweak, u"disabled"_s).accepted);

        const auto result = PlanBuilder{}.build(
            TweakCatalog({tweak}), queue,
            {{tweak.id, state(u"default"_s, QByteArray(64, 'b'))}}, profile());

        QVERIFY(result.plan.has_value());
        const auto& operation = std::get<PlannedBcdElementChange>(
            result.plan->operations.first());
        QCOMPARE(operation.change.spec, spec);
        QCOMPARE(std::get<bool>(*operation.change.value), true);
        QCOMPARE(operation.beforeFingerprint, QByteArray(64, 'b'));
    }

    void buildsTypedPowerSettingChangeWithBeforeFingerprint()
    {
        TweakDefinition tweak;
        tweak.id = *TweakId::parse(u"power.processor-minimum-ac"_s);
        tweak.title = u"Минимальное состояние процессора"_s;
        tweak.compatibility.architectures = {CpuArchitecture::X64};
        tweak.compatibility.operatingSystems = {WindowsFamily::Windows11};
        tweak.compatibility.minimumBuild = 22000;
        const PowerSettingLocation location{u"active"_s,
            u"54533251-82be-4824-96c1-47b60b740d00"_s,
            u"893dee8e-2bef-41e0-89c6-b55d0929964c"_s, PowerSource::Ac};
        tweak.states = {{.id = u"maximum"_s, .title = u"Максимум"_s,
                         .operations = {SetPowerSettingOperation{location, 100}}}};
        TweakQueue queue;
        QVERIFY(queue.setTarget(tweak, u"maximum"_s).accepted);
        const auto result = PlanBuilder{}.build(TweakCatalog({tweak}), queue,
            {{tweak.id, state(u"balanced"_s, QByteArray(64, 'p'))}}, profile());
        QVERIFY(result.plan.has_value());
        const auto& operation = std::get<PlannedPowerSettingChange>(result.plan->operations.first());
        QCOMPARE(operation.change.location, location);
        QCOMPARE(operation.change.index, 100U);
    }

    void buildsTypedWindowsComponentChangeWithBeforeFingerprint()
    {
        TweakDefinition tweak;
        tweak.id = *TweakId::parse(u"components.virtual-machine-platform"_s);
        tweak.title = u"Платформа виртуальной машины"_s;
        tweak.compatibility.architectures = {CpuArchitecture::X64};
        tweak.compatibility.operatingSystems = {WindowsFamily::Windows11};
        tweak.compatibility.minimumBuild = 22000;
        const WindowsComponentTarget target{
            WindowsComponentKind::Feature, u"VirtualMachinePlatform"_s};
        tweak.states = {{.id = u"enabled"_s, .title = u"Включено"_s,
                         .operations = {SetWindowsComponentStateOperation{
                             target, WindowsComponentState::Enabled}}}};
        TweakQueue queue;
        QVERIFY(queue.setTarget(tweak, u"enabled"_s).accepted);
        const auto result = PlanBuilder{}.build(TweakCatalog({tweak}), queue,
            {{tweak.id, state(u"disabled"_s, QByteArray(64, 'c'))}}, profile());
        QVERIFY(result.plan.has_value());
        const auto& operation = std::get<PlannedWindowsComponentChange>(
            result.plan->operations.first());
        QCOMPARE(operation.change.target, target);
        QCOMPARE(operation.change.state, WindowsComponentState::Enabled);
        QCOMPARE(operation.beforeFingerprint, QByteArray(64, 'c'));
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
