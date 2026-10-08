#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>

#include <variant>

namespace tweakopedia::domain {

enum class RegistryHive {
    CurrentUser,
    LocalMachine,
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

struct RegistryDwordDetection {
    RegistryLocation location;
    QHash<quint32, QString> statesByValue;
    QString missingState;
};

struct SetRegistryDwordOperation {
    RegistryLocation location;
    quint32 value{};

    friend bool operator==(const SetRegistryDwordOperation&, const SetRegistryDwordOperation&) = default;
};

using OperationSpec = std::variant<SetRegistryDwordOperation>;

} // namespace tweakopedia::domain
