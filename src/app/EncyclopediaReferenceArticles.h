#pragma once

#include "content/CategoryCatalog.h"

#include <QString>
#include <QStringList>
#include <QStringView>
#include <QVector>

namespace tweakopedia::app {

struct EncyclopediaReferenceSection {
    QString id;
    QString title;
    QString text;
    bool technical{};
};

struct EncyclopediaReferenceArticle {
    QString id;
    QString title;
    QString summary;
    QString purpose;
    QString categoryId;
    QString categoryTitle;
    QString subcategoryId;
    QString subcategoryTitle;
    QVector<EncyclopediaReferenceSection> sections;
    QStringList technicalObjects;
    QStringList relatedArticleIds;
};

[[nodiscard]] const QVector<EncyclopediaReferenceArticle>&
encyclopediaReferenceArticles();
[[nodiscard]] const EncyclopediaReferenceArticle*
findEncyclopediaReferenceArticle(QStringView id);
[[nodiscard]] content::CategoryDefinition encyclopediaReferenceCategory();

} // namespace tweakopedia::app
