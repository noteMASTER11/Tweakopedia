#pragma once
#include "domain/PowerSetting.h"
#include "platform/IPowerSettingBackend.h"
namespace tweakopedia::execution {
struct PowerSettingExecutionResult { bool success{}; QString code; QString message; };
struct PowerSettingCaptureResult : PowerSettingExecutionResult { domain::PowerSettingSnapshot snapshot; };
class PowerSettingExecutor final
{
public:
    explicit PowerSettingExecutor(platform::IPowerSettingBackend& backend) : backend_(&backend) {}
    PowerSettingCaptureResult capture(const domain::PowerSettingLocation& location) const;
    PowerSettingExecutionResult compareBefore(const domain::PowerSettingSnapshot& snapshot,
                                               const QByteArray& fingerprint) const;
    PowerSettingExecutionResult apply(const domain::SetPowerSettingOperation& operation) const;
    PowerSettingExecutionResult restore(const domain::PowerSettingSnapshot& snapshot) const;
    static QByteArray fingerprint(const domain::PowerSettingSnapshot& snapshot);
private: platform::IPowerSettingBackend* backend_{};
};
}
