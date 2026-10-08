#pragma once

#include "domain/RegistryTypes.h"

#include <QByteArray>

#include <system_error>

namespace tweakopedia::platform {

enum class RegistryPresence {
    Missing,
    Present,
    Error,
};

enum class RegistryValueType {
    None,
    Dword,
    String,
    Binary,
    Unknown,
};

struct RegistryReadResult {
    RegistryPresence presence{RegistryPresence::Missing};
    RegistryValueType type{RegistryValueType::None};
    quint32 nativeType{};
    QByteArray rawValue;
    std::error_code error;

    [[nodiscard]] static RegistryReadResult missing()
    {
        return {};
    }

    [[nodiscard]] static RegistryReadResult present(
        RegistryValueType type,
        QByteArray rawValue,
        quint32 nativeType = 0)
    {
        return {
            .presence = RegistryPresence::Present,
            .type = type,
            .nativeType = nativeType,
            .rawValue = std::move(rawValue),
        };
    }

    [[nodiscard]] static RegistryReadResult failed(std::error_code error)
    {
        return {
            .presence = RegistryPresence::Error,
            .type = RegistryValueType::None,
            .error = error,
        };
    }
};

struct RegistryWriteResult {
    bool success{};
    std::error_code error;

    [[nodiscard]] static RegistryWriteResult succeeded()
    {
        return {.success = true};
    }

    [[nodiscard]] static RegistryWriteResult failed(std::error_code error)
    {
        return {.success = false, .error = error};
    }
};

class IRegistryBackend
{
public:
    virtual ~IRegistryBackend() = default;

    [[nodiscard]] virtual RegistryReadResult read(const domain::RegistryLocation& location) const = 0;
    [[nodiscard]] virtual RegistryWriteResult writeDword(
        const domain::RegistryLocation& location,
        quint32 value) = 0;
    [[nodiscard]] virtual RegistryWriteResult writeRaw(
        const domain::RegistryLocation& location,
        quint32 nativeType,
        const QByteArray& rawValue) = 0;
    [[nodiscard]] virtual RegistryWriteResult deleteValue(const domain::RegistryLocation& location) = 0;
};

} // namespace tweakopedia::platform
