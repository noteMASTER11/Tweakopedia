#pragma once

#include "content/TweakCatalog.h"
#include "domain/DetectedState.h"

#include <QAbstractListModel>
#include <QHash>

namespace tweakopedia::app {

class TweakListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        SummaryRole,
        CurrentStateRole,
        TargetStateRole,
        SupportedRole,
        ImpactRole,
        RestartRole,
        CategoryRole,
        SubcategoryRole,
        CurrentStateTitleRole,
        TargetStateTitleRole,
        AvailableStatesRole,
        BinaryRole,
        PendingRole,
        SupportDetailsRole,
        ActionRole,
        InputsRole,
    };
    Q_ENUM(Role)

    explicit TweakListModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void reset(
        const content::TweakCatalog& catalog,
        const QHash<domain::TweakId, domain::DetectedState>& states,
        const QHash<domain::TweakId, bool>& supported);
    [[nodiscard]] bool setTargetState(const domain::TweakId& id, const QString& state);

private:
    struct Entry {
        domain::TweakDefinition tweak;
        domain::DetectedState detected;
        QString targetState;
        bool supported{};
    };

    QVector<Entry> entries_;
};

} // namespace tweakopedia::app
