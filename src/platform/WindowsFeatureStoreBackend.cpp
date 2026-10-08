#include "platform/WindowsFeatureStoreBackend.h"

#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

using namespace Qt::StringLiterals;

namespace tweakopedia::platform {
namespace {

#ifdef Q_OS_WIN
struct NativeFeatureConfiguration {
    quint32 featureId;
    quint32 compactState;
    quint32 variantPayload;
};

struct NativeFeatureConfigurationUpdate {
    quint32 featureId;
    quint32 priority;
    quint32 enabledState;
    quint32 enabledStateOptions;
    quint32 variant;
    quint32 variantPayloadKind;
    quint32 variantPayload;
    quint32 operation;
};

static_assert(sizeof(NativeFeatureConfiguration) == 12);
static_assert(sizeof(NativeFeatureConfigurationUpdate) == 32);

using QueryFeature = LONG (NTAPI *)(
    quint32, quint32, quint64*, NativeFeatureConfiguration*);
using SetFeatures = LONG (NTAPI *)(
    quint64*, quint32, NativeFeatureConfigurationUpdate*, int);

template<typename Function>
Function resolve(const char* name)
{
    const auto module = GetModuleHandleW(L"ntdll.dll");
    return module ? reinterpret_cast<Function>(GetProcAddress(module, name)) : nullptr;
}

QString statusText(QStringView operation, LONG status)
{
    return u"%1: NTSTATUS 0x%2"_s.arg(operation)
        .arg(static_cast<quint32>(status), 8, 16, QLatin1Char('0'));
}

FeatureMutationResult update(NativeFeatureConfigurationUpdate value)
{
    const auto function = resolve<SetFeatures>("RtlSetFeatureConfigurations");
    if (!function) {
        return {.success = false, .error = u"RtlSetFeatureConfigurations недоступна."_s};
    }
    quint64 changeStamp{};
    constexpr quint32 runtimeStore = 1;
    const auto status = function(&changeStamp, runtimeStore, &value, 1);
    return status == 0
        ? FeatureMutationResult{.success = true}
        : FeatureMutationResult{.success = false,
                                .error = statusText(u"Изменение Feature Store"_s, status)};
}
#endif

} // namespace

FeatureQueryResult WindowsFeatureStoreBackend::query(quint32 featureId) const
{
#ifdef Q_OS_WIN
    if (featureId == 0) return {.success = false, .error = u"Feature ID равен нулю."_s};
    const auto function = resolve<QueryFeature>("RtlQueryFeatureConfiguration");
    if (!function) {
        return {.success = false, .error = u"RtlQueryFeatureConfiguration недоступна."_s};
    }
    quint64 changeStamp{};
    NativeFeatureConfiguration native{};
    constexpr quint32 runtimeStore = 1;
    const auto status = function(featureId, runtimeStore, &changeStamp, &native);
    if (status != 0) {
        return {.success = false,
                .error = statusText(u"Чтение Feature Store"_s, status)};
    }
    return {.success = true,
            .configuration = FeatureConfiguration{
                .featureId = native.featureId,
                .priority = native.compactState & 0xFU,
                .state = static_cast<domain::FeatureEnabledState>((native.compactState >> 4U) & 0x3U),
                .enabledStateOptions = (native.compactState >> 6U) & 0x1U,
                .variant = (native.compactState >> 8U) & 0x3FU,
                .variantPayloadKind = (native.compactState >> 14U) & 0x3U,
                .variantPayload = native.variantPayload,
            }};
#else
    Q_UNUSED(featureId)
    return {.success = false, .error = u"Feature Store доступен только в Windows."_s};
#endif
}

FeatureMutationResult WindowsFeatureStoreBackend::setUserConfiguration(
    const FeatureConfiguration& configuration)
{
#ifdef Q_OS_WIN
    constexpr quint32 userPriority = 8;
    constexpr quint32 featureAndVariantState = 3;
    return update({
        .featureId = configuration.featureId,
        .priority = userPriority,
        .enabledState = static_cast<quint32>(configuration.state),
        .enabledStateOptions = configuration.enabledStateOptions,
        .variant = configuration.variant,
        .variantPayloadKind = configuration.variantPayloadKind,
        .variantPayload = configuration.variantPayload,
        .operation = featureAndVariantState,
    });
#else
    Q_UNUSED(configuration)
    return {.success = false, .error = u"Feature Store доступен только в Windows."_s};
#endif
}

FeatureMutationResult WindowsFeatureStoreBackend::setUserState(
    quint32 featureId,
    domain::FeatureEnabledState state)
{
#ifdef Q_OS_WIN
    constexpr quint32 userPriority = 8;
    constexpr quint32 featureState = 1;
    return update({
        .featureId = featureId,
        .priority = userPriority,
        .enabledState = static_cast<quint32>(state),
        .operation = featureState,
    });
#else
    Q_UNUSED(featureId)
    Q_UNUSED(state)
    return {.success = false, .error = u"Feature Store доступен только в Windows."_s};
#endif
}

FeatureMutationResult WindowsFeatureStoreBackend::resetUserConfiguration(quint32 featureId)
{
#ifdef Q_OS_WIN
    constexpr quint32 userPriority = 8;
    constexpr quint32 resetState = 4;
    return update({.featureId = featureId, .priority = userPriority, .operation = resetState});
#else
    Q_UNUSED(featureId)
    return {.success = false, .error = u"Feature Store доступен только в Windows."_s};
#endif
}

} // namespace tweakopedia::platform
