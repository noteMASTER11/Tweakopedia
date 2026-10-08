#include "app/EncyclopediaController.h"

namespace tweakopedia::app {

EncyclopediaController::EncyclopediaController(QObject* parent)
    : QObject(parent)
    , treeModel_(this)
    , articleModel_(this)
{
}

EncyclopediaTreeModel* EncyclopediaController::tree() noexcept
{
    return &treeModel_;
}

EncyclopediaArticleModel* EncyclopediaController::article() noexcept
{
    return &articleModel_;
}

QString EncyclopediaController::query() const
{
    return treeModel_.query();
}

bool EncyclopediaController::loading() const noexcept
{
    return loading_;
}

bool EncyclopediaController::canGoBack() const noexcept
{
    return historyIndex_ > 0;
}

bool EncyclopediaController::canGoForward() const noexcept
{
    return historyIndex_ >= 0 && historyIndex_ + 1 < history_.size();
}

void EncyclopediaController::reset(
    const content::TweakCatalog& catalog,
    const content::CategoryCatalog& categories)
{
    loading_ = true;
    emit loadingChanged();
    treeModel_.reset(catalog, categories);
    articleModel_.reset(catalog, categories);
    treeModel_.setSelectedArticleId(articleModel_.selectedArticleId());
    loading_ = false;
    emit loadingChanged();
}

void EncyclopediaController::setQuery(const QString& query)
{
    const auto before = treeModel_.query();
    treeModel_.setQuery(query);
    if (before != treeModel_.query()) emit queryChanged();
}

void EncyclopediaController::clearSearch()
{
    setQuery({});
}

bool EncyclopediaController::openArticle(const QString& id)
{
    if (!articleModel_.selectArticle(id)) {
        treeModel_.setSelectedArticleId({});
        history_.clear();
        historyIndex_ = -1;
        emit navigationChanged();
        return false;
    }
    treeModel_.setSelectedArticleId(id);
    if (historyIndex_ >= 0 && history_.at(historyIndex_) == id) return true;
    if (historyIndex_ + 1 < history_.size()) history_.resize(historyIndex_ + 1);
    history_.append(id);
    historyIndex_ = history_.size() - 1;
    emit navigationChanged();
    return true;
}

bool EncyclopediaController::goBack()
{
    return canGoBack() && activateHistoryEntry(historyIndex_ - 1);
}

bool EncyclopediaController::goForward()
{
    return canGoForward() && activateHistoryEntry(historyIndex_ + 1);
}

bool EncyclopediaController::activateHistoryEntry(int index)
{
    if (index < 0 || index >= history_.size()) return false;
    if (!articleModel_.selectArticle(history_.at(index))) return false;
    historyIndex_ = index;
    treeModel_.setSelectedArticleId(history_.at(index));
    emit navigationChanged();
    return true;
}

} // namespace tweakopedia::app
