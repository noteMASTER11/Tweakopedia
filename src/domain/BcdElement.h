#pragma once

#include <QHash>
#include <QJsonObject>
#include <QString>

#include <optional>
#include <variant>

namespace tweakopedia::domain {

enum class BcdValueKind { Boolean, Integer, String };
using BcdValue = std::variant<bool, quint64, QString>;

struct BcdElementSpec {
    QString objectId;
    quint32 elementType{};
    BcdValueKind valueKind{BcdValueKind::Boolean};

    friend bool operator==(const BcdElementSpec&, const BcdElementSpec&) = default;
};

struct BcdElementDetection {
    BcdElementSpec spec;
    QHash<QString, BcdValue> valuesByState;
    QString missingState;
};

struct SetBcdElementOperation {
    BcdElementSpec spec;
    std::optional<BcdValue> value;

    friend bool operator==(const SetBcdElementOperation&, const SetBcdElementOperation&) = default;
};

struct BcdElementSnapshot {
    BcdElementSpec spec;
    bool existed{};
    std::optional<BcdValue> value;

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static std::optional<BcdElementSnapshot> fromJson(const QJsonObject& object);
};

[[nodiscard]] bool bcdValueMatchesKind(const BcdValue& value, BcdValueKind kind);
[[nodiscard]] bool isWhitelistedBcdElement(const BcdElementSpec& spec);
[[nodiscard]] QJsonObject bcdValueToJson(const BcdValue& value);
[[nodiscard]] std::optional<BcdValue> bcdValueFromJson(
    const QJsonObject& object, BcdValueKind expectedKind);

} // namespace tweakopedia::domain
