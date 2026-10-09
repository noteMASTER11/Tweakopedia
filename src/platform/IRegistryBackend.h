#pragma once

#include "domain/RegistryTypes.h"
#include "domain/RegistryTree.h"

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
    Qword,
    String,
    ExpandString,
    MultiString,
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

struct RegistryTreeReadResult {
    bool success{};
    domain::RegistryTreeSnapshot snapshot;
    QString code;
    std::error_code error;

    [[nodiscard]] static RegistryTreeReadResult succeeded(domain::RegistryTreeSnapshot snapshot)
    {
        return {.success = true, .snapshot = std::move(snapshot)};
    }

    [[nodiscard]] static RegistryTreeReadResult failed(
        QString code, std::error_code error = {})
    {
        return {.success = false, .code = std::move(code), .error = error};
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
    [[nodiscard]] virtual RegistryWriteResult writeValue(
        const domain::RegistryLocation& location,
        const domain::RegistryValueSpec& value)
    {
        return writeRaw(location, value.nativeType, value.rawValue);
    }
    [[nodiscard]] virtual RegistryWriteResult writeRaw(
        const domain::RegistryLocation& location,
        quint32 nativeType,
        const QByteArray& rawValue) = 0;
    [[nodiscard]] virtual RegistryWriteResult deleteValue(const domain::RegistryLocation& location) = 0;
    [[nodiscard]] virtual RegistryTreeReadResult readTree(
        const domain::RegistryKeyLocation&,
        const domain::RegistryTreeLimits& = {}) const
    {
        return RegistryTreeReadResult::failed(
            QStringLiteral("registry.tree_unsupported"),
            std::make_error_code(std::errc::operation_not_supported));
    }
    [[nodiscard]] virtual RegistryWriteResult createKey(const domain::RegistryKeyLocation&)
    {
        return RegistryWriteResult::failed(
            std::make_error_code(std::errc::operation_not_supported));
    }
    [[nodiscard]] virtual RegistryWriteResult deleteTree(const domain::RegistryKeyLocation&)
    {
        return RegistryWriteResult::failed(
            std::make_error_code(std::errc::operation_not_supported));
    }
    [[nodiscard]] virtual RegistryWriteResult restoreTree(const domain::RegistryTreeSnapshot&)
    {
        return RegistryWriteResult::failed(
            std::make_error_code(std::errc::operation_not_supported));
    }
};

} // namespace tweakopedia::platform
