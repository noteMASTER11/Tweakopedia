#include "app/EncyclopediaArticleModel.h"

#include <QSet>

#include <algorithm>
#include <variant>

using namespace Qt::StringLiterals;

namespace tweakopedia::app {
namespace {

QString registryLocation(const domain::RegistryLocation& location)
{
    QString hive;
    switch (location.hive) {
    case domain::RegistryHive::CurrentUser: hive = u"HKCU"_s; break;
    case domain::RegistryHive::LocalMachine: hive = u"HKLM"_s; break;
    case domain::RegistryHive::ClassesRoot: hive = u"HKCR"_s; break;
    case domain::RegistryHive::Users: hive = u"HKU"_s; break;
    }
    return hive + u"\\"_s + location.key + u"\\"_s + location.valueName;
}

QString registryLocation(const domain::RegistryKeyLocation& location)
{
    QString hive;
    switch (location.hive) {
    case domain::RegistryHive::CurrentUser: hive = u"HKCU"_s; break;
    case domain::RegistryHive::LocalMachine: hive = u"HKLM"_s; break;
    case domain::RegistryHive::ClassesRoot: hive = u"HKCR"_s; break;
    case domain::RegistryHive::Users: hive = u"HKU"_s; break;
    }
    return hive + u"\\"_s + location.key;
}

QStringList technicalObjects(const domain::TweakDefinition& tweak)
{
    QStringList objects;
    if (tweak.detection) objects.append(registryLocation(tweak.detection->location));
    if (tweak.valueDetection) objects.append(registryLocation(tweak.valueDetection->location));
    if (tweak.treeDetection) objects.append(registryLocation(tweak.treeDetection->location));
    if (tweak.appxDetection) objects.append(u"AppX: "_s + tweak.appxDetection->packageName);
    if (tweak.featureDetection) {
        objects.append(u"Feature ID: "_s + QString::number(tweak.featureDetection->featureId));
    }
    if (tweak.scheduledTaskDetection) {
        objects.append(u"Задача: "_s + tweak.scheduledTaskDetection->location.folder
                       + u"\\"_s + tweak.scheduledTaskDetection->location.name);
    }
    if (tweak.bcdDetection) {
        objects.append(u"BCD: "_s + tweak.bcdDetection->spec.objectId + u" / 0x"_s
                       + QString::number(tweak.bcdDetection->spec.elementType, 16));
    }
    if (tweak.powerDetection) {
        objects.append(u"Питание: "_s + tweak.powerDetection->location.subgroup
                       + u" / "_s + tweak.powerDetection->location.setting);
    }
    if (tweak.windowsComponentDetection) {
        objects.append(u"Компонент Windows: "_s
                       + tweak.windowsComponentDetection->target.name);
    }
    for (const auto& state : tweak.states) {
        for (const auto& operation : state.operations) {
            std::visit([&](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, domain::SetRegistryDwordOperation>
                              || std::is_same_v<T, domain::SetRegistryValueOperation>
                              || std::is_same_v<T, domain::DeleteRegistryValueOperation>
                              || std::is_same_v<T, domain::CreateRegistryKeyOperation>
                              || std::is_same_v<T, domain::DeleteRegistryTreeOperation>) {
                    objects.append(registryLocation(value.location));
                } else if constexpr (std::is_same_v<T, domain::RemoveAppxPackageOperation>) {
                    objects.append(u"AppX: "_s + value.packageName);
                } else if constexpr (std::is_same_v<T, domain::FileOperationDefinition>) {
                    objects.append(value.destination);
                } else if constexpr (std::is_same_v<T, domain::SetScheduledTaskEnabledOperation>) {
                    objects.append(u"Задача: "_s + value.location.folder
                                   + u"\\"_s + value.location.name);
                } else if constexpr (std::is_same_v<T, domain::SetBcdElementOperation>) {
                    objects.append(u"BCD: "_s + value.spec.objectId + u" / 0x"_s
                                   + QString::number(value.spec.elementType, 16));
                } else if constexpr (std::is_same_v<T, domain::SetPowerSettingOperation>) {
                    objects.append(u"Питание: "_s + value.location.subgroup
                                   + u" / "_s + value.location.setting);
                } else if constexpr (std::is_same_v<T, domain::SetWindowsComponentStateOperation>) {
                    objects.append(u"Компонент Windows: "_s + value.target.name);
                } else {
                    objects.append(u"Feature ID: "_s + QString::number(value.featureId));
                }
            }, operation);
        }
    }
    objects.removeDuplicates();
    return objects;
}

QString restartId(domain::RestartRequirement restart)
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

QString restartTitle(domain::RestartRequirement restart)
{
    switch (restart) {
    case domain::RestartRequirement::None: return u"Не требуется"_s;
    case domain::RestartRequirement::Explorer: return u"Перезапуск Проводника"_s;
    case domain::RestartRequirement::Service: return u"Перезапуск службы"_s;
    case domain::RestartRequirement::SignOut: return u"Выход из системы"_s;
    case domain::RestartRequirement::Reboot: return u"Перезагрузка ПК"_s;
    }
    return {};
}

QString returnDescription(domain::Reversibility reversibility)
{
    switch (reversibility) {
    case domain::Reversibility::Reversible:
        return u"Возврат: Tweakopedia восстанавливает точное исходное состояние из журнала транзакции."_s;
    case domain::Reversibility::Conditional:
        return u"Возврат: автоматическое восстановление возможно только при наличии полного исходного снимка."_s;
    case domain::Reversibility::Irreversible:
        return u"Возврат: автоматическое восстановление не предусмотрено."_s;
    }
    return {};
}

QStringList operatingSystems(const domain::WindowsCompatibility& compatibility)
{
    QStringList result;
    for (const auto operatingSystem : compatibility.operatingSystems) {
        if (operatingSystem == domain::WindowsFamily::Windows10) result.append(u"Windows 10"_s);
        if (operatingSystem == domain::WindowsFamily::Windows11) result.append(u"Windows 11"_s);
    }
    result.removeDuplicates();
    return result;
}

bool containsId(const QVector<domain::TweakId>& ids, const domain::TweakId& id)
{
    return std::any_of(ids.cbegin(), ids.cend(), [&](const auto& candidate) {
        return candidate == id;
    });
}

QSet<QString> significantWords(const domain::TweakDefinition& tweak)
{
    QString text = (tweak.title + u' ' + tweak.summary).toCaseFolded();
    for (auto& character : text) {
        if (!character.isLetterOrNumber()) character = u' ';
    }
    const QSet<QString> ignored{
        u"windows"_s, u"параметр"_s, u"настройка"_s, u"управляет"_s,
        u"включение"_s, u"отключение"_s,
    };
    QSet<QString> result;
    for (const auto& word : text.split(u' ', Qt::SkipEmptyParts)) {
        if (word.size() >= 4 && !ignored.contains(word)) result.insert(word);
    }
    return result;
}

} // namespace

EncyclopediaArticleModel::EncyclopediaArticleModel(QObject* parent)
    : QObject(parent)
{
}

void EncyclopediaArticleModel::reset(
    const content::TweakCatalog& catalog,
    const content::CategoryCatalog& categories)
{
    tweaks_ = catalog.tweaks();
    categories_ = categories.categories();
    if (selectedArticleId_.isEmpty()) {
        article_.clear();
        emit articleChanged();
        return;
    }
    const auto* selected = find(selectedArticleId_);
    if (selected) article_ = buildArticle(*selected);
    else {
        selectedArticleId_.clear();
        article_.clear();
    }
    emit articleChanged();
}

bool EncyclopediaArticleModel::selectArticle(const QString& id)
{
    const auto* tweak = find(id);
    if (!tweak) {
        clear();
        return false;
    }
    selectedArticleId_ = id;
    article_ = buildArticle(*tweak);
    emit articleChanged();
    return true;
}

void EncyclopediaArticleModel::clear()
{
    if (selectedArticleId_.isEmpty() && article_.isEmpty()) return;
    selectedArticleId_.clear();
    article_.clear();
    emit articleChanged();
}

QVariantMap EncyclopediaArticleModel::article() const
{
    return article_;
}

bool EncyclopediaArticleModel::hasArticle() const noexcept
{
    return !article_.isEmpty();
}

QString EncyclopediaArticleModel::selectedArticleId() const
{
    return selectedArticleId_;
}

const domain::TweakDefinition* EncyclopediaArticleModel::find(QStringView id) const
{
    const auto iterator = std::find_if(tweaks_.cbegin(), tweaks_.cend(), [&](const auto& tweak) {
        return tweak.id.toString() == id;
    });
    return iterator == tweaks_.cend() ? nullptr : &*iterator;
}

QVariantMap EncyclopediaArticleModel::buildArticle(const domain::TweakDefinition& tweak) const
{
    QVariantList sections;
    const auto appendSection = [&](QString id, QString title, const QString& text, bool technical = false) {
        if (text.trimmed().isEmpty()) return;
        sections.append(QVariantMap{
            {u"id"_s, std::move(id)},
            {u"title"_s, std::move(title)},
            {u"text"_s, text},
            {u"technical"_s, technical},
        });
    };
    appendSection(u"purpose"_s, u"Назначение"_s, tweak.explanation.purpose);
    appendSection(u"mechanism"_s, u"Как это работает"_s, tweak.explanation.mechanism);
    appendSection(u"effect"_s, u"Что изменится"_s, tweak.explanation.effect);
    appendSection(u"tradeoffs"_s, u"Ограничения"_s, tweak.explanation.tradeoffs);
    appendSection(u"recommendation"_s, u"Рекомендация"_s, tweak.explanation.recommendation);
    appendSection(u"technical"_s, u"Технические сведения"_s,
                  tweak.explanation.technicalDetails + u"\n\n"_s
                      + returnDescription(tweak.reversibility), true);

    const auto category = categoryTitle(tweak.category);
    const auto subcategory = subcategoryTitle(tweak.category, tweak.subcategory);
    QVariantMap compatibility{
        {u"operatingSystems"_s, operatingSystems(tweak.compatibility)},
        {u"minimumBuild"_s, tweak.compatibility.minimumBuild},
        {u"architectures"_s, QStringList{u"x64"_s}},
    };
    if (tweak.compatibility.maximumBuild) {
        compatibility.insert(u"maximumBuild"_s, *tweak.compatibility.maximumBuild);
    }

    return {
        {u"id"_s, tweak.id.toString()},
        {u"title"_s, tweak.title},
        {u"summary"_s, tweak.summary},
        {u"purpose"_s, tweak.explanation.purpose},
        {u"breadcrumbs"_s, QStringList{u"Твикопедия"_s, category, subcategory}},
        {u"sections"_s, sections},
        {u"technicalObjects"_s, technicalObjects(tweak)},
        {u"compatibility"_s, compatibility},
        {u"restart"_s, QVariantMap{
            {u"id"_s, restartId(tweak.restart)},
            {u"title"_s, restartTitle(tweak.restart)},
            {u"required"_s, tweak.restart != domain::RestartRequirement::None},
        }},
        {u"relatedArticles"_s, relatedArticles(tweak)},
    };
}

QVariantList EncyclopediaArticleModel::relatedArticles(
    const domain::TweakDefinition& tweak) const
{
    struct RankedArticle {
        const domain::TweakDefinition* tweak{};
        int score{};
        int sourceOrder{};
    };
    const auto currentObjectsList = technicalObjects(tweak);
    const auto currentObjects = QSet<QString>(currentObjectsList.cbegin(),
                                               currentObjectsList.cend());
    const auto currentWords = significantWords(tweak);
    QVector<RankedArticle> ranked;
    for (int index = 0; index < tweaks_.size(); ++index) {
        const auto& candidate = tweaks_.at(index);
        if (candidate.id == tweak.id) continue;
        int relevance = 0;
        if (containsId(tweak.dependencies, candidate.id)
            || containsId(tweak.conflicts, candidate.id)
            || containsId(candidate.dependencies, tweak.id)
            || containsId(candidate.conflicts, tweak.id)) {
            relevance += 10000;
        }
        if (candidate.category == tweak.category
            && candidate.subcategory == tweak.subcategory) relevance += 3000;
        else if (candidate.category == tweak.category) relevance += 1500;

        const auto candidateObjectsList = technicalObjects(candidate);
        for (const auto& object : candidateObjectsList) {
            if (currentObjects.contains(object)) {
                relevance += 500;
                break;
            }
        }
        const auto candidateWords = significantWords(candidate);
        int commonWords = 0;
        for (const auto& word : candidateWords) {
            if (currentWords.contains(word)) ++commonWords;
        }
        relevance += commonWords * 20;
        if (relevance > 0) ranked.append({&candidate, relevance, index});
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
        if (left.score != right.score) return left.score > right.score;
        return left.sourceOrder < right.sourceOrder;
    });

    QVariantList result;
    QSet<QString> emitted;
    for (const auto& item : ranked) {
        const auto id = item.tweak->id.toString();
        if (emitted.contains(id)) continue;
        emitted.insert(id);
        result.append(QVariantMap{
            {u"id"_s, id},
            {u"title"_s, item.tweak->title},
            {u"summary"_s, item.tweak->summary},
            {u"path"_s, categoryTitle(item.tweak->category)
                + u" › "_s + subcategoryTitle(item.tweak->category, item.tweak->subcategory)},
        });
        if (result.size() == 6) break;
    }
    return result;
}

QString EncyclopediaArticleModel::categoryTitle(QStringView id) const
{
    const auto iterator = std::find_if(categories_.cbegin(), categories_.cend(),
                                       [&](const auto& category) { return category.id == id; });
    return iterator == categories_.cend() ? u"Другие материалы"_s : iterator->title;
}

QString EncyclopediaArticleModel::subcategoryTitle(
    QStringView categoryId, QStringView id) const
{
    const auto category = std::find_if(categories_.cbegin(), categories_.cend(),
                                       [&](const auto& value) { return value.id == categoryId; });
    if (category == categories_.cend()) return u"Без раздела"_s;
    const auto subcategory = std::find_if(
        category->subcategories.cbegin(), category->subcategories.cend(),
        [&](const auto& value) { return value.id == id; });
    return subcategory == category->subcategories.cend()
        ? u"Без раздела"_s : subcategory->title;
}

} // namespace tweakopedia::app
