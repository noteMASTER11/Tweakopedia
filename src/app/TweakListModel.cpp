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

QString stateTitle(const domain::TweakDefinition& tweak, QStringView stateId)
{
    for (const auto& state : tweak.states) {
        if (state.id == stateId) return state.title;
    }
    return {};
}

QVariantList availableStates(const domain::TweakDefinition& tweak)
{
    QVariantList result;
    result.reserve(tweak.states.size());
    for (const auto& state : tweak.states) {
        result.append(QVariantMap{
            {u"id"_s, state.id},
            {u"title"_s, state.title},
        });
    }
    return result;
}

bool isBinary(const domain::TweakDefinition& tweak)
{
    if (tweak.states.size() != 2) return false;
    bool hasDisabled{};
    bool hasEnabled{};
    for (const auto& state : tweak.states) {
        hasDisabled = hasDisabled || state.id == u"disabled";
        hasEnabled = hasEnabled || state.id == u"enabled";
    }
    return hasDisabled && hasEnabled;
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
    case CategoryRole: return entry.tweak.category;
    case SubcategoryRole: return entry.tweak.subcategory;
    case CurrentStateTitleRole: return stateTitle(entry.tweak, entry.detected.stateId);
    case TargetStateTitleRole: return stateTitle(entry.tweak, entry.targetState);
    case AvailableStatesRole: return availableStates(entry.tweak);
    case BinaryRole: return isBinary(entry.tweak);
    case PendingRole: return !entry.targetState.isEmpty();
    case SupportDetailsRole: return entry.detected.details;
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
        {CategoryRole, "category"},
        {SubcategoryRole, "subcategory"},
        {CurrentStateTitleRole, "currentStateTitle"},
        {TargetStateTitleRole, "targetStateTitle"},
        {AvailableStatesRole, "availableStates"},
        {BinaryRole, "binary"},
        {PendingRole, "pending"},
        {SupportDetailsRole, "supportDetails"},
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
        emit dataChanged(changed, changed, {
            TargetStateRole,
            TargetStateTitleRole,
            PendingRole,
        });
        return true;
    }
    return false;
}

} // namespace tweakopedia::app
