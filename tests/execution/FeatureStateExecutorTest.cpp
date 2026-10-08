#include "execution/FeatureStateExecutor.h"

#include <QtTest/QTest>

using namespace tweakopedia;

namespace {

class FakeFeatureStore final : public platform::IFeatureStoreBackend
{
public:
    platform::FeatureConfiguration current{.featureId = 42592269, .priority = 4,
        .state = domain::FeatureEnabledState::Disabled};

    platform::FeatureQueryResult query(quint32 featureId) const override
    {
        return featureId == current.featureId
            ? platform::FeatureQueryResult{.success = true, .configuration = current}
            : platform::FeatureQueryResult{.success = false};
    }
    platform::FeatureMutationResult setUserState(
        quint32 featureId, domain::FeatureEnabledState state) override
    {
        current.featureId = featureId;
        current.priority = 8;
        current.state = state;
        return {.success = true};
    }
    platform::FeatureMutationResult setUserConfiguration(
        const platform::FeatureConfiguration& configuration) override
    {
        current = configuration;
        return {.success = true};
    }
    platform::FeatureMutationResult resetUserConfiguration(quint32) override
    {
        current.priority = 4;
        current.state = domain::FeatureEnabledState::Disabled;
        return {.success = true};
    }
};

} // namespace

class FeatureStateExecutorTest final : public QObject
{
    Q_OBJECT
private slots:
    void appliesVerifiesAndRestoresUserOverride()
    {
        FakeFeatureStore backend;
        execution::FeatureStateExecutor executor(backend);
        const auto before = executor.capture(42592269);
        QVERIFY(before.success);
        QVERIFY(executor.compareBefore(before.snapshot, before.snapshot.fingerprint()).success);
        QVERIFY(executor.apply({42592269, domain::FeatureEnabledState::Enabled}).success);
        QCOMPARE(backend.current.priority, 8U);
        QCOMPARE(backend.current.state, domain::FeatureEnabledState::Enabled);
        QVERIFY(executor.restore(before.snapshot).success);
        QCOMPARE(backend.current.priority, 4U);
    }

    void snapshotRoundTrips()
    {
        execution::FeatureSnapshot snapshot{platform::FeatureConfiguration{
            .featureId = 42592269, .priority = 8,
            .state = domain::FeatureEnabledState::Enabled,
            .variant = 3, .variantPayload = 9}};
        const auto decoded = execution::FeatureSnapshot::fromJson(snapshot.toJson());
        QVERIFY(decoded.has_value());
        QCOMPARE(decoded->configuration, snapshot.configuration);
    }
};

QTEST_APPLESS_MAIN(FeatureStateExecutorTest)
#include "FeatureStateExecutorTest.moc"
