#pragma once

#include "content/CategoryCatalog.h"
#include "content/TweakCatalog.h"

#include <QObject>
#include <QVariantMap>

namespace tweakopedia::app {

class EncyclopediaArticleModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap article READ article NOTIFY articleChanged)
    Q_PROPERTY(bool hasArticle READ hasArticle NOTIFY articleChanged)

public:
    explicit EncyclopediaArticleModel(QObject* parent = nullptr);

    void reset(
        const content::TweakCatalog& catalog,
        const content::CategoryCatalog& categories);
    Q_INVOKABLE bool selectArticle(const QString& id);
    Q_INVOKABLE void clear();

    [[nodiscard]] QVariantMap article() const;
    [[nodiscard]] bool hasArticle() const noexcept;
    [[nodiscard]] QString selectedArticleId() const;

signals:
    void articleChanged();

private:
    [[nodiscard]] const domain::TweakDefinition* find(QStringView id) const;
    [[nodiscard]] QVariantMap buildArticle(const domain::TweakDefinition& tweak) const;
    [[nodiscard]] QVariantList relatedArticles(const domain::TweakDefinition& tweak) const;
    [[nodiscard]] QString categoryTitle(QStringView id) const;
    [[nodiscard]] QString subcategoryTitle(QStringView categoryId, QStringView id) const;

    QVector<domain::TweakDefinition> tweaks_;
    QVector<content::CategoryDefinition> categories_;
    QString selectedArticleId_;
    QVariantMap article_;
};

} // namespace tweakopedia::app
