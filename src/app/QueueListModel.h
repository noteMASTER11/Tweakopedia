#pragma once

#include "content/TweakCatalog.h"
#include "domain/DetectedState.h"
#include "planning/TweakQueue.h"

#include <QAbstractListModel>
#include <QHash>

namespace tweakopedia::app {

class QueueListModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role { IdRole = Qt::UserRole + 1, TitleRole, CurrentStateRole, TargetStateRole };
    Q_ENUM(Role)

    explicit QueueListModel(QObject* parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int count() const { return rowCount(); }
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    void reset(
        const content::TweakCatalog& catalog,
        const planning::TweakQueue& queue,
        const QHash<domain::TweakId, domain::DetectedState>& states);

signals:
    void countChanged();

private:
    struct Entry { domain::TweakId id; QString title; QString currentState; QString targetState; };
    QVector<Entry> entries_;
};

} // namespace tweakopedia::app
