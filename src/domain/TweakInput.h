#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <optional>
#include <variant>

namespace tweakopedia::domain {

enum class TweakInputType {
    Text,
    Integer,
    File,
    Choice,
    Boolean,
};

struct TweakInputChoice {
    QString value;
    QString label;

    friend bool operator==(const TweakInputChoice&, const TweakInputChoice&) = default;
};

struct TweakInputDefinition {
    QString id;
    QString label;
    TweakInputType type{TweakInputType::Text};
    bool required{};
    std::optional<qsizetype> minimumLength;
    std::optional<qsizetype> maximumLength;
    std::optional<qint64> minimum;
    std::optional<qint64> maximum;
    QStringList allowedExtensions;
    std::optional<quint64> maximumFileSize;
    QVector<TweakInputChoice> choices;

    friend bool operator==(const TweakInputDefinition&, const TweakInputDefinition&) = default;
};

struct InputArtifact {
    QString id;
    QString storageId;
    QString managedPath;
    quint64 size{};
    QByteArray sha256;

    friend bool operator==(const InputArtifact&, const InputArtifact&) = default;
};

using TweakInputValue = std::variant<QString, qint64, bool, InputArtifact>;
using TweakInputMap = QHash<QString, TweakInputValue>;

struct TweakInputIssue {
    QString inputId;
    QString code;
};

struct TweakInputValidationResult {
    std::optional<TweakInputMap> values;
    QVector<TweakInputIssue> issues;
};

[[nodiscard]] TweakInputValidationResult normalizeTweakInputs(
    const QVector<TweakInputDefinition>& definitions,
    const QVariantMap& provided);
[[nodiscard]] QVariant inputValueToVariant(const TweakInputValue& value);
[[nodiscard]] QVariantMap inputMapToVariantMap(const TweakInputMap& values);
[[nodiscard]] QVariantList inputDefinitionsToVariantList(
    const QVector<TweakInputDefinition>& definitions);
[[nodiscard]] QVariantList inputSummaryToVariantList(
    const QVector<TweakInputDefinition>& definitions,
    const TweakInputMap& values);
[[nodiscard]] QString inputTypeName(TweakInputType type);

} // namespace tweakopedia::domain
