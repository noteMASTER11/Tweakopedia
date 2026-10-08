#include "app/AppController.h"
#include "content/TweakCatalogLoader.h"
#include "detection/RegistryDwordStateDetector.h"
#include "execution/ExecutionProtocol.h"
#include "execution/ExecutorLauncher.h"
#include "execution/ExecutorServer.h"
#include "persistence/AppPaths.h"
#include "persistence/Database.h"
#include "persistence/TransactionFiles.h"
#include "persistence/TransactionRepository.h"
#include "platform/WindowsRegistryBackend.h"
#include "platform/WindowsSystemProfileProvider.h"

#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonObject>
#include <QJsonDocument>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <QUuid>

#include <functional>
#include <memory>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

class DesktopServices final : public app::IAppServices
{
public:
    DesktopServices()
        : paths_(QCoreApplication::applicationDirPath())
    {
        if (paths_.ensureDataDirectories() && database_.open(paths_.databasePath())) {
            repository_ = std::make_unique<persistence::TransactionRepository>(database_);
            (void)repository_->markRunningAsInterrupted();
        }
    }

    content::CatalogLoadResult loadCatalog() override
    {
        const auto portable = paths_.contentRoot() + u"/tweaks"_s;
        const auto root = QFileInfo::exists(portable)
            ? portable
            : QStringLiteral(TWEAKOPEDIA_SOURCE_CONTENT_ROOT);
        return content::TweakCatalogLoader{}.loadDirectory(root);
    }

    domain::SystemProfile currentProfile() const override { return profile_.current(); }

    domain::DetectedState detect(
        const domain::TweakDefinition& tweak,
        const domain::SystemProfile& profile) const override
    {
        return detection::RegistryDwordStateDetector{}.detect(tweak, registry_, profile);
    }

    app::AppOperationResult apply(
        const planning::ExecutionPlan& plan,
        const QString& packageName,
        const app::ProgressCallback& progress) override
    {
        if (!repository_) return databaseFailure();
        persistence::TransactionFiles files(paths_.transactionsRoot());
        if (!files.create(plan.transactionId)
            || !files.writePlan(plan.transactionId,
                QJsonDocument::fromJson(execution::ExecutionProtocol::encode(plan)).object())) {
            return failure(u"transaction.plan_persist_failed"_s, u"Не удалось подготовить файлы транзакции."_s);
        }
        const auto directory = files.directory(plan.transactionId);
        if (!repository_->insert(persistence::TransactionRecord::pending(
                plan.transactionId, packageName, directory))) {
            return failure(u"transaction.insert_failed"_s, repository_->lastError());
        }

        const auto encoded = execution::ExecutionProtocol::encode(plan);
        const auto result = runExecutor(
            plan.transactionId,
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
        const auto executable = QDir(QCoreApplication::applicationDirPath())
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
};

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(u"Tweakopedia"_s);
    QCoreApplication::setOrganizationName(u"Tweakopedia"_s);

    DesktopServices services;
    app::AppController controller(services);
    (void)controller.startup();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(u"appController"_s, &controller);
    engine.load(QUrl(u"qrc:/qml/Main.qml"_s));
    if (engine.rootObjects().isEmpty()) return 1;
    return app.exec();
}
