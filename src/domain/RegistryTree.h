#pragma once

#include "domain/RegistryTypes.h"

#include <QJsonObject>
#include <QVector>

#include <optional>

namespace tweakopedia::domain {

struct RegistryTreeValue {
    QString name;
    RegistryValueSpec value;

    friend bool operator==(const RegistryTreeValue&, const RegistryTreeValue&) = default;
};

struct RegistryTreeNode {
    QString relativePath;
    QVector<RegistryTreeValue> values;

    friend bool operator==(const RegistryTreeNode&, const RegistryTreeNode&) = default;
};

struct RegistryTreeLimits {
    qsizetype maxNodes{4096};
    qsizetype maxBytes{16 * 1024 * 1024};
};

struct RegistryTreeSnapshot {
    RegistryKeyLocation location;
    bool existed{};
    QVector<RegistryTreeNode> nodes;

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static std::optional<RegistryTreeSnapshot> fromJson(const QJsonObject& object);

    friend bool operator==(const RegistryTreeSnapshot&, const RegistryTreeSnapshot&) = default;
};

struct RegistryTreeDetection {
    RegistryKeyLocation location;
    QString presentState;
    QString missingState;
};

} // namespace tweakopedia::domain
