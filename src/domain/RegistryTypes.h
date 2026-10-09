#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QStringView>

#include <QtEndian>

#include <variant>
#include <optional>

#include "domain/FileOperation.h"
#include "domain/BcdElement.h"
#include "domain/PowerSetting.h"
#include "domain/ScheduledTask.h"
#include "domain/WindowsComponent.h"

namespace tweakopedia::domain {

enum class RegistryHive {
    CurrentUser,
    LocalMachine,
    ClassesRoot,
    Users,
};

enum class RegistryView {
    Default,
    Registry32,
    Registry64,
};

struct RegistryLocation {
    RegistryHive hive{RegistryHive::CurrentUser};
    QString key;
    QString valueName;
    RegistryView view{RegistryView::Default};

    friend bool operator==(const RegistryLocation&, const RegistryLocation&) = default;
};

struct RegistryKeyLocation {
    RegistryHive hive{RegistryHive::CurrentUser};
    QString key;
    RegistryView view{RegistryView::Default};

    friend bool operator==(const RegistryKeyLocation&, const RegistryKeyLocation&) = default;
};

struct RegistryDwordDetection {
    RegistryLocation location;
    QHash<quint32, QString> statesByValue;
    QString missingState;
};

struct RegistryValueSpec {
    quint32 nativeType{};
    QByteArray rawValue;

    [[nodiscard]] static RegistryValueSpec dword(quint32 value)
    {
        QByteArray bytes(sizeof(value), Qt::Uninitialized);
        qToLittleEndian(value, bytes.data());
        return {.nativeType = 4, .rawValue = std::move(bytes)};
    }

    [[nodiscard]] static RegistryValueSpec qword(quint64 value)
    {
        QByteArray bytes(sizeof(value), Qt::Uninitialized);
        qToLittleEndian(value, bytes.data());
        return {.nativeType = 11, .rawValue = std::move(bytes)};
    }

    [[nodiscard]] static RegistryValueSpec string(QStringView value)
    {
        return {.nativeType = 1, .rawValue = utf16Terminated(value)};
    }

    [[nodiscard]] static RegistryValueSpec expandString(QStringView value)
    {
        return {.nativeType = 2, .rawValue = utf16Terminated(value)};
    }

    [[nodiscard]] static RegistryValueSpec multiString(const QStringList& values)
    {
        QByteArray bytes;
        for (const auto& value : values) {
            bytes.append(utf16Terminated(value));
        }
        if (values.isEmpty()) {
            bytes.append(QByteArray(2, '\0'));
        }
        bytes.append(QByteArray(2, '\0'));
        return {.nativeType = 7, .rawValue = std::move(bytes)};
    }

    [[nodiscard]] static RegistryValueSpec binary(QByteArray value)
    {
        return {.nativeType = 3, .rawValue = std::move(value)};
    }

    friend bool operator==(const RegistryValueSpec&, const RegistryValueSpec&) = default;

private:
    [[nodiscard]] static QByteArray utf16Terminated(QStringView value)
    {
        QByteArray bytes((value.size() + 1) * qsizetype{2}, Qt::Uninitialized);
        for (qsizetype index = 0; index < value.size(); ++index) {
            qToLittleEndian(value.at(index).unicode(), bytes.data() + index * 2);
        }
        qToLittleEndian<quint16>(0, bytes.data() + value.size() * 2);
        return bytes;
    }
};

struct RegistryValueDetection {
    RegistryLocation location;
    QHash<QString, RegistryValueSpec> valuesByState;
    QString missingState;
};

struct SetRegistryDwordOperation {
    RegistryLocation location;
    quint32 value{};
    std::optional<QString> valueInput;

    friend bool operator==(const SetRegistryDwordOperation&, const SetRegistryDwordOperation&) = default;
};

struct SetRegistryValueOperation {
    RegistryLocation location;
    RegistryValueSpec value;
    std::optional<QString> valueInput;

    friend bool operator==(const SetRegistryValueOperation&, const SetRegistryValueOperation&) = default;
};

struct DeleteRegistryValueOperation {
    RegistryLocation location;

    friend bool operator==(const DeleteRegistryValueOperation&, const DeleteRegistryValueOperation&) = default;
};

struct CreateRegistryKeyOperation {
    RegistryKeyLocation location;

    friend bool operator==(const CreateRegistryKeyOperation&, const CreateRegistryKeyOperation&) = default;
};

struct DeleteRegistryTreeOperation {
    RegistryKeyLocation location;

    friend bool operator==(const DeleteRegistryTreeOperation&, const DeleteRegistryTreeOperation&) = default;
};

struct AppxPackageDetection {
    QString packageName;
};

struct RemoveAppxPackageOperation {
    QString packageName;

    friend bool operator==(const RemoveAppxPackageOperation&, const RemoveAppxPackageOperation&) = default;
};

enum class FeatureEnabledState : quint32 {
    Default = 0,
    Disabled = 1,
    Enabled = 2,
};

struct FeatureStateDetection {
    quint32 featureId{};
};

struct SetFeatureStateOperation {
    quint32 featureId{};
    FeatureEnabledState state{FeatureEnabledState::Default};

    friend bool operator==(const SetFeatureStateOperation&, const SetFeatureStateOperation&) = default;
};

using OperationSpec = std::variant<
    SetRegistryDwordOperation,
    SetRegistryValueOperation,
    DeleteRegistryValueOperation,
    CreateRegistryKeyOperation,
    DeleteRegistryTreeOperation,
    FileOperationDefinition,
    SetScheduledTaskEnabledOperation,
    SetBcdElementOperation,
    SetPowerSettingOperation,
    SetWindowsComponentStateOperation,
    RemoveAppxPackageOperation,
    SetFeatureStateOperation>;

} // namespace tweakopedia::domain
