#include "app/QueueListModel.h"

namespace tweakopedia::app {

QueueListModel::QueueListModel(QObject* parent) : QAbstractListModel(parent) {}

int QueueListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(entries_.size());
}

QVariant QueueListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= entries_.size()) return {};
    const auto& entry = entries_.at(index.row());
    switch (role) {
    case IdRole: return entry.id.toString();
    case TitleRole: return entry.title;
    case CurrentStateRole: return entry.currentState;
    case TargetStateRole: return entry.targetState;
    default: return {};
    }
}

QHash<int, QByteArray> QueueListModel::roleNames() const
{
    return {{IdRole, "id"}, {TitleRole, "title"}, {CurrentStateRole, "currentState"},
            {TargetStateRole, "targetState"}};
}

void QueueListModel::reset(
    const content::TweakCatalog& catalog,
    const planning::TweakQueue& queue,
    const QHash<domain::TweakId, domain::DetectedState>& states)
{
    beginResetModel();
    entries_.clear();
    for (const auto& item : queue.items()) {
        if (const auto* tweak = catalog.find(item.tweakId)) {
            entries_.append({item.tweakId, tweak->title, states.value(item.tweakId).stateId, item.targetState});
        }
    }
    endResetModel();
}

} // namespace tweakopedia::app
