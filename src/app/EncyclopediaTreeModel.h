#pragma once

#include "content/CategoryCatalog.h"
#include "content/TweakCatalog.h"

#include <QAbstractItemModel>

#include <memory>
#include <vector>

namespace tweakopedia::app {

class EncyclopediaTreeModel final : public QAbstractItemModel
{
    Q_OBJECT
    Q_PROPERTY(QString query READ query NOTIFY queryChanged)
    Q_PROPERTY(int articleCount READ articleCount NOTIFY articleCountChanged)

public:
    enum Role {
        NodeTypeRole = Qt::UserRole + 1,
        IdRole,
        TitleRole,
        SummaryRole,
        PathRole,
        DepthRole,
        ExpandedRole,
        SelectedRole,
        MatchScoreRole,
    };
    Q_ENUM(Role)

    explicit EncyclopediaTreeModel(QObject* parent = nullptr);
    ~EncyclopediaTreeModel() override;

    [[nodiscard]] QModelIndex index(
        int row, int column, const QModelIndex& parent = {}) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex& child) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;

    void reset(
        const content::TweakCatalog& catalog,
        const content::CategoryCatalog& categories);
    void setQuery(const QString& query);
    [[nodiscard]] QString query() const;
    [[nodiscard]] int articleCount() const noexcept;
    void setSelectedArticleId(const QString& id);

signals:
    void queryChanged();
    void articleCountChanged();

private:
    enum class NodeType { Root, Category, Subcategory, Article };

    struct Node {
        NodeType type{NodeType::Root};
        QString id;
        QString title;
        QString summary;
        QString path;
        int depth{};
        int matchScore{};
        int sourceOrder{};
        bool expanded{};
        Node* parent{};
        std::vector<std::unique_ptr<Node>> children;
    };

    struct ArticleRecord {
        QString id;
        QString title;
        QString categoryId;
        QString subcategoryId;
        QString summary;
        QString titleSearch;
        QString summarySearch;
        QString bodySearch;
        QString technicalSearch;
        int sourceOrder{};
        int matchScore{};
    };

    [[nodiscard]] static QString normalize(QStringView text);
    [[nodiscard]] static QString technicalTerms(const domain::TweakDefinition& tweak);
    [[nodiscard]] int score(const ArticleRecord& article, const QStringList& tokens) const;
    void buildTree();
    void appendCategory(
        const content::CategoryDefinition& category,
        const QVector<ArticleRecord*>& records,
        bool fallback = false);
    [[nodiscard]] QModelIndex indexForNode(const Node* node) const;

    std::unique_ptr<Node> root_;
    QVector<ArticleRecord> articles_;
    QVector<content::CategoryDefinition> categories_;
    QHash<QString, Node*> visibleArticles_;
    QString query_;
    QString normalizedQuery_;
    QString selectedArticleId_;
    int articleCount_{};
};

} // namespace tweakopedia::app
