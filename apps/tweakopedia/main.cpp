#include "app/AppController.h"
#include "app/SessionLogger.h"
#include "app/SettingsController.h"
#include "content/AppRemovalCatalogLoader.h"
#include "content/TweakCatalogLoader.h"
#include "content/CategoryCatalogLoader.h"
#include "detection/RegistryDwordStateDetector.h"
#include "detection/RegistryValueStateDetector.h"
#include "detection/RegistryTreeStateDetector.h"
#include "detection/AppxPackageStateDetector.h"
#include "detection/FeatureStateDetector.h"
#include "detection/ScheduledTaskStateDetector.h"
#include "detection/BcdStateDetector.h"
#include "detection/PowerSettingStateDetector.h"
#include "detection/WindowsComponentStateDetector.h"
#include "execution/ExecutionProtocol.h"
#include "execution/ExecutorLauncher.h"
#include "execution/ExecutorServer.h"
#include "persistence/AppPaths.h"
#include "persistence/AppSettings.h"
#include "persistence/Database.h"
#include "persistence/TransactionFiles.h"
#include "persistence/PendingInputStore.h"
#include "persistence/TransactionRepository.h"
#include "platform/WindowsRegistryBackend.h"
#include "platform/WindowsAppxPackageProvider.h"
#include "platform/WindowsFeatureStoreBackend.h"
#include "platform/WindowsScheduledTaskBackend.h"
#include "platform/WindowsBcdBackend.h"
#include "platform/WindowsPowerSettingBackend.h"
#include "platform/WindowsComponentBackend.h"
#include "platform/WindowsSystemProfileProvider.h"
#include "platform/WindowsSystemOverviewProvider.h"
#include "UiTypography.h"

#include <QDir>
#include <QCommandLineParser>
#include <QDesktopServices>
#include <QEventLoop>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonObject>
#include <QJsonDocument>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QTemporaryDir>
#include <QSysInfo>
#include <QUrl>
#include <QWindow>
#include <QUuid>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#endif

#include <functional>
#include <memory>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

#ifdef Q_OS_WIN
void applyLightTitleBar(QWindow* window)
{
    if (!window) return;
    const auto handle = reinterpret_cast<HWND>(window->winId());
    const BOOL useDarkMode = FALSE;
    constexpr DWORD useImmersiveDarkMode = 20;
    (void)DwmSetWindowAttribute(
        handle, useImmersiveDarkMode, &useDarkMode, sizeof(useDarkMode));
    const COLORREF captionColor = RGB(247, 248, 252);
    constexpr DWORD captionColorAttribute = 35;
    (void)DwmSetWindowAttribute(
        handle, captionColorAttribute, &captionColor, sizeof(captionColor));
}
#else
void applyLightTitleBar(QWindow*)
{
}
#endif

class DesktopServices final : public app::IAppServices
{
public:
    explicit DesktopServices(QString runtimeRoot, QString dataRoot = {})
        : paths_(std::move(runtimeRoot), std::move(dataRoot))
    {
        if (paths_.ensureDataDirectories() && database_.open(paths_.databasePath())) {
            repository_ = std::make_unique<persistence::TransactionRepository>(database_);
            (void)repository_->markRunningAsInterrupted();
        }
    }

    content::CatalogLoadResult loadCatalog() override
    {
        const auto portableRoot = paths_.contentRoot();
#ifdef TWEAKOPEDIA_SOURCE_CONTENT_ROOT
        const auto root = QFileInfo::exists(portableRoot + u"/categories.yaml"_s)
            ? portableRoot
            : QStringLiteral(TWEAKOPEDIA_SOURCE_CONTENT_ROOT);
#else
        const auto& root = portableRoot;
#endif
        return content::TweakCatalogLoader{}.loadDirectory(root + u"/tweaks"_s);
    }

    content::CatalogLoadResult loadAppRemovalCatalog() override
    {
        const auto portableRoot = paths_.contentRoot();
#ifdef TWEAKOPEDIA_SOURCE_CONTENT_ROOT
        const auto root = QFileInfo::exists(portableRoot + u"/categories.yaml"_s)
            ? portableRoot
            : QStringLiteral(TWEAKOPEDIA_SOURCE_CONTENT_ROOT);
#else
        const auto& root = portableRoot;
#endif
        content::CatalogLoadResult result;
        appxInventory_ = appx_.installedForCurrentUser();
        if (!appxInventory_.error.isEmpty()) {
            result.errors.append({
                .code = u"apps.scan_failed"_s,
                .message = appxInventory_.error,
                .filePath = root + u"/apps.json"_s,
                .line = 1,
                .column = 1,
            });
            return result;
        }
        const auto removals = content::AppRemovalCatalogLoader{}.loadFile(
            root + u"/apps.json"_s, appxInventory_.packageNames());
        if (!removals.errors.isEmpty()) {
            for (const auto& message : removals.errors) {
                result.errors.append({
                    .code = u"apps.catalog_invalid"_s,
                    .message = message,
                    .filePath = root + u"/apps.json"_s,
                    .line = 1,
                    .column = 1,
                });
            }
            return result;
        }
        result.catalog = content::TweakCatalog(removals.tweaks);
        return result;
    }

    content::CategoryCatalogLoadResult loadCategories() override
    {
        const auto portable = paths_.contentRoot() + u"/categories.yaml"_s;
#ifdef TWEAKOPEDIA_SOURCE_CONTENT_ROOT
        const auto path = QFileInfo::exists(portable)
            ? portable
            : QStringLiteral(TWEAKOPEDIA_SOURCE_CONTENT_ROOT) + u"/categories.yaml"_s;
#else
        const auto& path = portable;
#endif
        return content::CategoryCatalogLoader{}.loadFile(path);
    }

    domain::SystemProfile currentProfile() const override { return profile_.current(); }
    domain::SystemOverviewSnapshot systemOverview() const override
    {
        return platform::WindowsSystemOverviewProvider{}.collect();
    }

    domain::DetectedState detect(
        const domain::TweakDefinition& tweak,
        const domain::SystemProfile& profile) const override
    {
        if (tweak.appxDetection) {
            return detection::AppxPackageStateDetector{}.detect(
                tweak, appxInventory_.packages, profile);
        }
        if (tweak.featureDetection) {
            return detection::FeatureStateDetector{}.detect(tweak, features_, profile);
        }
        if (tweak.scheduledTaskDetection) {
            return detection::ScheduledTaskStateDetector{}.detect(tweak, tasks_, profile);
        }
        if (tweak.bcdDetection) {
            return detection::BcdStateDetector{}.detect(tweak, bcd_, profile);
        }
        if (tweak.powerDetection) {
            return detection::PowerSettingStateDetector{}.detect(tweak, power_, profile);
        }
        if (tweak.windowsComponentDetection) {
            return detection::WindowsComponentStateDetector{}.detect(
                tweak, windowsComponents_, profile);
        }
        if (tweak.valueDetection) {
            return detection::RegistryValueStateDetector{}.detect(tweak, registry_, profile);
        }
        if (tweak.treeDetection) {
            return detection::RegistryTreeStateDetector{}.detect(tweak, registry_, profile);
        }
        return detection::RegistryDwordStateDetector{}.detect(tweak, registry_, profile);
    }

    app::AppOperationResult apply(
        const planning::ExecutionPlan& plan,
        const QString& packageName,
        const app::ProgressCallback& progress) override
    {
        if (!repository_) return databaseFailure();
        auto materializedPlan = plan;
        persistence::TransactionFiles files(paths_.transactionsRoot());
        if (!files.create(materializedPlan.transactionId)) {
            return failure(u"transaction.plan_persist_failed"_s, u"Не удалось подготовить файлы транзакции."_s);
        }
        persistence::PendingInputStore pending(paths_.pendingInputsRoot());
        for (auto& operationVariant : materializedPlan.operations) {
            auto* operation = std::get_if<planning::PlannedFileChange>(&operationVariant);
            if (!operation || operation->change.kind == domain::FileOperationKind::Delete) continue;
            auto artifact = operation->change.artifact;
            if (artifact.storageId.isEmpty() || artifact.sha256.isEmpty()) {
                const auto imported = pending.importFile(
                    artifact.id,
                    artifact.managedPath,
                    operation->allowedExtensions,
                    operation->maximumInputSize);
                if (!imported.artifact) {
                    return failure(imported.code, imported.message);
                }
                artifact = *imported.artifact;
            }
            const auto copied = files.materializeInput(materializedPlan.transactionId, artifact);
            if (!copied.artifact) {
                return failure(copied.code, u"Не удалось перенести выбранный файл в транзакцию."_s);
            }
            operation->change.artifact = *copied.artifact;
            operation->inputs.insert(copied.artifact->id, *copied.artifact);
        }
        (void)pending.cleanupUnused({});
        const auto encoded = execution::ExecutionProtocol::encode(materializedPlan);
        if (!files.writePlan(materializedPlan.transactionId,
                QJsonDocument::fromJson(encoded).object())) {
            return failure(u"transaction.plan_persist_failed"_s, u"Не удалось подготовить файлы транзакции."_s);
        }
        const auto directory = files.directory(materializedPlan.transactionId);
        if (!repository_->insert(persistence::TransactionRecord::pending(
                materializedPlan.transactionId, packageName, directory))) {
            return failure(u"transaction.insert_failed"_s, repository_->lastError());
        }

        const auto result = runExecutor(
            materializedPlan.transactionId,
            progress,
            [&](execution::ExecutorServer& server) {
                return server.sendPlan(encoded, paths_.dataRoot(), directory);
            });
        if (result.status != app::AppOperationStatus::Succeeded
            && result.code != u"ipc.disconnected") {
            (void)repository_->updateStatus(plan.transactionId, persistence::TransactionStatus::Failed, result.message);
        }
        return result;
    }

    app::AppOperationResult rollback(
        const QUuid& transactionId,
        const app::ProgressCallback& progress) override
    {
        if (!repository_) return databaseFailure();
        const auto record = repository_->find(transactionId);
        if (!record) return failure(u"transaction.not_found"_s, u"Транзакция отсутствует в истории."_s);
        const auto beforePath = QDir(record->directory).filePath(u"before.json"_s);
        if (!QFileInfo::exists(beforePath)) {
            return failure(u"rollback.snapshot_missing"_s, u"Полный снимок исходных значений отсутствует."_s);
        }
        return runExecutor(
            transactionId,
            progress,
            [&](execution::ExecutorServer& server) {
                return server.sendRollback(
                    transactionId, paths_.dataRoot(), record->directory);
            });
    }

    QVector<persistence::TransactionRecord> history() const override
    {
        if (!repository_) return {};
        auto records = repository_->list();
        persistence::TransactionFiles files(paths_.transactionsRoot());
        for (auto& record : records) {
            const auto result = files.readResult(record.id);
            if (!result) continue;
            const auto status = persistence::transactionStatusFromName(
                result->value(u"status"_s).toString());
            if (status) record.status = *status;
            if (record.error.isEmpty()) record.error = result->value(u"message"_s).toString();
        }
        return records;
    }

    bool restartComputer() override
    {
#ifdef Q_OS_WIN
        return QProcess::startDetached(
            u"shutdown.exe"_s, {u"/r"_s, u"/t"_s, u"0"_s});
#else
        return false;
#endif
    }

    bool databaseReady() const { return repository_ != nullptr && database_.isOpen(); }

private:
    static app::AppOperationResult failure(QString code, QString message)
    {
        return {.status = app::AppOperationStatus::Failed,
                .code = std::move(code), .message = std::move(message)};
    }

    app::AppOperationResult databaseFailure() const
    {
        return failure(u"database.open_failed"_s, database_.lastError());
    }

    app::AppOperationResult runExecutor(
        const QUuid& transactionId,
        const app::ProgressCallback& progress,
        const std::function<bool(execution::ExecutorServer&)>& sendRequest)
    {
        const auto serverName = u"tweakopedia-"_s + QUuid::createUuid().toString(QUuid::WithoutBraces);
        const auto nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
        execution::ExecutorServer server;
        if (!server.listen(serverName, nonce)) {
            return failure(u"ipc.listen_failed"_s, server.errorString());
        }

        QEventLoop loop;
        QJsonObject executorResult;
        QString sessionError;
        bool authenticated{};
        bool completed{};
        QObject::connect(&server, &execution::ExecutorServer::authenticated, &loop, [&](qint64) {
            authenticated = true;
            if (!sendRequest(server)) {
                sessionError = u"ipc.send_failed"_s;
                loop.quit();
            }
        });
        QObject::connect(&server, &execution::ExecutorServer::progressReceived, &loop,
                         [&](int percent, const QString& message) { progress(percent, message); });
        QObject::connect(&server, &execution::ExecutorServer::completed, &loop,
                         [&](const QJsonObject& result) {
                             completed = true;
                             executorResult = result;
                             loop.quit();
                         });
        QObject::connect(&server, &execution::ExecutorServer::clientRejected, &loop,
                         [&](const QString& code) { sessionError = code; loop.quit(); });
        QObject::connect(&server, &execution::ExecutorServer::clientDisconnected, &loop, [&] {
            if (!completed) {
                sessionError = u"ipc.disconnected"_s;
                loop.quit();
            }
        });

        execution::WindowsShellExecuteBackend shell;
        execution::ExecutorLauncher launcher(shell);
        const auto executable = QDir(paths_.applicationDirectory())
            .filePath(u"Tweakopedia.Executor.exe"_s);
        const auto session = launcher.start(executable, serverName, nonce);
        if (!session.started) {
            return {
                .status = session.code == u"launch.cancelled"
                    ? app::AppOperationStatus::Cancelled : app::AppOperationStatus::Failed,
                .code = session.code,
                .message = session.message,
                .transactionId = transactionId,
            };
        }
        server.setExpectedProcessId(session.processId);
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, [&] {
            sessionError = u"ipc.timeout"_s;
            loop.quit();
        });
        timer.start(120000);
        loop.exec();
        timer.stop();

        const auto monitored = launcher.monitor(session, 5000, completed);
        if (!monitored.success && sessionError.isEmpty()) sessionError = monitored.code;
        if (!completed) {
            return failure(
                sessionError.isEmpty() ? u"ipc.no_result"_s : sessionError,
                authenticated ? u"Executor не вернул итоговый результат."_s
                              : u"Не удалось установить проверенный IPC-сеанс."_s);
        }
        const auto status = executorResult.value(u"status"_s).toString();
        if (status == u"succeeded" || status == u"rolled_back") {
            return {.status = app::AppOperationStatus::Succeeded,
                    .transactionId = transactionId};
        }
        return failure(
            executorResult.value(u"code"_s).toString(u"executor.failed"_s),
            executorResult.value(u"message"_s).toString());
    }

    persistence::AppPaths paths_;
    persistence::Database database_;
    std::unique_ptr<persistence::TransactionRepository> repository_;
    platform::WindowsSystemProfileProvider profile_;
    mutable platform::WindowsRegistryBackend registry_;
    platform::WindowsAppxPackageProvider appx_;
    platform::WindowsFeatureStoreBackend features_;
    mutable platform::WindowsScheduledTaskBackend tasks_;
    mutable platform::WindowsBcdBackend bcd_;
    mutable platform::WindowsPowerSettingBackend power_;
    mutable platform::WindowsComponentBackend windowsComponents_;
    platform::AppxPackageQueryResult appxInventory_;
};

class DesktopSettingsServices final : public app::ISettingsServices
{
public:
    DesktopSettingsServices(
        QString settingsPath,
        QString logsRoot,
        QString runtimeRoot,
        QString dataRoot)
        : settings_(std::move(settingsPath))
        , logger_(logsRoot)
        , logsRoot_(QDir::cleanPath(std::move(logsRoot)))
        , runtimeRoot_(QDir::cleanPath(std::move(runtimeRoot)))
        , dataRoot_(QDir::cleanPath(std::move(dataRoot)))
    {
        if (settings_.debugLoggingEnabled()) {
            if (logger_.setEnabled(true)) {
                writeSessionContext();
            } else {
                qWarning().noquote() << "Failed to start debug logging:"
                                     << logger_.errorString();
            }
        }
    }

    bool debugLoggingEnabled() const override
    {
        return logger_.enabled();
    }

    bool setDebugLoggingEnabled(bool enabled) override
    {
        const auto previous = logger_.enabled();
        if (previous == enabled) {
            return settings_.setDebugLoggingEnabled(enabled);
        }

        if (!logger_.setEnabled(enabled)) {
            qWarning().noquote() << "Failed to change debug logging:"
                                 << logger_.errorString();
            return false;
        }

        if (!settings_.setDebugLoggingEnabled(enabled)) {
            (void)logger_.setEnabled(previous);
            return false;
        }

        if (enabled) writeSessionContext();
        return true;
    }

    bool openLogsDirectory() override
    {
        if (!QDir{}.mkpath(logsRoot_)) return false;
        return QDesktopServices::openUrl(QUrl::fromLocalFile(logsRoot_));
    }

private:
    void writeSessionContext() const
    {
        qInfo().noquote() << "Tweakopedia debug logging enabled";
        qInfo().noquote() << "Operating system:" << QSysInfo::prettyProductName()
                          << "architecture:" << QSysInfo::currentCpuArchitecture();
        qDebug().noquote() << "Runtime root:" << runtimeRoot_;
        qDebug().noquote() << "Data root:" << dataRoot_;
        qDebug().noquote() << "Log file:" << logger_.currentFilePath();
    }

    persistence::AppSettings settings_;
    app::SessionLogger logger_;
    QString logsRoot_;
    QString runtimeRoot_;
    QString dataRoot_;
};

} // namespace

int main(int argc, char* argv[])
{
    QQuickStyle::setStyle(u"Basic"_s);
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(u"Tweakopedia"_s);
    QCoreApplication::setOrganizationName(u"Tweakopedia"_s);
    app.setWindowIcon(QIcon(u":/images/tweakopedia-icon.png"_s));
    app.setFont(ui::UiTypography::applicationFont());
    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({u"self-check"_s, u"Проверить portable-комплект и завершиться."_s});
    parser.addOption({u"no-elevation"_s, u"Не запускать операции с повышением прав."_s});
    parser.addOption({u"runtime-root"_s, u"Каталог извлечённого runtime."_s, u"path"_s});
    parser.addOption({u"container-path"_s, u"Путь к исходному EXE-контейнеру."_s, u"path"_s});
    parser.process(app);

    const auto applicationRoot = QDir::cleanPath(QCoreApplication::applicationDirPath());
    auto normalizedExistingPath = [](const QString& path) {
        const QFileInfo info(path);
        const auto canonical = info.canonicalFilePath();
        return QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
    };
    const auto runtimeRoot = parser.isSet(u"runtime-root"_s)
        ? normalizedExistingPath(parser.value(u"runtime-root"_s))
        : applicationRoot;
    if (QString::compare(runtimeRoot, normalizedExistingPath(applicationRoot), Qt::CaseInsensitive) != 0) {
        return 5;
    }
    if (parser.isSet(u"container-path"_s)) {
        const QFileInfo container(parser.value(u"container-path"_s));
        if (!container.isAbsolute() || !container.isFile()) return 6;
    }

    const auto selfCheck = parser.isSet(u"self-check"_s);
    std::unique_ptr<QTemporaryDir> selfCheckData;
    if (selfCheck) {
        selfCheckData = std::make_unique<QTemporaryDir>(
            QDir(qEnvironmentVariable("TEMP")).filePath(u"Tweakopedia-SelfCheck-XXXXXX"_s));
        if (!selfCheckData->isValid() || !parser.isSet(u"no-elevation"_s)) return 2;
    }

    const auto dataRootOverride = selfCheckData ? selfCheckData->path() : QString{};
    const persistence::AppPaths applicationPaths(runtimeRoot, dataRootOverride);
    DesktopSettingsServices settingsServices(
        applicationPaths.settingsPath(),
        applicationPaths.logsRoot(),
        runtimeRoot,
        applicationPaths.dataRoot());
    app::SettingsController settingsController(settingsServices);

    DesktopServices services(runtimeRoot, dataRootOverride);
    app::AppController controller(services);
    const auto started = controller.startup();
    qInfo().noquote() << "Application startup completed:" << started;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(u"appController"_s, &controller);
    engine.rootContext()->setContextProperty(u"settingsController"_s, &settingsController);
    engine.load(QUrl(u"qrc:/qml/Main.qml"_s));
    if (engine.rootObjects().isEmpty()) return 1;
    applyLightTitleBar(qobject_cast<QWindow*>(engine.rootObjects().constFirst()));
    if (selfCheck) {
        const auto profile = services.currentProfile();
        const auto loaded = services.loadCatalog();
        const auto expectedId = domain::TweakId::parse(u"filesystem.win32-long-paths"_s).value();
        const auto supportedProfile = profile.architecture == domain::CpuArchitecture::X64
            && (profile.family == domain::WindowsFamily::Windows10
                || profile.family == domain::WindowsFamily::Windows11)
            && profile.build >= 10240;
        return started && services.databaseReady() && loaded.catalog
                && loaded.catalog->find(expectedId) && supportedProfile
            ? 0 : 3;
    }
    return app.exec();
}
