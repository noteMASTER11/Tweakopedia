#include "content/AppRemovalCatalogLoader.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace tweakopedia::content {
namespace {

QString normalizedId(QString packageName)
{
    packageName = packageName.toLower();
    packageName.replace(QRegularExpression(u"[^a-z0-9.]"_s), u"-"_s);
    packageName.replace(QRegularExpression(u"-+"_s), u"-"_s);
    packageName.replace(QRegularExpression(u"\\.+"_s), u"."_s);
    return u"apps.remove."_s + packageName;
}

domain::Impact impactFor(QStringView recommendation)
{
    if (recommendation == u"unsafe") return domain::Impact::Critical;
    if (recommendation == u"optional") return domain::Impact::Medium;
    return domain::Impact::Low;
}

QString recommendationText(QStringView recommendation)
{
    if (recommendation == u"unsafe") {
        return u"Удаляйте только при точном понимании зависимостей: пакет используется другими компонентами Windows."_s;
    }
    if (recommendation == u"optional") {
        return u"Удаление уместно, если приложение и связанные с ним функции не используются."_s;
    }
    return u"Можно удалить, если приложение не требуется; перед применением проверьте описание и зависимости."_s;
}

domain::TweakDefinition makeDefinition(const AppRemovalMetadata& app)
{
    domain::TweakDefinition tweak;
    tweak.id = *domain::TweakId::parse(normalizedId(app.packageName));
    tweak.title = u"Удалить "_s + app.friendlyName;
    tweak.category = u"app-removal"_s;
    tweak.subcategory = u"installed"_s;
    tweak.kind = domain::TweakKind::Action;
    tweak.summary = app.description;
    tweak.explanation = {
        .purpose = u"Удаляет приложение "_s + app.friendlyName + u" из текущей учётной записи Windows."_s,
        .mechanism = u"Tweakopedia повторно находит установленный AppX-пакет по его постоянному имени и передаёт точное полное имя пакета системному механизму удаления."_s,
        .effect = app.description,
        .tradeoffs = u"Локальные данные приложения могут быть удалены. Для возврата обычно потребуется повторная установка пакета."_s,
        .recommendation = recommendationText(app.recommendation),
        .technicalDetails = u"Постоянное имя AppX-пакета: "_s + app.packageName,
    };
    tweak.compatibility = {
        .architectures = {domain::CpuArchitecture::X64},
        .operatingSystems = {domain::WindowsFamily::Windows10, domain::WindowsFamily::Windows11},
        .minimumBuild = 10240,
    };
    tweak.states = {{
        .id = u"remove"_s,
        .title = u"Удалить"_s,
        .operations = {domain::RemoveAppxPackageOperation{.packageName = app.packageName}},
    }};
    tweak.appxDetection = domain::AppxPackageDetection{.packageName = app.packageName};
    tweak.impact = impactFor(app.recommendation);
    tweak.reversibility = domain::Reversibility::Conditional;
    return tweak;
}

} // namespace

AppRemovalMetadataResult AppRemovalCatalogLoader::readMetadata(const QString& path) const
{
    AppRemovalMetadataResult result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.errors.append(u"Не удалось открыть каталог приложений: "_s + path);
        return result;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.errors.append(u"Некорректный JSON каталога приложений."_s);
        return result;
    }
    const auto apps = document.object().value(u"Apps"_s);
    if (!apps.isArray()) {
        result.errors.append(u"Поле Apps отсутствует или не является массивом."_s);
        return result;
    }
    QSet<QString> packageNames;
    for (const auto& value : apps.toArray()) {
        if (!value.isObject()) continue;
        const auto object = value.toObject();
        if (object.value(u"RemovalMethod"_s).toString() != u"Appx") continue;
        const auto packageName = object.value(u"AppId"_s).toString().trimmed();
        const auto friendlyName = object.value(u"FriendlyName"_s).toString().trimmed();
        const auto description = object.value(u"Description"_s).toString().trimmed();
        const auto recommendation = object.value(u"Recommendation"_s).toString().trimmed();
        if (packageName.isEmpty() || friendlyName.isEmpty() || description.isEmpty()) {
            result.errors.append(u"Запись AppX содержит пустое обязательное поле."_s);
            continue;
        }
        const auto normalizedPackageName = packageName.toLower();
        if (packageNames.contains(normalizedPackageName)) {
            result.errors.append(u"Дублирующее имя AppX-пакета: "_s + packageName);
            continue;
        }
        packageNames.insert(normalizedPackageName);
        result.apps.append({friendlyName, packageName, description, recommendation});
    }
    return result;
}

AppRemovalCatalogResult AppRemovalCatalogLoader::loadFile(
    const QString& path,
    const QSet<QString>& installedPackageNames) const
{
    const auto metadata = readMetadata(path);
    AppRemovalCatalogResult result{.errors = metadata.errors};
    if (!result.errors.isEmpty()) return result;

    QSet<QString> installed;
    for (const auto& name : installedPackageNames) installed.insert(name.toLower());
    for (const auto& app : metadata.apps) {
        if (!installed.contains(app.packageName.toLower())) continue;
        auto tweak = makeDefinition(app);
        const auto validation = tweak.validationErrors();
        if (!validation.isEmpty()) {
            result.errors.append(u"Некорректная запись "_s + app.packageName + u": "_s
                                 + validation.join(u", "_s));
            continue;
        }
        result.tweaks.append(std::move(tweak));
    }
    return result;
}

} // namespace tweakopedia::content
