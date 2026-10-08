#pragma once

#include "platform/IRegistryBackend.h"

#include <QHash>

namespace tweakopedia::tests {

class FakeRegistryBackend final : public platform::IRegistryBackend
{
public:
    void setReadResult(const domain::RegistryLocation& location, platform::RegistryReadResult result);

    [[nodiscard]] platform::RegistryReadResult read(const domain::RegistryLocation& location) const override;
    [[nodiscard]] platform::RegistryWriteResult writeDword(
        const domain::RegistryLocation& location,
        quint32 value) override;
    [[nodiscard]] platform::RegistryWriteResult writeRaw(
        const domain::RegistryLocation& location,
        quint32 nativeType,
        const QByteArray& rawValue) override;
    [[nodiscard]] platform::RegistryWriteResult deleteValue(const domain::RegistryLocation& location) override;

private:
    [[nodiscard]] static QString keyFor(const domain::RegistryLocation& location);

    QHash<QString, platform::RegistryReadResult> values_;
};

} // namespace tweakopedia::tests
