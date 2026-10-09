#include "domain/TweakInput.h"

#include <QDir>
#include <QFileInfo>
#include <QMetaType>
#include <QSet>

#include <algorithm>
#include <type_traits>

using namespace Qt::StringLiterals;

namespace tweakopedia::domain {
namespace {

void issue(TweakInputValidationResult& result, QStringView id, QString code)
{
    result.issues.append({id.toString(), std::move(code)});
}

bool isIntegerVariant(const QVariant& value)
{
    switch (value.metaType().id()) {
    case QMetaType::Char:
    case QMetaType::SChar:
    case QMetaType::UChar:
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Long:
    case QMetaType::ULong:
    case QMetaType::LongLong:
        return true;
    default:
        return false;
    }
}

QString normalizedExtension(QString extension)
{
    extension = extension.trimmed().toLower();
    while (extension.startsWith(u'.')) extension.remove(0, 1);
    return extension;
}

} // namespace

TweakInputValidationResult normalizeTweakInputs(
    const QVector<TweakInputDefinition>& definitions,
    const QVariantMap& provided)
{
    TweakInputValidationResult result;
    TweakInputMap normalized;
    QSet<QString> declared;
    for (const auto& definition : definitions) declared.insert(definition.id);
    for (auto iterator = provided.cbegin(); iterator != provided.cend(); ++iterator) {
        if (!declared.contains(iterator.key())) {
            issue(result, iterator.key(), u"input.undeclared"_s);
        }
    }

    for (const auto& definition : definitions) {
        const auto iterator = provided.constFind(definition.id);
        if (iterator == provided.cend() || !iterator->isValid() || iterator->isNull()) {
            if (definition.required) issue(result, definition.id, u"input.required"_s);
            continue;
        }
        const auto& raw = *iterator;
        switch (definition.type) {
        case TweakInputType::Text: {
            if (raw.metaType().id() != QMetaType::QString) {
                issue(result, definition.id, u"input.type_invalid"_s);
                break;
            }
            const auto value = raw.toString().trimmed();
            if (definition.required && value.isEmpty()) {
                issue(result, definition.id, u"input.required"_s);
            } else if (definition.minimumLength && value.size() < *definition.minimumLength) {
                issue(result, definition.id, u"input.text_too_short"_s);
            } else if (definition.maximumLength && value.size() > *definition.maximumLength) {
                issue(result, definition.id, u"input.text_too_long"_s);
            } else if (!value.isEmpty() || definition.required) {
                normalized.insert(definition.id, value);
            }
            break;
        }
        case TweakInputType::Integer: {
            if (!isIntegerVariant(raw)) {
                issue(result, definition.id, u"input.type_invalid"_s);
                break;
            }
            bool ok{};
            const auto value = raw.toLongLong(&ok);
            if (!ok) {
                issue(result, definition.id, u"input.type_invalid"_s);
            } else if ((definition.minimum && value < *definition.minimum)
                       || (definition.maximum && value > *definition.maximum)) {
                issue(result, definition.id, u"input.integer_out_of_range"_s);
            } else {
                normalized.insert(definition.id, value);
            }
            break;
        }
        case TweakInputType::File: {
            if (raw.metaType().id() != QMetaType::QString) {
                issue(result, definition.id, u"input.type_invalid"_s);
                break;
            }
            const auto path = QDir::cleanPath(raw.toString().trimmed());
            if (path.isEmpty() || path == u"."_s) {
                if (definition.required) issue(result, definition.id, u"input.required"_s);
                break;
            }
            const QFileInfo info(path);
            const auto extension = normalizedExtension(info.suffix());
            QStringList allowed;
            for (const auto& item : definition.allowedExtensions) {
                allowed.append(normalizedExtension(item));
            }
            if (!allowed.isEmpty() && !allowed.contains(extension, Qt::CaseInsensitive)) {
                issue(result, definition.id, u"input.file_extension_invalid"_s);
            } else if (info.exists() && (!info.isFile() || !info.isReadable())) {
                issue(result, definition.id, u"input.file_unreadable"_s);
            } else if (info.exists() && definition.maximumFileSize
                       && static_cast<quint64>(info.size()) > *definition.maximumFileSize) {
                issue(result, definition.id, u"input.file_too_large"_s);
            } else {
                normalized.insert(definition.id, path);
            }
            break;
        }
        case TweakInputType::Choice: {
            if (raw.metaType().id() != QMetaType::QString) {
                issue(result, definition.id, u"input.type_invalid"_s);
                break;
            }
            const auto value = raw.toString().trimmed();
            const auto found = std::any_of(
                definition.choices.cbegin(), definition.choices.cend(),
                [&](const auto& choice) { return choice.value == value; });
            if (!found) issue(result, definition.id, u"input.choice_unknown"_s);
            else normalized.insert(definition.id, value);
            break;
        }
        case TweakInputType::Boolean:
            if (raw.metaType().id() != QMetaType::Bool) {
                issue(result, definition.id, u"input.type_invalid"_s);
            } else {
                normalized.insert(definition.id, raw.toBool());
            }
            break;
        }
    }

    if (result.issues.isEmpty()) result.values = std::move(normalized);
    return result;
}

QVariant inputValueToVariant(const TweakInputValue& value)
{
    return std::visit([](const auto& item) -> QVariant {
        using Value = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<Value, InputArtifact>) {
            return QVariantMap{
                {u"id"_s, item.id}, {u"storageId"_s, item.storageId},
                {u"managedPath"_s, item.managedPath},
                {u"size"_s, static_cast<qulonglong>(item.size)},
                {u"sha256"_s, QString::fromLatin1(item.sha256)},
            };
        } else {
            return QVariant::fromValue(item);
        }
    }, value);
}

QVariantMap inputMapToVariantMap(const TweakInputMap& values)
{
    QVariantMap result;
    for (auto iterator = values.cbegin(); iterator != values.cend(); ++iterator) {
        result.insert(iterator.key(), inputValueToVariant(iterator.value()));
    }
    return result;
}

QString inputTypeName(TweakInputType type)
{
    switch (type) {
    case TweakInputType::Text: return u"text"_s;
    case TweakInputType::Integer: return u"integer"_s;
    case TweakInputType::File: return u"file"_s;
    case TweakInputType::Choice: return u"choice"_s;
    case TweakInputType::Boolean: return u"boolean"_s;
    }
    return {};
}

QVariantList inputDefinitionsToVariantList(const QVector<TweakInputDefinition>& definitions)
{
    QVariantList result;
    for (const auto& definition : definitions) {
        QVariantList choices;
        for (const auto& choice : definition.choices) {
            choices.append(QVariantMap{{u"value"_s, choice.value}, {u"label"_s, choice.label}});
        }
        QVariantMap item{
            {u"id"_s, definition.id}, {u"label"_s, definition.label},
            {u"type"_s, inputTypeName(definition.type)}, {u"required"_s, definition.required},
            {u"choices"_s, choices}, {u"extensions"_s, definition.allowedExtensions},
        };
        if (definition.minimumLength) item.insert(u"minLength"_s, *definition.minimumLength);
        if (definition.maximumLength) item.insert(u"maxLength"_s, *definition.maximumLength);
        if (definition.minimum) item.insert(u"minimum"_s, *definition.minimum);
        if (definition.maximum) item.insert(u"maximum"_s, *definition.maximum);
        if (definition.maximumFileSize) item.insert(
            u"maxFileSize"_s, static_cast<qulonglong>(*definition.maximumFileSize));
        result.append(item);
    }
    return result;
}

QVariantList inputSummaryToVariantList(
    const QVector<TweakInputDefinition>& definitions,
    const TweakInputMap& values)
{
    QVariantList result;
    for (const auto& definition : definitions) {
        const auto value = values.constFind(definition.id);
        if (value == values.cend()) continue;
        result.append(QVariantMap{
            {u"id"_s, definition.id}, {u"label"_s, definition.label},
            {u"type"_s, inputTypeName(definition.type)},
            {u"value"_s, inputValueToVariant(*value)},
        });
    }
    return result;
}

} // namespace tweakopedia::domain
