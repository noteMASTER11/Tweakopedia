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
            registry.insert(u"hive"_s, u"HKCR"_s);
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
