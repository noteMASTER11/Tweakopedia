#include "execution/ExecutionProtocol.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest/QTest>

using namespace tweakopedia::domain;
using namespace tweakopedia::execution;
using namespace tweakopedia::planning;
using namespace Qt::StringLiterals;

namespace {

ExecutionPlan plan()
{
    const auto location = RegistryLocation{
        .hive = RegistryHive::LocalMachine,
        .key = u"SYSTEM\\CurrentControlSet\\Control\\FileSystem"_s,
        .valueName = u"LongPathsEnabled"_s,
        .view = RegistryView::Registry64,
    };
    return {
        .schemaVersion = executionPlanSchemaVersion,
        .transactionId = QUuid(u"{11111111-2222-3333-4444-555555555555}"_s),
        .profile = {
            .family = WindowsFamily::Windows11,
            .build = 26200,
            .ubr = 9457,
            .edition = u"Professional"_s,
            .architecture = CpuArchitecture::X64,
        },
        .createdAtUtc = QDateTime::fromString(u"2026-10-08T08:00:00.000Z"_s, Qt::ISODateWithMs),
        .operations = {PlannedRegistryDwordChange{
            .tweakId = *TweakId::parse(u"filesystem.win32-long-paths"),
            .targetState = u"enabled"_s,
            .change = SetRegistryDwordOperation{.location = location, .value = 1},
            .beforeFingerprint = QByteArray(64, 'a'),
            .restart = RestartRequirement::None,
        }},
        .summary = u"Будет применено операций: 1."_s,
        .restart = RestartRequirement::None,
    };
}

QByteArray mutateBody(
    const QByteArray& encoded,
    const std::function<void(QJsonObject&)>& mutation,
    bool updateHash = true)
{
    auto envelope = QJsonDocument::fromJson(encoded).object();
    auto body = envelope.value(u"body"_s).toObject();
    mutation(body);
    envelope.insert(u"body"_s, body);
    if (updateHash) {
        envelope.insert(u"sha256"_s, QString::fromLatin1(ExecutionProtocol::canonicalHash(body)));
    }
    return QJsonDocument(envelope).toJson(QJsonDocument::Compact);
}

bool hasError(const ProtocolDecodeResult& result, QStringView code)
{
    return std::any_of(result.errors.cbegin(), result.errors.cend(), [code](const ProtocolError& error) {
        return error.code == code;
    });
}

} // namespace

class ExecutionProtocolTest final : public QObject
{
    Q_OBJECT

private slots:
    void roundTripsWhitelistedPlan()
    {
        const auto original = plan();
        const auto encoded = ExecutionProtocol::encode(original);
        const auto decoded = ExecutionProtocol::decode(encoded);

        QVERIFY(decoded.errors.isEmpty());
        QVERIFY(decoded.plan.has_value());
        QCOMPARE(decoded.plan->transactionId, original.transactionId);
        QCOMPARE(decoded.plan->operations.size(), 1);
        const auto& operation = std::get<PlannedRegistryDwordChange>(decoded.plan->operations.first());
        QCOMPARE(operation.change.location.key, u"SYSTEM\\CurrentControlSet\\Control\\FileSystem"_s);
        QCOMPARE(operation.change.value, quint32{1});
        QCOMPARE(operation.beforeFingerprint, QByteArray(64, 'a'));
    }

    void roundTripsTypedInputsWithoutJsonNumberLoss()
    {
        auto original = plan();
        auto& operation = std::get<PlannedRegistryDwordChange>(original.operations.first());
        operation.inputs = {
            {u"name"_s, QString{u"ACME"_s}},
            {u"count"_s, qint64{9007199254740993LL}},
            {u"active"_s, true},
        };

        const auto decoded = ExecutionProtocol::decode(ExecutionProtocol::encode(original));

        QVERIFY(decoded.errors.isEmpty());
        const auto& restored = std::get<PlannedRegistryDwordChange>(
            decoded.plan->operations.first());
        QCOMPARE(std::get<QString>(restored.inputs.value(u"name"_s)), u"ACME"_s);
        QCOMPARE(std::get<qint64>(restored.inputs.value(u"count"_s)), 9007199254740993LL);
        QVERIFY(std::get<bool>(restored.inputs.value(u"active"_s)));
    }

    void roundTripsWhitelistedAppxRemoval()
    {
        auto original = plan();
        original.operations = {PlannedAppxRemoval{
            .tweakId = *TweakId::parse(u"apps.remove.clipchamp.clipchamp"_s),
            .targetState = u"remove"_s,
            .change = RemoveAppxPackageOperation{u"Clipchamp.Clipchamp"_s},
            .beforeFingerprint = QByteArray(64, 'c'),
        }};

        const auto decoded = ExecutionProtocol::decode(ExecutionProtocol::encode(original));

        QVERIFY2(decoded.errors.isEmpty(), qPrintable(
            decoded.errors.isEmpty() ? QString{} : decoded.errors.first().message));
        QVERIFY(decoded.plan.has_value());
        QVERIFY(std::holds_alternative<PlannedAppxRemoval>(decoded.plan->operations.first()));
        const auto& operation = std::get<PlannedAppxRemoval>(decoded.plan->operations.first());
        QCOMPARE(operation.change.packageName, u"Clipchamp.Clipchamp"_s);
        QCOMPARE(operation.beforeFingerprint, QByteArray(64, 'c'));
    }

    void roundTripsGenericRegistrySetAndDelete()
    {
        auto original = plan();
        const RegistryLocation setLocation{
            .hive = RegistryHive::ClassesRoot,
            .key = u"Applications\\Tweakopedia.exe"_s,
            .valueName = u"FriendlyAppName"_s,
            .view = RegistryView::Registry64,
        };
        auto deleteLocation = setLocation;
        deleteLocation.valueName = u"LegacyValue"_s;
        original.operations = {
            PlannedRegistryValueChange{
                .tweakId = *TweakId::parse(u"devices.registry-set"_s),
                .targetState = u"enabled"_s,
                .change = SetRegistryValueOperation{
                    setLocation, RegistryValueSpec::expandString(u"%ProgramFiles%\\Tweakopedia")},
                .beforeFingerprint = QByteArray(64, 'd'),
            },
            PlannedRegistryValueChange{
                .tweakId = *TweakId::parse(u"devices.registry-delete"_s),
                .targetState = u"disabled"_s,
                .change = DeleteRegistryValueOperation{deleteLocation},
                .beforeFingerprint = QByteArray(64, 'e'),
            },
        };

        const auto encoded = ExecutionProtocol::encode(original);
        const auto body = QJsonDocument::fromJson(encoded).object().value(u"body"_s).toObject();
        const auto jsonOperations = body.value(u"operations"_s).toArray();
        QCOMPARE(jsonOperations[0].toObject().value(u"type"_s).toString(), u"registry.set_value"_s);
        QCOMPARE(jsonOperations[1].toObject().value(u"type"_s).toString(), u"registry.delete_value"_s);

        const auto decoded = ExecutionProtocol::decode(encoded);
        QVERIFY2(decoded.errors.isEmpty(), qPrintable(
            decoded.errors.isEmpty() ? QString{} : decoded.errors.first().message));
        QVERIFY(decoded.plan.has_value());
        const auto& set = std::get<PlannedRegistryValueChange>(decoded.plan->operations[0]);
        const auto& setChange = std::get<SetRegistryValueOperation>(set.change);
        QCOMPARE(setChange.location.hive, RegistryHive::ClassesRoot);
        QCOMPARE(setChange.value, RegistryValueSpec::expandString(u"%ProgramFiles%\\Tweakopedia"));
        const auto& remove = std::get<PlannedRegistryValueChange>(decoded.plan->operations[1]);
        QCOMPARE(std::get<DeleteRegistryValueOperation>(remove.change).location, deleteLocation);
    }

    void roundTripsRegistryTreeOperations()
    {
        auto original = plan();
        const RegistryKeyLocation target{
            .hive = RegistryHive::Users,
            .key = u".DEFAULT\\Software\\Tweakopedia"_s,
            .view = RegistryView::Registry64,
        };
        original.operations = {
            PlannedRegistryTreeChange{
                .tweakId = *TweakId::parse(u"devices.registry-tree-create"_s),
                .targetState = u"enabled"_s,
                .change = CreateRegistryKeyOperation{target},
                .beforeFingerprint = QByteArray(64, 'a'),
            },
            PlannedRegistryTreeChange{
                .tweakId = *TweakId::parse(u"devices.registry-tree-delete"_s),
                .targetState = u"disabled"_s,
                .change = DeleteRegistryTreeOperation{target},
                .beforeFingerprint = QByteArray(64, 'b'),
            },
        };

        const auto encoded = ExecutionProtocol::encode(original);
        const auto decoded = ExecutionProtocol::decode(encoded);

        QVERIFY(decoded.errors.isEmpty());
        QVERIFY(decoded.plan.has_value());
        QCOMPARE(std::get<CreateRegistryKeyOperation>(
            std::get<PlannedRegistryTreeChange>(decoded.plan->operations[0]).change).location,
            target);
        QCOMPARE(std::get<DeleteRegistryTreeOperation>(
            std::get<PlannedRegistryTreeChange>(decoded.plan->operations[1]).change).location,
            target);
    }

    void roundTripsTransactionRelativeFileOperation()
    {
        auto original = plan();
        const InputArtifact artifact{
            .id = u"logo"_s,
            .storageId = u"artifact-1"_s,
            .managedPath = u"inputs/artifact-1/logo.bmp"_s,
            .size = 4,
            .sha256 = QByteArray(64, 'a'),
        };
        original.operations = {PlannedFileChange{
            .tweakId = *TweakId::parse(u"devices.oem-logo"_s),
            .targetState = u"configured"_s,
            .change = FileOperation{
                .kind = FileOperationKind::Replace,
                .artifact = artifact,
                .destination = u"C:/Windows/System32/oemlogo.bmp"_s,
            },
            .beforeFingerprint = QByteArray(64, 'b'),
            .inputs = {{u"logo"_s, artifact}},
        }};

        const auto decoded = ExecutionProtocol::decode(ExecutionProtocol::encode(original));

        QVERIFY2(decoded.errors.isEmpty(), qPrintable(
            decoded.errors.isEmpty() ? QString{} : decoded.errors.first().message));
        const auto& operation = std::get<PlannedFileChange>(decoded.plan->operations.first());
        QCOMPARE(operation.change.kind, FileOperationKind::Replace);
        QCOMPARE(operation.change.artifact, artifact);
        QCOMPARE(operation.change.destination, u"C:/Windows/System32/oemlogo.bmp"_s);
        QCOMPARE(std::get<InputArtifact>(operation.inputs.value(u"logo"_s)), artifact);
    }

    void rejectsAppxWildcard()
    {
        auto original = plan();
        original.operations = {PlannedAppxRemoval{
            .tweakId = *TweakId::parse(u"apps.remove.clipchamp.clipchamp"_s),
            .targetState = u"remove"_s,
            .change = RemoveAppxPackageOperation{u"*Clipchamp*"_s},
            .beforeFingerprint = QByteArray(64, 'c'),
        }};

        const auto decoded = ExecutionProtocol::decode(ExecutionProtocol::encode(original));

        QVERIFY(!decoded.plan.has_value());
        QVERIFY(hasError(decoded, u"appx.package_name_invalid"));
    }

    void roundTripsWhitelistedFeatureChange()
    {
        auto original = plan();
        original.operations = {PlannedFeatureStateChange{
            .tweakId = *TweakId::parse(u"experimental.end-task"_s),
            .targetState = u"enabled"_s,
            .change = SetFeatureStateOperation{42592269, FeatureEnabledState::Enabled},
            .beforeFingerprint = QByteArray(64, 'f'),
            .restart = RestartRequirement::Explorer,
        }};

        const auto decoded = ExecutionProtocol::decode(ExecutionProtocol::encode(original));

        QVERIFY(decoded.errors.isEmpty());
        QVERIFY(decoded.plan.has_value());
        const auto& operation = std::get<PlannedFeatureStateChange>(
            decoded.plan->operations.first());
        QCOMPARE(operation.change.featureId, 42592269U);
        QCOMPARE(operation.change.state, FeatureEnabledState::Enabled);
        QCOMPARE(operation.beforeFingerprint, QByteArray(64, 'f'));
    }

    void roundTripsWhitelistedScheduledTaskChange()
    {
        auto original = plan();
        original.operations = {PlannedScheduledTaskChange{
            .tweakId = *TweakId::parse(u"privacy.disable-telemetry-task"_s),
            .targetState = u"disabled"_s,
            .change = SetScheduledTaskEnabledOperation{
                {.folder = u"\\Microsoft\\Windows\\Application Experience"_s,
                 .name = u"Microsoft Compatibility Appraiser"_s}, false},
            .beforeFingerprint = QByteArray(64, 't'),
        }};

        const auto decoded = ExecutionProtocol::decode(ExecutionProtocol::encode(original));

        QVERIFY(decoded.errors.isEmpty());
        QVERIFY(decoded.plan.has_value());
        const auto& operation = std::get<PlannedScheduledTaskChange>(
            decoded.plan->operations.first());
        QCOMPARE(operation.change.location, std::get<PlannedScheduledTaskChange>(
            original.operations.first()).change.location);
        QVERIFY(!operation.change.enabled);
    }

    void roundTripsWhitelistedBcdElementChange()
    {
        auto original = plan();
        original.operations = {PlannedBcdElementChange{
            .tweakId = *TweakId::parse(u"boot.dynamic-tick"_s),
            .targetState = u"disabled"_s,
            .change = {.spec = {u"{current}"_s, 0x260000A5,
                                BcdValueKind::Boolean},
                       .value = BcdValue{true}},
            .beforeFingerprint = QByteArray(64, 'b'),
            .restart = RestartRequirement::Reboot,
        }};

        const auto decoded = ExecutionProtocol::decode(ExecutionProtocol::encode(original));

        QVERIFY(decoded.errors.isEmpty());
        QVERIFY(decoded.plan.has_value());
        const auto& operation = std::get<PlannedBcdElementChange>(
            decoded.plan->operations.first());
        QCOMPARE(operation.change.spec.elementType, 0x260000A5U);
        QCOMPARE(std::get<bool>(*operation.change.value), true);
    }

    void roundTripsWhitelistedPowerSettingChange()
    {
        auto original = plan();
        original.operations = {PlannedPowerSettingChange{
            .tweakId = *TweakId::parse(u"power.processor-minimum-ac"_s),
            .targetState = u"maximum"_s,
            .change = {{u"active"_s, u"54533251-82be-4824-96c1-47b60b740d00"_s,
                        u"893dee8e-2bef-41e0-89c6-b55d0929964c"_s, PowerSource::Ac}, 100},
            .beforeFingerprint = QByteArray(64, 'p')}};
        const auto decoded = ExecutionProtocol::decode(ExecutionProtocol::encode(original));
        QVERIFY(decoded.errors.isEmpty());
        const auto& operation = std::get<PlannedPowerSettingChange>(decoded.plan->operations.first());
        QCOMPARE(operation.change.index, 100U);
        QCOMPARE(operation.change.location.source, PowerSource::Ac);
    }

    void roundTripsWhitelistedWindowsComponentChange()
    {
        auto original = plan();
        original.operations = {PlannedWindowsComponentChange{
            .tweakId = *TweakId::parse(u"components.virtual-machine-platform"_s),
            .targetState = u"enabled"_s,
            .change = {{WindowsComponentKind::Feature, u"VirtualMachinePlatform"_s},
                       WindowsComponentState::Enabled},
            .beforeFingerprint = QByteArray(64, 'c'),
            .restart = RestartRequirement::Reboot}};
        const auto decoded = ExecutionProtocol::decode(ExecutionProtocol::encode(original));
        QVERIFY(decoded.errors.isEmpty());
        const auto& operation = std::get<PlannedWindowsComponentChange>(
            decoded.plan->operations.first());
        QCOMPARE(operation.change.target.name, u"VirtualMachinePlatform"_s);
        QCOMPARE(operation.change.state, WindowsComponentState::Enabled);
    }

    void producesStableJsonForSamePlan()
    {
        const auto value = plan();

        QCOMPARE(ExecutionProtocol::encode(value), ExecutionProtocol::encode(value));
    }

    void rejectsUnknownSchemaVersion()
    {
        const auto encoded = mutateBody(ExecutionProtocol::encode(plan()), [](QJsonObject& body) {
            body.insert(u"schema_version"_s, 99);
        });

        const auto decoded = ExecutionProtocol::decode(encoded);
        QVERIFY(!decoded.plan.has_value());
        QVERIFY(hasError(decoded, u"schema.unsupported"));
    }

    void rejectsUnknownOperationType()
    {
        const auto encoded = mutateBody(ExecutionProtocol::encode(plan()), [](QJsonObject& body) {
            auto operations = body.value(u"operations"_s).toArray();
            auto operation = operations[0].toObject();
            operation.insert(u"type"_s, u"shell"_s);
            operations[0] = operation;
            body.insert(u"operations"_s, operations);
        });

        const auto decoded = ExecutionProtocol::decode(encoded);
        QVERIFY(hasError(decoded, u"operation.unknown"));
    }

    void rejectsExtraField()
    {
        const auto encoded = mutateBody(ExecutionProtocol::encode(plan()), [](QJsonObject& body) {
            body.insert(u"extra"_s, true);
        });

        const auto decoded = ExecutionProtocol::decode(encoded);
        QVERIFY(hasError(decoded, u"field.unknown"));
    }

    void rejectsRegistryHiveOutsideWhitelist()
    {
        const auto encoded = mutateBody(ExecutionProtocol::encode(plan()), [](QJsonObject& body) {
            auto operations = body.value(u"operations"_s).toArray();
            auto operation = operations[0].toObject();
            auto registry = operation.value(u"registry"_s).toObject();
            registry.insert(u"hive"_s, u"HKCC"_s);
            operation.insert(u"registry"_s, registry);
            operations[0] = operation;
            body.insert(u"operations"_s, operations);
        });

        const auto decoded = ExecutionProtocol::decode(encoded);
        QVERIFY(hasError(decoded, u"registry.hive_unknown"));
    }

    void detectsTamperedFingerprint()
    {
        const auto encoded = mutateBody(ExecutionProtocol::encode(plan()), [](QJsonObject& body) {
            auto operations = body.value(u"operations"_s).toArray();
            auto operation = operations[0].toObject();
            operation.insert(u"before_fingerprint"_s, QString(64, u'b'));
            operations[0] = operation;
            body.insert(u"operations"_s, operations);
        }, false);

        const auto decoded = ExecutionProtocol::decode(encoded);
        QVERIFY(hasError(decoded, u"hash.mismatch"));
    }

    void rejectsCommandFieldEvenForKnownOperation()
    {
        const auto encoded = mutateBody(ExecutionProtocol::encode(plan()), [](QJsonObject& body) {
            auto operations = body.value(u"operations"_s).toArray();
            auto operation = operations[0].toObject();
            operation.insert(u"command"_s, u"cmd /c whoami"_s);
            operations[0] = operation;
            body.insert(u"operations"_s, operations);
        });

        const auto decoded = ExecutionProtocol::decode(encoded);
        QVERIFY(hasError(decoded, u"field.unknown"));
    }
};

QTEST_APPLESS_MAIN(ExecutionProtocolTest)

#include "ExecutionProtocolTest.moc"
