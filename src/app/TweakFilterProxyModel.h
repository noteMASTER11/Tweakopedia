#pragma once

#include <QSortFilterProxyModel>

namespace tweakopedia::app {

class TweakFilterProxyModel final : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QString categoryId READ categoryId WRITE setCategoryId NOTIFY categoryIdChanged)

public:
    explicit TweakFilterProxyModel(QObject* parent = nullptr);

    [[nodiscard]] QString query() const;
    [[nodiscard]] QString categoryId() const;

public slots:
    void setQuery(QString query);
    void setCategoryId(QString categoryId);

signals:
    void queryChanged();
    void categoryIdChanged();

protected:
    [[nodiscard]] bool filterAcceptsRow(
        int sourceRow,
        const QModelIndex& sourceParent) const override;

private:
    QString query_;
    QString categoryId_;
};

} // namespace tweakopedia::app
