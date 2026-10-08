#pragma once

#include "domain/RegistryTypes.h"
#include "platform/IRegistryBackend.h"

#include <QJsonObject>

namespace tweakopedia::execution {

struct RegistrySnapshot {
    domain::RegistryLocation location;
    platform::RegistryPresence presence{platform::RegistryPresence::Missing};
    platform::RegistryValueType type{platform::RegistryValueType::None};
    quint32 nativeType{};
    QByteArray rawValue;

    [[nodiscard]] static RegistrySnapshot fromRead(
        domain::RegistryLocation location,
        const platform::RegistryReadResult& value);
    [[nodiscard]] static QByteArray fingerprint(const platform::RegistryReadResult& value);
    [[nodiscard]] QByteArray fingerprint() const;
    [[nodiscard]] QJsonObject toJson() const;
};

} // namespace tweakopedia::execution
