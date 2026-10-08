#include "execution/ExecutionProtocol.h"
#include "execution/PlanValidator.h"

#include <QtTest/QTest>

using namespace tweakopedia::domain;
using namespace tweakopedia::execution;
using namespace tweakopedia::planning;
using namespace Qt::StringLiterals;

namespace {

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

ExecutionPlan plan(const QDateTime& createdAt)
{
    return {
        .schemaVersion = executionPlanSchemaVersion,
        .transactionId = QUuid(u"{11111111-2222-3333-4444-555555555555}"_s),
        .profile = profile(),
        .createdAtUtc = createdAt,
        .operations = {PlannedRegistryDwordChange{
            .tweakId = *TweakId::parse(u"filesystem.win32-long-paths"),
            .targetState = u"enabled"_s,
            .change = SetRegistryDwordOperation{
                .location = {
                    .hive = RegistryHive::LocalMachine,
                    .key = u"SYSTEM\\CurrentControlSet\\Control\\FileSystem"_s,
                    .valueName = u"LongPathsEnabled"_s,
                    .view = RegistryView::Registry64,
                },
                .value = 1,
            },
            .beforeFingerprint = QByteArray(64, 'a'),
        }},
        .summary = u"Будет применено операций: 1."_s,
    };
}

} // namespace

class PlanValidatorTest final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsFreshMatchingPlan()
    {
        const auto now = QDateTime::fromString(u"2026-10-08T08:04:00.000Z"_s, Qt::ISODateWithMs);
        const auto candidate = plan(now.addSecs(-60));
        const auto hash = ExecutionProtocol::bodyHash(candidate);

        const auto result = PlanValidator{}.validate(candidate, profile(), now, hash, hash);

        QVERIFY(result.accepted);
        QVERIFY(result.errors.isEmpty());
    }

    void acceptsTypedAppxRemoval()
    {
        const auto now = QDateTime::currentDateTimeUtc();
        auto candidate = plan(now);
        candidate.operations = {PlannedAppxRemoval{
            .tweakId = *TweakId::parse(u"apps.remove.clipchamp.clipchamp"_s),
            .targetState = u"remove"_s,
            .change = RemoveAppxPackageOperation{u"Clipchamp.Clipchamp"_s},
            .beforeFingerprint = QByteArray(64, 'c'),
        }};
        const auto hash = ExecutionProtocol::bodyHash(candidate);

        const auto result = PlanValidator{}.validate(candidate, profile(), now, hash, hash);

        QVERIFY(result.accepted);
        QVERIFY(result.errors.isEmpty());
    }

    void rejectsChangedSystemProfile()
    {
        const auto now = QDateTime::currentDateTimeUtc();
        const auto candidate = plan(now);
        auto changed = profile();
        changed.build += 1;
        const auto hash = ExecutionProtocol::bodyHash(candidate);

        const auto result = PlanValidator{}.validate(candidate, changed, now, hash, hash);

        QVERIFY(!result.accepted);
        QVERIFY(result.hasError(u"profile.changed"));
    }

    void rejectsNullTransactionId()
    {
        const auto now = QDateTime::currentDateTimeUtc();
        auto candidate = plan(now);
        candidate.transactionId = QUuid{};
        const auto hash = ExecutionProtocol::bodyHash(candidate);

        const auto result = PlanValidator{}.validate(candidate, profile(), now, hash, hash);

        QVERIFY(result.hasError(u"transaction.invalid_id"));
    }

    void rejectsEmptyPlan()
    {
        const auto now = QDateTime::currentDateTimeUtc();
        auto candidate = plan(now);
        candidate.operations.clear();
        const auto hash = ExecutionProtocol::bodyHash(candidate);

        const auto result = PlanValidator{}.validate(candidate, profile(), now, hash, hash);

        QVERIFY(!result.accepted);
        QVERIFY(result.hasError(u"operations.empty"));
    }

    void rejectsExpiredPlan()
    {
        const auto now = QDateTime::currentDateTimeUtc();
        const auto candidate = plan(now.addSecs(-301));
        const auto hash = ExecutionProtocol::bodyHash(candidate);

        const auto result = PlanValidator{}.validate(candidate, profile(), now, hash, hash);

        QVERIFY(result.hasError(u"plan.expired"));
    }

    void rejectsUnexpectedCanonicalHash()
    {
        const auto now = QDateTime::currentDateTimeUtc();
        const auto candidate = plan(now);
        const auto hash = ExecutionProtocol::bodyHash(candidate);

        const auto result = PlanValidator{}.validate(candidate, profile(), now, QByteArray(64, 'b'), hash);

        QVERIFY(result.hasError(u"hash.mismatch"));
    }
};

QTEST_APPLESS_MAIN(PlanValidatorTest)

#include "PlanValidatorTest.moc"
