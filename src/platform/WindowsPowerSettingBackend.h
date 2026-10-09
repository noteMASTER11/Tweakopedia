#pragma once

#include "platform/IPowerSettingBackend.h"

namespace tweakopedia::platform {

struct PowerSchemeResult { std::optional<QString> scheme; QString error; };

class WindowsPowerSettingBackend final : public IPowerSettingBackend
{
public:
    PowerSettingReadResult read(const domain::PowerSettingLocation& location) const override;
    PowerSettingMutationResult write(
        const domain::PowerSettingLocation& location, quint32 index) override;
    PowerSettingMutationResult activateScheme(QStringView scheme) override;
    [[nodiscard]] PowerSchemeResult duplicateActiveScheme();
    [[nodiscard]] PowerSettingMutationResult deleteScheme(QStringView scheme);
};

} // namespace tweakopedia::platform
