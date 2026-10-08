#pragma once

#include "app/EncyclopediaArticleModel.h"
#include "app/EncyclopediaTreeModel.h"

#include <QObject>

namespace tweakopedia::app {

class EncyclopediaController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(EncyclopediaTreeModel* tree READ tree CONSTANT)
    Q_PROPERTY(EncyclopediaArticleModel* article READ article CONSTANT)
    Q_PROPERTY(QString query READ query NOTIFY queryChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY navigationChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY navigationChanged)

public:
    explicit EncyclopediaController(QObject* parent = nullptr);

    [[nodiscard]] EncyclopediaTreeModel* tree() noexcept;
    [[nodiscard]] EncyclopediaArticleModel* article() noexcept;
    [[nodiscard]] QString query() const;
    [[nodiscard]] bool loading() const noexcept;
    [[nodiscard]] bool canGoBack() const noexcept;
    [[nodiscard]] bool canGoForward() const noexcept;

    void reset(
        const content::TweakCatalog& catalog,
        const content::CategoryCatalog& categories);
    Q_INVOKABLE void setQuery(const QString& query);
    Q_INVOKABLE void clearSearch();
    Q_INVOKABLE bool openArticle(const QString& id);
    Q_INVOKABLE bool goBack();
    Q_INVOKABLE bool goForward();

signals:
    void queryChanged();
    void loadingChanged();
    void navigationChanged();

private:
    bool activateHistoryEntry(int index);

    EncyclopediaTreeModel treeModel_;
    EncyclopediaArticleModel articleModel_;
    QVector<QString> history_;
    int historyIndex_{-1};
    bool loading_{};
};

} // namespace tweakopedia::app
