#include "app/AppController.h"
#include "content/TweakCatalogLoader.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QTest>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

class FakeAppServices final : public app::IAppServices
{
public:
    content::TweakCatalog catalog;
    content::CategoryCatalog categoryCatalog{QVector<content::CategoryDefinition>{{
        .id = u"filesystem"_s,
        .title = u"Накопители, NTFS и файловая система"_s,
        .subcategories = {{.id = u"paths"_s, .title = u"Пути"_s}},
    }}};
    domain::SystemProfile profile{
        .family = domain::WindowsFamily::Windows11,
        .build = 26200,
        .ubr = 1000,
        .edition = u"Professional"_s,
        .architecture = domain::CpuArchitecture::X64,
    };
    domain::DetectedState detected{
        .status = domain::DetectionStatus::Named,
        .stateId = u"disabled"_s,
        .fingerprint = QByteArray(64, 'a'),
    };
    app::AppOperationResult nextApply{.status = app::AppOperationStatus::Cancelled,
                                      .code = u"launch.cancelled"_s};
    QVector<persistence::TransactionRecord> records;
    content::TweakCatalog appRemovalCatalog;
    int appRemovalScans{};
    int applyCalls{};
    int rollbackCalls{};
    int restartCalls{};

    bool restartComputer() override
    {
        ++restartCalls;
        return true;
    }

    content::CatalogLoadResult loadCatalog() override { return {.catalog = catalog}; }
    content::CatalogLoadResult loadAppRemovalCatalog() override
    {
        ++appRemovalScans;
        return {.catalog = appRemovalCatalog};
    }
    content::CategoryCatalogLoadResult loadCategories() override
    {
        return {.catalog = categoryCatalog};
    }
    domain::SystemProfile currentProfile() const override { return profile; }
    domain::DetectedState detect(
        const domain::TweakDefinition&,
        const domain::SystemProfile&) const override { return detected; }
    domain::SystemOverviewSnapshot systemOverview() const override
    {
        domain::SystemOverviewSnapshot result;
        result.displayName = u"Иван"_s;
        result.computerName = u"DESKTOP-TEST"_s;
        result.os.caption = u"Windows 11 Pro"_s;
        result.os.version = u"10.0.26200"_s;
        result.os.buildNumber = 26200;
        result.os.architecture = u"x64"_s;
        result.processor.name = u"AMD Ryzen"_s;
        result.processor.coreCount = 8;
        result.processor.logicalProcessorCount = 16;
        result.memory.totalBytes = 16ULL * 1024 * 1024 * 1024;
        return result;
    }

    app::AppOperationResult apply(
        const planning::ExecutionPlan& plan,
        const QString& packageName,
        const app::ProgressCallback& progress) override
    {
        ++applyCalls;
        progress(50, u"Выполнение"_s);
        if (nextApply.status == app::AppOperationStatus::Succeeded) {
            detected.stateId = u"enabled"_s;
            auto record = persistence::TransactionRecord::pending(
                plan.transactionId, packageName, u"D:/ChatGPT/Temp/Tweakopedia/fake"_s);
            record.status = persistence::TransactionStatus::Succeeded;
            records.append(record);
            nextApply.transactionId = plan.transactionId;
        }
        return nextApply;
    }

    app::AppOperationResult rollback(
        const QUuid& transactionId,
        const app::ProgressCallback& progress) override
    {
        ++rollbackCalls;
        progress(50, u"Возврат"_s);
        detected.stateId = u"disabled"_s;
        for (auto& record : records) {
            if (record.id == transactionId) record.status = persistence::TransactionStatus::RolledBack;
        }
        return {.status = app::AppOperationStatus::Succeeded, .transactionId = transactionId};
    }

    QVector<persistence::TransactionRecord> history() const override { return records; }
};

FakeAppServices services()
{
    FakeAppServices result;
    const auto loaded = content::TweakCatalogLoader{}.loadDirectory(
        QStringLiteral(TWEAKOPEDIA_TEST_CONTENT_ROOT));
    Q_ASSERT(loaded.catalog.has_value());
    const auto id = *domain::TweakId::parse(u"filesystem.win32-long-paths");
    const auto* tweak = loaded.catalog->find(id);
    Q_ASSERT(tweak != nullptr);
    result.catalog = content::TweakCatalog({*tweak});
    return result;
}

} // namespace

class AppControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void startupExposesCategoriesAndFilteredTweaks()
    {
        auto backend = services();
        app::AppController controller(backend);

        QVERIFY(controller.startup());
        QCOMPARE(controller.categories()->rowCount(), 2);
        QCOMPARE(controller.categories()->data(
                     controller.categories()->index(0), app::CategoryListModel::TitleRole).toString(),
                 u"Все категории"_s);
        QCOMPARE(controller.categories()->data(
                     controller.categories()->index(1), app::CategoryListModel::IdRole).toString(),
                 u"filesystem"_s);
        QCOMPARE(controller.filteredTweaks()->rowCount(), 1);
    }

    void hidingUnsupportedTweaksRemovesEmptyCategoriesAndResetsSelection()
    {
        auto backend = services();
        auto supported = backend.catalog.tweaks().first();
        auto unsupported = supported;
        unsupported.id = *domain::TweakId::parse(u"experimental.future-feature"_s);
        unsupported.title = u"Будущая функция"_s;
        unsupported.category = u"experimental"_s;
        unsupported.subcategory = u"feature-store"_s;
        unsupported.compatibility.minimumBuild = 99999;
        backend.catalog = content::TweakCatalog({supported, unsupported});
        backend.categoryCatalog = content::CategoryCatalog({
            {.id = u"filesystem"_s, .title = u"Файловая система"_s},
            {.id = u"experimental"_s, .title = u"Экспериментальные функции"_s},
            {.id = u"app-removal"_s, .title = u"Удаление приложений"_s},
        });
        app::AppController controller(backend);

        QVERIFY(controller.startup());
        QCOMPARE(controller.categories()->rowCount(), 4);
        controller.setTweakCategory(u"experimental"_s);
        QCOMPARE(controller.filteredTweaks()->rowCount(), 1);

        controller.setHideUnsupportedTweaks(true);

        QCOMPARE(controller.categories()->rowCount(), 3);
        QCOMPARE(controller.categories()->data(
                     controller.categories()->index(0), app::CategoryListModel::IdRole).toString(),
                 QString{});
        QCOMPARE(controller.categories()->data(
                     controller.categories()->index(1), app::CategoryListModel::IdRole).toString(),
                 u"filesystem"_s);
        QCOMPARE(controller.categories()->data(
                     controller.categories()->index(2), app::CategoryListModel::IdRole).toString(),
                 u"app-removal"_s);
        QCOMPARE(controller.filteredTweaks()->categoryId(), QString{});
        QCOMPARE(controller.filteredTweaks()->rowCount(), 1);

        controller.setHideUnsupportedTweaks(false);
        QCOMPARE(controller.categories()->rowCount(), 4);
    }

    void appRemovalCatalogIsLoadedOnlyAfterExplicitScan()
    {
        auto backend = services();
        auto removal = backend.catalog.tweaks().first();
        removal.id = *domain::TweakId::parse(u"app-removal.remove.clipchamp.clipchamp"_s);
        removal.title = u"Удалить Clipchamp"_s;
        removal.category = u"app-removal"_s;
        removal.subcategory = u"installed"_s;
        backend.appRemovalCatalog = content::TweakCatalog({removal});
        app::AppController controller(backend);

        QVERIFY(controller.startup());
        QCOMPARE(backend.appRemovalScans, 0);
        QCOMPARE(controller.appRemovalScanStatus(), u"idle"_s);
        QCOMPARE(controller.tweaks()->rowCount(), 1);

        controller.scanInstalledApps();
        QTRY_COMPARE(controller.appRemovalScanStatus(), u"succeeded"_s);
        QCOMPARE(backend.appRemovalScans, 1);
        QCOMPARE(controller.tweaks()->rowCount(), 2);
    }

    void returningToggleToActualStateRemovesQueueItem()
    {
        auto backend = services();
        app::AppController controller(backend);
        QVERIFY(controller.startup());
        QSignalSpy countChanged(controller.queue(), &app::QueueListModel::countChanged);

        QVERIFY(controller.selectTarget(u"filesystem.win32-long-paths"_s, u"enabled"_s));
        QCOMPARE(controller.queue()->rowCount(), 1);
        QCOMPARE(controller.queue()->count(), 1);
        QCOMPARE(countChanged.count(), 1);
        QCOMPARE(controller.tweaks()->data(
                     controller.tweaks()->index(0), app::TweakListModel::TargetStateRole).toString(),
                 u"enabled"_s);

        QVERIFY(controller.selectTarget(u"filesystem.win32-long-paths"_s, u"disabled"_s));
        QCOMPARE(controller.queue()->rowCount(), 0);
        QCOMPARE(controller.queue()->count(), 0);
        QCOMPARE(countChanged.count(), 2);
        QCOMPARE(controller.tweaks()->data(
                     controller.tweaks()->index(0), app::TweakListModel::TargetStateRole).toString(),
                 QString{});
        QVERIFY(!controller.tweaks()->data(
                     controller.tweaks()->index(0), app::TweakListModel::PendingRole).toBool());

        QVERIFY(controller.selectTarget(u"filesystem.win32-long-paths"_s, u"disabled"_s));
        QCOMPARE(controller.queue()->rowCount(), 0);
    }

    void startupQueuePreviewCancelSuccessAndRollback()
    {
        auto backend = services();
        app::AppController controller(backend);

        QVERIFY(controller.startup());
        QCOMPARE(controller.tweaks()->rowCount(), 1);
        QCOMPARE(controller.tweaks()->data(
                     controller.tweaks()->index(0), app::TweakListModel::CurrentStateRole).toString(),
                 u"disabled"_s);

        QVERIFY(controller.selectTarget(u"filesystem.win32-long-paths"_s, u"enabled"_s));
        QCOMPARE(backend.applyCalls, 0);
        QCOMPARE(controller.queue()->rowCount(), 1);
        QCOMPARE(controller.tweaks()->data(
                     controller.tweaks()->index(0), app::TweakListModel::CurrentStateRole).toString(),
                 u"disabled"_s);
        QVERIFY(controller.buildPreview());
        QVERIFY(controller.previewSummary().contains(u"1"));

        QVERIFY(!controller.applyQueue(u"Мой пакет"_s));
        QCOMPARE(controller.lastErrorCode(), u"launch.cancelled"_s);
        QCOMPARE(controller.queue()->rowCount(), 1);

        backend.nextApply = {.status = app::AppOperationStatus::Succeeded};
        QVERIFY(controller.applyQueue(u"Мой пакет"_s));
        QCOMPARE(controller.queue()->rowCount(), 0);
        QCOMPARE(controller.history()->rowCount(), 1);
        QCOMPARE(controller.tweaks()->data(
                     controller.tweaks()->index(0), app::TweakListModel::CurrentStateRole).toString(),
                 u"enabled"_s);
        QCOMPARE(controller.tweaks()->data(
                     controller.tweaks()->index(0), app::TweakListModel::TargetStateRole).toString(),
                 QString{});

        const auto transactionId = backend.records.first().id.toString(QUuid::WithoutBraces);
        QVERIFY(controller.rollback(transactionId));
        QCOMPARE(backend.rollbackCalls, 1);
        QCOMPARE(controller.tweaks()->data(
                     controller.tweaks()->index(0), app::TweakListModel::CurrentStateRole).toString(),
                 u"disabled"_s);
    }

    void opensCompleteExplanationWithoutArticleLinks()
    {
        auto backend = services();
        app::AppController controller(backend);
        QVERIFY(controller.startup());

        const auto explanation = controller.openExplanation(u"filesystem.win32-long-paths"_s);

        QVERIFY(!explanation.value(u"purpose"_s).toString().isEmpty());
        QVERIFY(!explanation.value(u"mechanism"_s).toString().isEmpty());
        QVERIFY(!explanation.contains(u"article"_s));
        QVERIFY(!explanation.contains(u"sources"_s));
    }

    void previewsAppxRemovalAndExplainsConditionalReturn()
    {
        auto backend = services();
        domain::TweakDefinition tweak;
        tweak.id = *domain::TweakId::parse(u"apps.remove.clipchamp.clipchamp"_s);
        tweak.title = u"Удалить Clipchamp"_s;
        tweak.category = u"apps"_s;
        tweak.subcategory = u"removal"_s;
        tweak.kind = domain::TweakKind::Action;
        tweak.summary = u"Видеоредактор Microsoft"_s;
        tweak.explanation = {
            u"Удаляет приложение."_s, u"Использует AppX."_s, u"Пакет исчезает."_s,
            u"Данные могут быть удалены."_s, u"Удалять, если не нужен."_s,
            u"Пакет Clipchamp.Clipchamp."_s,
        };
        tweak.compatibility = {
            .architectures = {domain::CpuArchitecture::X64},
            .operatingSystems = {domain::WindowsFamily::Windows11},
            .minimumBuild = 22000,
        };
        tweak.states = {{
            .id = u"remove"_s,
            .title = u"Удалить"_s,
            .operations = {domain::RemoveAppxPackageOperation{u"Clipchamp.Clipchamp"_s}},
        }};
        tweak.appxDetection = domain::AppxPackageDetection{u"Clipchamp.Clipchamp"_s};
        tweak.reversibility = domain::Reversibility::Conditional;
        backend.catalog = content::TweakCatalog({tweak});
        backend.detected = {
            .status = domain::DetectionStatus::Named,
            .stateId = u"installed"_s,
            .fingerprint = QByteArray(64, 'c'),
        };
        app::AppController controller(backend);

        QVERIFY(controller.startup());
        QVERIFY(controller.selectTarget(tweak.id.toString(), u"remove"_s));
        QVERIFY(controller.buildPreview());
        const auto operations = controller.previewOperations();
        QCOMPARE(operations.size(), 1);
        QCOMPARE(operations.first().toMap().value(u"registryObject"_s).toString(),
                 u"AppX: Clipchamp.Clipchamp"_s);
        const auto explanation = controller.openExplanation(tweak.id.toString());
        QVERIFY(explanation.value(u"rollback"_s).toString().contains(u"повторная установка"_s));
    }

    void successfulRebootPlanExposesRequirementAndRestartCommand()
    {
        auto backend = services();
        const auto loaded = content::TweakCatalogLoader{}.loadDirectory(
            QStringLiteral(TWEAKOPEDIA_TEST_CONTENT_ROOT));
        QVERIFY(loaded.catalog.has_value());
        const auto id = *domain::TweakId::parse(u"boot.verbose-logon-messages");
        const auto* tweak = loaded.catalog->find(id);
        QVERIFY(tweak != nullptr);
        backend.catalog = content::TweakCatalog({*tweak});
        backend.nextApply = {.status = app::AppOperationStatus::Succeeded};
        app::AppController controller(backend);

        QVERIFY(controller.startup());
        QVERIFY(controller.selectTarget(id.toString(), u"enabled"_s));
        QVERIFY(controller.applyQueue(u"Пакет перезагрузки"_s));
        QVERIFY(controller.property("rebootRequired").toBool());

        bool restarted{};
        QVERIFY(QMetaObject::invokeMethod(
            &controller, "restartComputer", Qt::DirectConnection,
            Q_RETURN_ARG(bool, restarted)));
        QVERIFY(restarted);
        QCOMPARE(backend.restartCalls, 1);

        QVERIFY(controller.selectTarget(id.toString(), u"disabled"_s));
        QCOMPARE(controller.property("applyStatus").toString(), u"idle"_s);
        QVERIFY(!controller.property("rebootRequired").toBool());
    }

    void interruptedTransactionWithSnapshotCanBeRolledBack()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto directory = root.filePath(u"transaction"_s);
        QVERIFY(QDir{}.mkpath(directory));
        QFile before(QDir(directory).filePath(u"before.json"_s));
        QVERIFY(before.open(QIODevice::WriteOnly));
        QVERIFY(before.write(R"({"operations":[{"snapshot":true}]})") > 0);
        before.close();

        auto record = persistence::TransactionRecord::pending(
            QUuid::createUuid(), u"Прерванный пакет"_s, directory);
        record.status = persistence::TransactionStatus::Interrupted;
        app::HistoryListModel model;
        model.reset({record});

        QVERIFY(model.data(model.index(0), app::HistoryListModel::CanRollbackRole).toBool());
    }

    void appxRemovalSnapshotIsNotAdvertisedAsAutomaticallyReversible()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto directory = root.filePath(u"transaction"_s);
        QVERIFY(QDir{}.mkpath(directory));
        QFile before(QDir(directory).filePath(u"before.json"_s));
        QVERIFY(before.open(QIODevice::WriteOnly));
        QVERIFY(before.write(R"({"operations":[{"type":"appx.packages","packages":[]}]})") > 0);
        before.close();

        auto record = persistence::TransactionRecord::pending(
            QUuid::createUuid(), u"Удаление приложения"_s, directory);
        record.status = persistence::TransactionStatus::Succeeded;
        app::HistoryListModel model;
        model.reset({record});

        QVERIFY(!model.data(model.index(0), app::HistoryListModel::CanRollbackRole).toBool());
    }
};

QTEST_GUILESS_MAIN(AppControllerTest)

#include "AppControllerTest.moc"
