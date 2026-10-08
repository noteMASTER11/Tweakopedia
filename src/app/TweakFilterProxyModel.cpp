#include "app/TweakFilterProxyModel.h"

#include "app/TweakListModel.h"

namespace tweakopedia::app {

TweakFilterProxyModel::TweakFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

QString TweakFilterProxyModel::query() const
{
    return query_;
}

QString TweakFilterProxyModel::categoryId() const
{
    return categoryId_;
}

bool TweakFilterProxyModel::hideUnsupported() const noexcept
{
    return hideUnsupported_;
}

void TweakFilterProxyModel::setQuery(QString query)
{
    query = query.trimmed();
    if (query_ == query) return;
    query_ = std::move(query);
    invalidateRowsFilter();
    emit queryChanged();
}

void TweakFilterProxyModel::setCategoryId(QString categoryId)
{
    categoryId = categoryId.trimmed();
    if (categoryId_ == categoryId) return;
    categoryId_ = std::move(categoryId);
    invalidateRowsFilter();
    emit categoryIdChanged();
}

void TweakFilterProxyModel::setHideUnsupported(bool hideUnsupported)
{
    if (hideUnsupported_ == hideUnsupported) return;
    hideUnsupported_ = hideUnsupported;
    invalidateRowsFilter();
    emit hideUnsupportedChanged();
}

bool TweakFilterProxyModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex& sourceParent) const
{
    if (!sourceModel()) return false;
    const auto index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (hideUnsupported_
        && !sourceModel()->data(index, TweakListModel::SupportedRole).toBool()) {
        return false;
    }
    if (!categoryId_.isEmpty()
        && sourceModel()->data(index, TweakListModel::CategoryRole).toString() != categoryId_) {
        return false;
    }
    if (query_.isEmpty()) return true;
    const auto title = sourceModel()->data(index, TweakListModel::TitleRole).toString();
    const auto summary = sourceModel()->data(index, TweakListModel::SummaryRole).toString();
    return title.contains(query_, Qt::CaseInsensitive)
        || summary.contains(query_, Qt::CaseInsensitive);
}

} // namespace tweakopedia::app
