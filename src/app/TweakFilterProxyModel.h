#pragma once

#include <QSortFilterProxyModel>

namespace tweakopedia::app {

class TweakFilterProxyModel final : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QString categoryId READ categoryId WRITE setCategoryId NOTIFY categoryIdChanged)
    Q_PROPERTY(bool hideUnsupported READ hideUnsupported WRITE setHideUnsupported NOTIFY hideUnsupportedChanged)

public:
    explicit TweakFilterProxyModel(QObject* parent = nullptr);

    [[nodiscard]] QString query() const;
    [[nodiscard]] QString categoryId() const;
    [[nodiscard]] bool hideUnsupported() const noexcept;

public slots:
    void setQuery(QString query);
    void setCategoryId(QString categoryId);
    void setHideUnsupported(bool hideUnsupported);

signals:
    void queryChanged();
    void categoryIdChanged();
    void hideUnsupportedChanged();

protected:
    [[nodiscard]] bool filterAcceptsRow(
        int sourceRow,
        const QModelIndex& sourceParent) const override;

private:
    QString query_;
    QString categoryId_;
    bool hideUnsupported_{};
};

} // namespace tweakopedia::app
