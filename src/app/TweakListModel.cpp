#include "app/TweakListModel.h"

using namespace Qt::StringLiterals;

namespace tweakopedia::app {
namespace {

QString impactName(domain::Impact impact)
{
    switch (impact) {
    case domain::Impact::Low: return u"low"_s;
    case domain::Impact::Medium: return u"medium"_s;
    case domain::Impact::High: return u"high"_s;
    case domain::Impact::Critical: return u"critical"_s;
    }
    return {};
}

QString restartName(domain::RestartRequirement restart)
{
    switch (restart) {
    case domain::RestartRequirement::None: return u"none"_s;
    case domain::RestartRequirement::Explorer: return u"explorer"_s;
    case domain::RestartRequirement::Service: return u"service"_s;
    case domain::RestartRequirement::SignOut: return u"sign_out"_s;
    case domain::RestartRequirement::Reboot: return u"reboot"_s;
    }
    return {};
}

} // namespace

TweakListModel::TweakListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int TweakListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(entries_.size());
}

QVariant TweakListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= entries_.size()) return {};
    const auto& entry = entries_.at(index.row());
    switch (role) {
    case IdRole: return entry.tweak.id.toString();
    case TitleRole: return entry.tweak.title;
    case SummaryRole: return entry.tweak.summary;
    case CurrentStateRole: return entry.detected.stateId;
    case TargetStateRole: return entry.targetState;
    case SupportedRole: return entry.supported;
    case ImpactRole: return impactName(entry.tweak.impact);
    case RestartRole: return restartName(entry.tweak.restart);
    default: return {};
    }
}

QHash<int, QByteArray> TweakListModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {TitleRole, "title"},
        {SummaryRole, "summary"},
        {CurrentStateRole, "currentState"},
        {TargetStateRole, "targetState"},
        {SupportedRole, "supported"},
        {ImpactRole, "impact"},
        {RestartRole, "restart"},
    };
}

void TweakListModel::reset(
    const content::TweakCatalog& catalog,
    const QHash<domain::TweakId, domain::DetectedState>& states,
    const QHash<domain::TweakId, bool>& supported)
{
    beginResetModel();
    entries_.clear();
    entries_.reserve(catalog.size());
    for (const auto& tweak : catalog.tweaks()) {
        entries_.append({
            .tweak = tweak,
            .detected = states.value(tweak.id),
            .targetState = {},
            .supported = supported.value(tweak.id),
        });
    }
    endResetModel();
}

bool TweakListModel::setTargetState(const domain::TweakId& id, const QString& state)
{
    for (qsizetype row = 0; row < entries_.size(); ++row) {
        auto& entry = entries_[row];
        if (entry.tweak.id != id) continue;
        if (entry.targetState == state) return true;
        entry.targetState = state;
        const auto changed = index(static_cast<int>(row));
        emit dataChanged(changed, changed, {TargetStateRole});
        return true;
    }
    return false;
}

} // namespace tweakopedia::app
