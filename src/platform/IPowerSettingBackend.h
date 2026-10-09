#pragma once

#include "domain/PowerSetting.h"

namespace tweakopedia::platform {

struct PowerSettingReadResult {
    bool success{};
    bool missing{true};
    quint32 index{};
    QString resolvedScheme;
    QString error;
    static PowerSettingReadResult present(quint32 index, QString resolvedScheme)
    { return {.success = true, .missing = false, .index = index,
              .resolvedScheme = std::move(resolvedScheme)}; }
    static PowerSettingReadResult failed(QString error)
    { return {.success = false, .missing = false, .error = std::move(error)}; }
};

struct PowerSettingMutationResult { bool success{}; QString error; };

class IPowerSettingBackend
{
public:
    virtual ~IPowerSettingBackend() = default;
    virtual PowerSettingReadResult read(const domain::PowerSettingLocation& location) const = 0;
    virtual PowerSettingMutationResult write(
        const domain::PowerSettingLocation& location, quint32 index) = 0;
    virtual PowerSettingMutationResult activateScheme(QStringView) { return {.success = true}; }
};

} // namespace tweakopedia::platform
