#include "app/EncyclopediaTreeModel.h"

#include <QSet>

#include <algorithm>
#include <variant>

using namespace Qt::StringLiterals;

namespace tweakopedia::app {
namespace {

QString registryLocation(const domain::RegistryLocation& location)
{
    const auto hive = location.hive == domain::RegistryHive::LocalMachine
        ? u"HKLM"_s : u"HKCU"_s;
    return hive + u"\\"_s + location.key + u"\\"_s + location.valueName;
}

} // namespace

EncyclopediaTreeModel::EncyclopediaTreeModel(QObject* parent)
    : QAbstractItemModel(parent)
    , root_(std::make_unique<Node>())
{
}

EncyclopediaTreeModel::~EncyclopediaTreeModel() = default;

QModelIndex EncyclopediaTreeModel::index(
    int row, int column, const QModelIndex& parentIndex) const
{
    if (column != 0 || row < 0) return {};
    auto* parentNode = parentIndex.isValid()
        ? static_cast<Node*>(parentIndex.internalPointer()) : root_.get();
    if (!parentNode || static_cast<std::size_t>(row) >= parentNode->children.size()) return {};
    return createIndex(row, column, parentNode->children.at(row).get());
}

QModelIndex EncyclopediaTreeModel::parent(const QModelIndex& child) const
{
    if (!child.isValid()) return {};
    const auto* node = static_cast<Node*>(child.internalPointer());
    const auto* parentNode = node ? node->parent : nullptr;
    if (!parentNode || parentNode == root_.get()) return {};
    return indexForNode(parentNode);
}

int EncyclopediaTreeModel::rowCount(const QModelIndex& parentIndex) const
{
    if (parentIndex.column() > 0) return 0;
    const auto* parentNode = parentIndex.isValid()
        ? static_cast<Node*>(parentIndex.internalPointer()) : root_.get();
    return parentNode ? static_cast<int>(parentNode->children.size()) : 0;
}

int EncyclopediaTreeModel::columnCount(const QModelIndex&) const
{
    return 1;
}

QVariant EncyclopediaTreeModel::data(const QModelIndex& modelIndex, int role) const
{
    if (!modelIndex.isValid()) return {};
    const auto* node = static_cast<Node*>(modelIndex.internalPointer());
    if (!node) return {};
    switch (role) {
    case NodeTypeRole:
        switch (node->type) {
        case NodeType::Category: return u"category"_s;
        case NodeType::Subcategory: return u"subcategory"_s;
        case NodeType::Article: return u"article"_s;
        case NodeType::Root: return u"root"_s;
        }
        return {};
    case IdRole: return node->id;
    case TitleRole: return node->title;
    case SummaryRole: return node->summary;
    case PathRole: return node->path;
    case DepthRole: return node->depth;
    case ExpandedRole: return node->expanded;
    case SelectedRole:
        return node->type == NodeType::Article && node->id == selectedArticleId_;
    case MatchScoreRole: return node->matchScore;
    default: return {};
    }
}

QHash<int, QByteArray> EncyclopediaTreeModel::roleNames() const
{
    return {
        {NodeTypeRole, "nodeType"},
        {IdRole, "id"},
        {TitleRole, "title"},
        {SummaryRole, "summary"},
        {PathRole, "path"},
        {DepthRole, "depth"},
        {ExpandedRole, "expanded"},
        {SelectedRole, "selected"},
        {MatchScoreRole, "matchScore"},
    };
}

Qt::ItemFlags EncyclopediaTreeModel::flags(const QModelIndex& modelIndex) const
{
    if (!modelIndex.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void EncyclopediaTreeModel::reset(
    const content::TweakCatalog& catalog,
    const content::CategoryCatalog& categories)
{
    beginResetModel();
    categories_ = categories.categories();
    QHash<QString, QString> navigationTitles;
    for (const auto& category : categories_) {
        for (const auto& subcategory : category.subcategories) {
            navigationTitles.insert(
                category.id + u'\n' + subcategory.id,
                normalize(category.title + u' ' + subcategory.title));
        }
    }
    articles_.clear();
    articles_.reserve(catalog.tweaks().size());
    int sourceOrder = 0;
    for (const auto& tweak : catalog.tweaks()) {
        const auto body = QStringList{
            tweak.explanation.purpose,
            tweak.explanation.mechanism,
            tweak.explanation.effect,
            tweak.explanation.tradeoffs,
            tweak.explanation.recommendation,
            tweak.explanation.technicalDetails,
        }.join(u' ');
        articles_.append({
            .id = tweak.id.toString(),
            .title = tweak.title,
            .categoryId = tweak.category,
            .subcategoryId = tweak.subcategory,
            .summary = tweak.summary,
            .titleSearch = normalize(tweak.title),
            .summarySearch = normalize(tweak.summary),
            .navigationSearch = navigationTitles.value(
                tweak.category + u'\n' + tweak.subcategory,
                normalize(u"Другие материалы Без раздела"_s)),
            .bodySearch = normalize(body),
            .technicalSearch = normalize(technicalTerms(tweak)),
            .sourceOrder = sourceOrder++,
        });
    }
    buildTree();
    endResetModel();
    emit articleCountChanged();
}

void EncyclopediaTreeModel::setQuery(const QString& query)
{
    const auto cleaned = query.trimmed();
    const auto normalized = normalize(cleaned);
    if (query_ == cleaned && normalizedQuery_ == normalized) return;
    beginResetModel();
    query_ = cleaned;
    normalizedQuery_ = normalized;
    buildTree();
    endResetModel();
    emit queryChanged();
    emit articleCountChanged();
}

QString EncyclopediaTreeModel::query() const
{
    return query_;
}

int EncyclopediaTreeModel::articleCount() const noexcept
{
    return articleCount_;
}

void EncyclopediaTreeModel::setSelectedArticleId(const QString& id)
{
    if (selectedArticleId_ == id) return;
    const auto oldIndex = indexForNode(visibleArticles_.value(selectedArticleId_));
    selectedArticleId_ = id;
    const auto newIndex = indexForNode(visibleArticles_.value(selectedArticleId_));
    if (oldIndex.isValid()) emit dataChanged(oldIndex, oldIndex, {SelectedRole});
    if (newIndex.isValid()) emit dataChanged(newIndex, newIndex, {SelectedRole});
}

QString EncyclopediaTreeModel::normalize(QStringView text)
{
    QString normalized;
    normalized.reserve(text.size());
    bool previousSpace = true;
    for (const auto character : text.toString().toCaseFolded()) {
        if (character.isLetterOrNumber()) {
            normalized.append(character);
            previousSpace = false;
        } else if (!previousSpace) {
            normalized.append(u' ');
            previousSpace = true;
        }
    }
    return normalized.trimmed();
}

QString EncyclopediaTreeModel::technicalTerms(const domain::TweakDefinition& tweak)
{
    QStringList terms;
    if (tweak.detection) terms.append(registryLocation(tweak.detection->location));
    if (tweak.appxDetection) terms.append(tweak.appxDetection->packageName);
    if (tweak.featureDetection) terms.append(QString::number(tweak.featureDetection->featureId));
    for (const auto& state : tweak.states) {
        for (const auto& operation : state.operations) {
            std::visit([&](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, domain::SetRegistryDwordOperation>) {
                    terms.append(registryLocation(value.location));
                } else if constexpr (std::is_same_v<T, domain::RemoveAppxPackageOperation>) {
                    terms.append(value.packageName);
                } else {
                    terms.append(QString::number(value.featureId));
                }
            }, operation);
        }
    }
    return terms.join(u' ');
}

int EncyclopediaTreeModel::score(
    const ArticleRecord& article, const QStringList& tokens) const
{
    if (normalizedQuery_.isEmpty()) return 1;
    const auto containsAll = [&](const QString& value) {
        return std::all_of(tokens.cbegin(), tokens.cend(), [&](const QString& token) {
            return value.contains(token);
        });
    };
    if (article.titleSearch == normalizedQuery_) return 1000;
    if (article.titleSearch.startsWith(normalizedQuery_)) return 800;
    if (containsAll(article.titleSearch)) return 600;
    if (containsAll(article.summarySearch)) return 400;
    const auto idSearch = normalize(article.id);
    if (containsAll(idSearch)) return 300;
    if (containsAll(article.navigationSearch)) return 250;
    if (containsAll(article.bodySearch)) return 200;
    if (containsAll(article.technicalSearch)) return 100;
    return 0;
}

void EncyclopediaTreeModel::buildTree()
{
    root_ = std::make_unique<Node>();
    visibleArticles_.clear();
    articleCount_ = 0;
    const auto tokens = normalizedQuery_.split(u' ', Qt::SkipEmptyParts);
    QVector<ArticleRecord*> matches;
    matches.reserve(articles_.size());
    for (auto& article : articles_) {
        article.matchScore = score(article, tokens);
        if (article.matchScore > 0) matches.append(&article);
    }

    QSet<QString> knownPairs;
    for (const auto& category : categories_) {
        for (const auto& subcategory : category.subcategories) {
            knownPairs.insert(category.id + u'\n' + subcategory.id);
        }
        QVector<ArticleRecord*> categoryRecords;
        for (auto* article : matches) {
            if (article->categoryId == category.id
                && knownPairs.contains(category.id + u'\n' + article->subcategoryId)) {
                categoryRecords.append(article);
            }
        }
        appendCategory(category, categoryRecords);
    }

    QVector<ArticleRecord*> orphanRecords;
    for (auto* article : matches) {
        if (!knownPairs.contains(article->categoryId + u'\n' + article->subcategoryId)) {
            orphanRecords.append(article);
        }
    }
    if (!orphanRecords.isEmpty()) {
        appendCategory({
            .id = u"other"_s,
            .title = u"Другие материалы"_s,
            .subcategories = {{.id = u"uncategorized"_s, .title = u"Без раздела"_s}},
        }, orphanRecords, true);
    }
}

void EncyclopediaTreeModel::appendCategory(
    const content::CategoryDefinition& category,
    const QVector<ArticleRecord*>& records,
    bool fallback)
{
    if (records.isEmpty()) return;
    auto categoryNode = std::make_unique<Node>();
    categoryNode->type = NodeType::Category;
    categoryNode->id = category.id;
    categoryNode->title = category.title;
    categoryNode->depth = 0;
    categoryNode->expanded = !normalizedQuery_.isEmpty();
    categoryNode->parent = root_.get();

    for (const auto& subcategory : category.subcategories) {
        QVector<ArticleRecord*> subcategoryRecords;
        for (auto* article : records) {
            if (fallback || article->subcategoryId == subcategory.id) {
                subcategoryRecords.append(article);
            }
        }
        if (subcategoryRecords.isEmpty()) continue;
        if (!normalizedQuery_.isEmpty()) {
            std::stable_sort(subcategoryRecords.begin(), subcategoryRecords.end(),
                             [](const auto* left, const auto* right) {
                if (left->matchScore != right->matchScore) {
                    return left->matchScore > right->matchScore;
                }
                return left->sourceOrder < right->sourceOrder;
            });
        }
        auto subcategoryNode = std::make_unique<Node>();
        subcategoryNode->type = NodeType::Subcategory;
        subcategoryNode->id = subcategory.id;
        subcategoryNode->title = subcategory.title;
        subcategoryNode->depth = 1;
        subcategoryNode->expanded = !normalizedQuery_.isEmpty();
        subcategoryNode->parent = categoryNode.get();

        for (auto* article : subcategoryRecords) {
            auto articleNode = std::make_unique<Node>();
            articleNode->type = NodeType::Article;
            articleNode->id = article->id;
            articleNode->title = article->title;
            articleNode->summary = article->summary;
            articleNode->path = category.title + u" › "_s + subcategory.title;
            articleNode->depth = 2;
            articleNode->matchScore = article->matchScore;
            articleNode->sourceOrder = article->sourceOrder;
            articleNode->parent = subcategoryNode.get();
            visibleArticles_.insert(articleNode->id, articleNode.get());
            subcategoryNode->children.push_back(std::move(articleNode));
            ++articleCount_;
        }
        categoryNode->children.push_back(std::move(subcategoryNode));
    }
    if (!categoryNode->children.empty()) root_->children.push_back(std::move(categoryNode));
}

QModelIndex EncyclopediaTreeModel::indexForNode(const Node* node) const
{
    if (!node || !node->parent || node == root_.get()) return {};
    const auto& siblings = node->parent->children;
    for (int row = 0; row < static_cast<int>(siblings.size()); ++row) {
        if (siblings.at(row).get() == node) return createIndex(row, 0, const_cast<Node*>(node));
    }
    return {};
}

} // namespace tweakopedia::app
