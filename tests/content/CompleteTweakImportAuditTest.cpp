#include "content/AppRemovalCatalogLoader.h"
#include "content/TweakCatalogLoader.h"
#include "domain/TweakId.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QtTest/QTest>

using namespace Qt::StringLiterals;
using tweakopedia::content::AppRemovalCatalogLoader;
using tweakopedia::content::TweakCatalogLoader;
using tweakopedia::domain::TweakId;

namespace {

QJsonObject loadManifest()
{
    QFile file(QString::fromUtf8(TWEAKOPEDIA_IMPORT_AUDIT));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }
    return document.object();
}

QSet<QString> catalogIds()
{
    const auto result = TweakCatalogLoader{}.loadDirectory(
        QDir(QString::fromUtf8(TWEAKOPEDIA_CONTENT_ROOT)).filePath(u"tweaks"_s));
    if (!result.catalog.has_value()) {
        return {};
    }
    QSet<QString> ids;
    for (const auto& tweak : result.catalog->tweaks()) {
        ids.insert(tweak.id.toString());
    }

    const auto appsPath = QDir(QString::fromUtf8(TWEAKOPEDIA_CONTENT_ROOT)).filePath(u"apps.json"_s);
    const auto metadata = AppRemovalCatalogLoader{}.readMetadata(appsPath);
    QSet<QString> installed;
    for (const auto& app : metadata.apps) {
        installed.insert(app.packageName);
    }
    const auto appCatalog = AppRemovalCatalogLoader{}.loadFile(appsPath, installed);
    for (const auto& tweak : appCatalog.tweaks) {
        ids.insert(tweak.id.toString());
    }
    return ids;
}

} // namespace

class CompleteTweakImportAuditTest final : public QObject
{
    Q_OBJECT

private slots:
    void manifestUsesSupportedSchema()
    {
        const auto manifest = loadManifest();
        QVERIFY2(!manifest.isEmpty(), "complete-tweak-import.json is missing or invalid");
        QCOMPARE(manifest.value(u"schema"_s).toString(),
                 u"tweakopedia.complete-import-audit/1"_s);
        QVERIFY(manifest.value(u"candidates"_s).isArray());
        QVERIFY(!manifest.value(u"candidates"_s).toArray().isEmpty());
    }

    void candidateKeysAreUnique()
    {
        const auto candidates = loadManifest().value(u"candidates"_s).toArray();
        QSet<QString> keys;
        for (const auto& value : candidates) {
            const auto key = value.toObject().value(u"candidate_key"_s).toString();
            QVERIFY2(!key.isEmpty(), "candidate_key must not be empty");
            QVERIFY2(!keys.contains(key), qPrintable(u"duplicate candidate_key: "_s + key));
            keys.insert(key);
        }
    }

    void everyCandidateHasDecision()
    {
        const auto candidates = loadManifest().value(u"candidates"_s).toArray();
        const QSet<QString> decisions{u"accepted"_s, u"duplicate"_s, u"rejected"_s};
        for (const auto& value : candidates) {
            const auto candidate = value.toObject();
            const auto decision = candidate.value(u"decision"_s).toString();
            QVERIFY2(decisions.contains(decision), qPrintable(
                u"invalid decision for "_s + candidate.value(u"candidate_key"_s).toString()));
            QVERIFY(!candidate.value(u"object_type"_s).toString().isEmpty());
            QVERIFY(!candidate.value(u"normalized_object"_s).toString().isEmpty());
            if (decision == u"rejected"_s) {
                QVERIFY(!candidate.value(u"reason_code"_s).toString().isEmpty());
            } else {
                QVERIFY(TweakId::parse(candidate.value(u"tweak_id"_s).toString()).has_value());
            }
        }
    }

    void acceptedEntriesHaveStablePlannedIds()
    {
        const auto candidates = loadManifest().value(u"candidates"_s).toArray();
        QSet<QString> acceptedIds;
        for (const auto& value : candidates) {
            const auto candidate = value.toObject();
            if (candidate.value(u"decision"_s).toString() != u"accepted"_s) {
                continue;
            }
            const auto id = candidate.value(u"tweak_id"_s).toString();
            QVERIFY(TweakId::parse(id).has_value());
            QVERIFY2(!acceptedIds.contains(id), qPrintable(u"duplicate accepted tweak_id: "_s + id));
            acceptedIds.insert(id);
        }
    }

    void everyAcceptedEntryExistsInCatalog()
    {
        const auto existingIds = catalogIds();
        QVERIFY(!existingIds.isEmpty());
        const auto candidates = loadManifest().value(u"candidates"_s).toArray();
        QStringList missing;
        for (const auto& value : candidates) {
            const auto candidate = value.toObject();
            if (candidate.value(u"decision"_s).toString() != u"accepted"_s) continue;
            const auto id = candidate.value(u"tweak_id"_s).toString();
            if (!existingIds.contains(id)) missing.append(id);
        }
        QVERIFY2(missing.isEmpty(), qPrintable(
            u"accepted IDs missing from catalog ("_s
            + QString::number(missing.size()) + u"): "_s
            + missing.mid(0, 25).join(u", "_s)));
    }

    void acceptedRegistryObjectsAreUnique()
    {
        const auto candidates = loadManifest().value(u"candidates"_s).toArray();
        QSet<QString> objects;
        for (const auto& value : candidates) {
            const auto candidate = value.toObject();
            const auto objectType = candidate.value(u"object_type"_s).toString();
            if (candidate.value(u"decision"_s).toString() != u"accepted"_s
                || (objectType != u"registry_value"_s
                    && objectType != u"registry_tree"_s)) {
                continue;
            }
            const auto object = candidate.value(u"normalized_object"_s)
                                    .toString().toCaseFolded();
            QVERIFY2(!objects.contains(object), qPrintable(
                u"accepted registry object collision: "_s + object));
            objects.insert(object);
        }
    }

    void acceptedImplementationsUseDeclaredObjectType()
    {
        const auto loaded = TweakCatalogLoader{}.loadDirectory(
            QDir(QString::fromUtf8(TWEAKOPEDIA_CONTENT_ROOT)).filePath(u"tweaks"_s));
        QVERIFY(loaded.catalog.has_value());
        const auto candidates = loadManifest().value(u"candidates"_s).toArray();
        for (const auto& value : candidates) {
            const auto candidate = value.toObject();
            if (candidate.value(u"decision"_s).toString() != u"accepted"_s) continue;
            const auto parsedId = TweakId::parse(candidate.value(u"tweak_id"_s).toString());
            QVERIFY(parsedId.has_value());
            const auto* tweak = loaded.catalog->find(*parsedId);
            QVERIFY(tweak != nullptr);
            const auto type = candidate.value(u"object_type"_s).toString();
            if (type == u"registry_value") QVERIFY(tweak->valueDetection.has_value());
            else if (type == u"scheduled_task") QVERIFY(tweak->scheduledTaskDetection.has_value());
            else if (type == u"bcd_element") QVERIFY(tweak->bcdDetection.has_value());
            else if (type == u"power_setting") QVERIFY(tweak->powerDetection.has_value());
            else if (type == u"windows_feature") {
                QVERIFY(tweak->windowsComponentDetection.has_value());
                QCOMPARE(tweak->windowsComponentDetection->target.kind,
                         tweakopedia::domain::WindowsComponentKind::Feature);
            } else {
                QFAIL(qPrintable(u"accepted object type has no coverage contract: "_s + type));
            }
        }
    }

    void duplicateEntriesReferenceExistingTweak()
    {
        const auto existingIds = catalogIds();
        QVERIFY(!existingIds.isEmpty());
        const auto candidates = loadManifest().value(u"candidates"_s).toArray();
        for (const auto& value : candidates) {
            const auto candidate = value.toObject();
            if (candidate.value(u"decision"_s).toString() != u"duplicate"_s) {
                continue;
            }
            const auto id = candidate.value(u"tweak_id"_s).toString();
            QVERIFY2(existingIds.contains(id), qPrintable(u"unknown duplicate tweak_id: "_s + id));
        }
    }

    void serviceCandidatesAreRejected()
    {
        const auto candidates = loadManifest().value(u"candidates"_s).toArray();
        for (const auto& value : candidates) {
            const auto candidate = value.toObject();
            if (candidate.value(u"object_type"_s).toString() != u"service"_s) {
                continue;
            }
            QCOMPARE(candidate.value(u"decision"_s).toString(), u"rejected"_s);
            QCOMPARE(candidate.value(u"reason_code"_s).toString(), u"service-management"_s);
        }
    }

    void normalizesRegistryAliasesAndControlSets()
    {
        const auto candidates = loadManifest().value(u"candidates"_s).toArray();
        for (const auto& value : candidates) {
            const auto candidate = value.toObject();
            if (!candidate.value(u"object_type"_s).toString().startsWith(u"registry"_s)) {
                continue;
            }
            const auto object = candidate.value(u"normalized_object"_s).toString();
            QVERIFY2(!object.contains(u"HKEY_LOCAL_MACHINE"_s, Qt::CaseInsensitive), qPrintable(object));
            QVERIFY2(!object.contains(u"HKEY_CURRENT_USER"_s, Qt::CaseInsensitive), qPrintable(object));
            QVERIFY2(!object.contains(u"ControlSet001"_s, Qt::CaseInsensitive), qPrintable(object));
        }
    }
};

QTEST_MAIN(CompleteTweakImportAuditTest)
#include "CompleteTweakImportAuditTest.moc"
