#include "app/TweakGroupListModel.h"

#include <algorithm>

using namespace Qt::StringLiterals;

namespace tweakopedia::app {

TweakGroupListModel::TweakGroupListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int TweakGroupListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(groups_.size());
}

QVariant TweakGroupListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= groups_.size()) return {};
    const auto& group = groups_.at(index.row());
    switch (role) {
    case IdRole: return group.id;
    case TitleRole: return group.title;
    case CountRole: return group.count;
    case EnabledRole: return true;
    case SelectedRole: return group.id == selectedId_;
    default: return {};
    }
}

QHash<int, QByteArray> TweakGroupListModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {TitleRole, "title"},
        {CountRole, "count"},
        {EnabledRole, "enabled"},
        {SelectedRole, "selected"},
    };
}

QString TweakGroupListModel::selectedId() const
{
    return selectedId_;
}

void TweakGroupListModel::reset(
    const content::CategoryCatalog& categories,
    const content::TweakCatalog& tweaks,
    const QHash<domain::TweakId, bool>& supported,
    QString categoryId,
    bool hideUnsupported)
{
    categoryId = categoryId.trimmed();
    const auto previousSelection = selectedId_;
    const auto categoryChanged = categoryId_ != categoryId;

    QVector<Group> groups;
    const auto* category = categoryId.isEmpty() ? nullptr : categories.find(categoryId);
    if (category) {
        for (const auto& subcategory : category->subcategories) {
            int count{};
            for (const auto& tweak : tweaks.tweaks()) {
                if (tweak.category != categoryId || tweak.subcategory != subcategory.id) continue;
                if (hideUnsupported && !supported.value(tweak.id)) continue;
                ++count;
            }
            if (count > 0) groups.append({subcategory.id, subcategory.title, count});
        }
    }

    if (!groups.isEmpty()) {
        int total{};
        for (const auto& group : groups) total += group.count;
        groups.prepend({{}, u"Все"_s, total});
    }

    beginResetModel();
    groups_ = std::move(groups);
    categoryId_ = std::move(categoryId);
    if (categoryChanged || indexOf(selectedId_) < 0) selectedId_.clear();
    endResetModel();

    if (selectedId_ != previousSelection) emit selectedIdChanged();
}

bool TweakGroupListModel::select(QStringView id)
{
    const auto row = indexOf(id);
    if (row < 0 || selectedId_ == id) return row >= 0;
    const auto previousRow = indexOf(selectedId_);
    selectedId_ = id.toString();
    if (previousRow >= 0) emit dataChanged(index(previousRow), index(previousRow), {SelectedRole});
    emit dataChanged(index(row), index(row), {SelectedRole});
    emit selectedIdChanged();
    return true;
}

QString TweakGroupListModel::adjacentId(int delta) const
{
    if (groups_.isEmpty()) return {};
    const auto current = indexOf(selectedId_);
    if (current < 0) return groups_.first().id;
    const auto requested = current + delta;
    const auto bounded = std::max(0, std::min(requested, static_cast<int>(groups_.size()) - 1));
    return groups_.at(bounded).id;
}

int TweakGroupListModel::indexOf(QStringView id) const
{
    for (int row = 0; row < groups_.size(); ++row) {
        if (groups_.at(row).id == id) return row;
    }
    return -1;
}

} // namespace tweakopedia::app
