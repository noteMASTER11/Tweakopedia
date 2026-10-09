#pragma once

#include "content/CategoryCatalog.h"
#include "content/TweakCatalog.h"

#include <QAbstractListModel>
#include <QHash>

namespace tweakopedia::app {

class TweakGroupListModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString selectedId READ selectedId NOTIFY selectedIdChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        CountRole,
        EnabledRole,
        SelectedRole,
    };
    Q_ENUM(Role)

    explicit TweakGroupListModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] QString selectedId() const;

    void reset(
        const content::CategoryCatalog& categories,
        const content::TweakCatalog& tweaks,
        const QHash<domain::TweakId, bool>& supported,
        QString categoryId,
        bool hideUnsupported);
    [[nodiscard]] bool select(QStringView id);
    [[nodiscard]] QString adjacentId(int delta) const;

signals:
    void selectedIdChanged();

private:
    struct Group {
        QString id;
        QString title;
        int count{};
    };

    [[nodiscard]] int indexOf(QStringView id) const;

    QVector<Group> groups_;
    QString categoryId_;
    QString selectedId_;
};

} // namespace tweakopedia::app
