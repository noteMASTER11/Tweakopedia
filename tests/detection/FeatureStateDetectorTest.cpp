#include "detection/FeatureStateDetector.h"

#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

class FakeFeatureStore final : public platform::IFeatureStoreBackend
{
public:
    platform::FeatureQueryResult result;
    platform::FeatureQueryResult query(quint32) const override { return result; }
    platform::FeatureMutationResult setUserState(
        quint32, domain::FeatureEnabledState) override { return {.success = true}; }
    platform::FeatureMutationResult setUserConfiguration(
        const platform::FeatureConfiguration&) override { return {.success = true}; }
    platform::FeatureMutationResult resetUserConfiguration(quint32) override
    { return {.success = true}; }
};

domain::TweakDefinition tweak()
{
    domain::TweakDefinition result;
    result.id = *domain::TweakId::parse(u"experimental.end-task"_s);
    result.title = u"Завершение задачи"_s;
    result.category = u"experimental"_s;
    result.subcategory = u"feature-store"_s;
    result.summary = u"Проверка функции"_s;
    result.explanation = {u"Назначение"_s, u"Механизм"_s, u"Эффект"_s,
                          u"Ограничения"_s, u"Рекомендация"_s, u"Детали"_s};
    result.compatibility = {{domain::CpuArchitecture::X64},
                            {domain::WindowsFamily::Windows11}, 22621, {}, {}};
    result.featureDetection = domain::FeatureStateDetection{42592269};
    return result;
}

domain::SystemProfile profile()
{
    return {.family = domain::WindowsFamily::Windows11, .build = 26200,
            .architecture = domain::CpuArchitecture::X64};
}

} // namespace

class FeatureStateDetectorTest final : public QObject
{
    Q_OBJECT
private slots:
    void reportsEffectiveStateAndStableFingerprint()
    {
        FakeFeatureStore backend;
        backend.result = {.success = true,
                          .configuration = platform::FeatureConfiguration{
                              .featureId = 42592269, .priority = 8,
                              .state = domain::FeatureEnabledState::Enabled}};
        const auto result = detection::FeatureStateDetector{}.detect(tweak(), backend, profile());
        QCOMPARE(result.status, domain::DetectionStatus::Named);
        QCOMPARE(result.stateId, u"enabled"_s);
        QCOMPARE(result.fingerprint.size(), 64);
    }

    void missingFeatureIsUnsupported()
    {
        FakeFeatureStore backend;
        backend.result = {.success = false, .error = u"Функция не найдена."_s};
        const auto result = detection::FeatureStateDetector{}.detect(tweak(), backend, profile());
        QCOMPARE(result.status, domain::DetectionStatus::Unsupported);
    }

    void nonUserConfigurationIsReportedAsWindowsDefault()
    {
        FakeFeatureStore backend;
        backend.result = {.success = true,
                          .configuration = platform::FeatureConfiguration{
                              .featureId = 42592269, .priority = 4,
                              .state = domain::FeatureEnabledState::Enabled}};
        const auto result = detection::FeatureStateDetector{}.detect(tweak(), backend, profile());
        QCOMPARE(result.stateId, u"default"_s);
        QVERIFY(result.details.contains(u"enabled"_s));
    }
};

QTEST_APPLESS_MAIN(FeatureStateDetectorTest)
#include "FeatureStateDetectorTest.moc"
