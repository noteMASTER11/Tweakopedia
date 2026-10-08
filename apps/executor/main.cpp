#include "execution/ExecutionProtocol.h"
#include "execution/ExecutorClient.h"
#include "execution/PlanValidator.h"
#include "execution/RegistryDwordExecutor.h"
#include "execution/RegistrySnapshot.h"
#include "execution/TransactionRunner.h"
#include "persistence/AppPaths.h"
#include "persistence/Database.h"
#include "persistence/TransactionFiles.h"
#include "persistence/TransactionRecord.h"
#include "persistence/TransactionRepository.h"
#include "platform/WindowsRegistryBackend.h"
#include "platform/WindowsAppxPackageProvider.h"
#include "platform/WindowsSystemProfileProvider.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>
#include <QtEndian>

#include <memory>

using namespace tweakopedia;
using namespace Qt::StringLiterals;

namespace {

#ifdef TWEAKOPEDIA_TESTING
class MemoryRegistryBackend final : public platform::IRegistryBackend
{
public:
    platform::RegistryReadResult read(const domain::RegistryLocation& location) const override
    {
        return values_.value(key(location), platform::RegistryReadResult::missing());
    }

    platform::RegistryWriteResult writeDword(
        const domain::RegistryLocation& location,
        quint32 value) override
    {
        QByteArray bytes(sizeof(value), Qt::Uninitialized);
        qToLittleEndian(value, bytes.data());
        values_.insert(key(location), platform::RegistryReadResult::present(
            platform::RegistryValueType::Dword, bytes, 4));
        return platform::RegistryWriteResult::succeeded();
    }

    platform::RegistryWriteResult writeRaw(
        const domain::RegistryLocation& location,
        quint32 nativeType,
        const QByteArray& rawValue) override
    {
        values_.insert(key(location), platform::RegistryReadResult::present(
            nativeType == 4 ? platform::RegistryValueType::Dword : platform::RegistryValueType::Unknown,
            rawValue,
            nativeType));
        return platform::RegistryWriteResult::succeeded();
    }

    platform::RegistryWriteResult deleteValue(const domain::RegistryLocation& location) override
    {
        values_.remove(key(location));
        return platform::RegistryWriteResult::succeeded();
    }

private:
    static QString key(const domain::RegistryLocation& location)
    {
        return QString::number(static_cast<int>(location.hive)) + u'|'
            + QString::number(static_cast<int>(location.view)) + u'|'
            + location.key + u'|' + location.valueName;
    }

    QHash<QString, platform::RegistryReadResult> values_;
};
#endif

QJsonObject errorResult(QString code, QString message)
{
    return {{u"status"_s, u"failed"_s}, {u"code"_s, std::move(code)}, {u"message"_s, std::move(message)}};
}

void sendAndQuit(execution::ExecutorClient& client, const QJsonObject& result, int exitCode)
{
    QObject::connect(
        &client,
        &execution::ExecutorClient::resultAcknowledged,
        qApp,
        [exitCode] { QCoreApplication::exit(exitCode); },
        Qt::SingleShotConnection);
    if (!client.sendResult(result)) {
        QCoreApplication::exit(18);
        return;
    }
    QTimer::singleShot(5000, qApp, [] { QCoreApplication::exit(18); });
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(u"Tweakopedia.Executor"_s);

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({u"server"_s, u"Имя локального IPC-сервера."_s, u"name"_s});
    parser.addOption({u"nonce"_s, u"Одноразовый nonce IPC-сеанса."_s, u"value"_s});
#ifdef TWEAKOPEDIA_TESTING
    parser.addOption({u"test-mode"_s, u"Использовать тестовый исполнитель."_s});
#endif
    parser.process(app);

    const auto serverName = parser.value(u"server"_s);
    const auto nonce = parser.value(u"nonce"_s);
    if (serverName.isEmpty() || nonce.isEmpty()) return 2;

    execution::ExecutorClient client;
    QObject::connect(&client, &execution::ExecutorClient::failed, &app, [](const QString&) {
        QCoreApplication::exit(3);
    });
    QObject::connect(
        &client,
        &execution::ExecutorClient::planReceived,
        &app,
        [&](const QByteArray& encodedPlan, const QString& dataRoot, const QString& transactionDirectory) {
            const auto decoded = execution::ExecutionProtocol::decode(encodedPlan);
            if (!decoded.plan) {
                sendAndQuit(client, errorResult(u"plan.invalid"_s, u"План не прошёл проверку формата."_s), 4);
                return;
            }
#ifdef TWEAKOPEDIA_TESTING
            if (parser.isSet(u"test-mode"_s)) {
                persistence::AppPaths paths(QCoreApplication::applicationDirPath(), dataRoot);
                persistence::TransactionFiles files(paths.transactionsRoot());
                const auto expectedDirectory = QDir::cleanPath(files.directory(decoded.plan->transactionId));
                if (expectedDirectory.compare(QDir::cleanPath(transactionDirectory), Qt::CaseInsensitive) != 0
                    || !paths.ensureDataDirectories() || !files.create(decoded.plan->transactionId)) {
                    sendAndQuit(client, errorResult(u"transaction.path_invalid"_s, u"Каталог транзакции отклонён."_s), 6);
                    return;
                }
                const auto planDocument = QJsonDocument::fromJson(encodedPlan);
                if (!planDocument.isObject() || !files.writePlan(decoded.plan->transactionId, planDocument.object())) {
                    sendAndQuit(client, errorResult(u"transaction.plan_persist_failed"_s, u"План не записан."_s), 7);
                    return;
                }
                persistence::Database database;
                if (!database.open(paths.databasePath())) {
                    sendAndQuit(client, errorResult(u"database.open_failed"_s, database.lastError()), 8);
                    return;
                }
                persistence::TransactionRepository repository(database);
                if (!repository.insert(persistence::TransactionRecord::pending(
                        decoded.plan->transactionId, decoded.plan->summary, expectedDirectory))) {
                    sendAndQuit(client, errorResult(u"transaction.insert_failed"_s, repository.lastError()), 9);
                    return;
                }
                MemoryRegistryBackend backend;
                execution::TransactionRunner runner(backend, files, repository);
                (void)client.sendProgress(25, u"План принят"_s);
                const auto result = runner.run(*decoded.plan);
                (void)client.sendProgress(100, u"Тестовая операция завершена"_s);
                sendAndQuit(client, {
                    {u"status"_s, result.success ? u"succeeded"_s
                                                 : result.rolledBack ? u"rolled_back"_s : u"failed"_s},
                    {u"code"_s, result.code},
                    {u"message"_s, result.message},
                }, result.success ? 0 : 10);
                return;
            }
#endif
            platform::WindowsSystemProfileProvider profileProvider;
            const auto validation = execution::PlanValidator{}.validate(
                *decoded.plan,
                profileProvider.current(),
                QDateTime::currentDateTimeUtc(),
                decoded.bodyHash,
                execution::ExecutionProtocol::bodyHash(*decoded.plan));
            if (!validation.accepted) {
                sendAndQuit(client, errorResult(u"plan.rejected"_s, validation.errors.first().message), 5);
                return;
            }

            persistence::AppPaths paths(QCoreApplication::applicationDirPath(), dataRoot);
            persistence::TransactionFiles files(paths.transactionsRoot());
            const auto expectedDirectory = QDir::cleanPath(files.directory(decoded.plan->transactionId));
            if (expectedDirectory.compare(QDir::cleanPath(transactionDirectory), Qt::CaseInsensitive) != 0
                || !paths.ensureDataDirectories() || !files.create(decoded.plan->transactionId)) {
                sendAndQuit(client, errorResult(u"transaction.path_invalid"_s, u"Каталог транзакции отклонён."_s), 6);
                return;
            }
            const auto planDocument = QJsonDocument::fromJson(encodedPlan);
            if (!planDocument.isObject() || !files.writePlan(decoded.plan->transactionId, planDocument.object())) {
                sendAndQuit(client, errorResult(u"transaction.plan_persist_failed"_s, u"План не записан."_s), 7);
                return;
            }

            persistence::Database database;
            if (!database.open(paths.databasePath())) {
                sendAndQuit(client, errorResult(u"database.open_failed"_s, database.lastError()), 8);
                return;
            }
            persistence::TransactionRepository repository(database);
            if (!repository.find(decoded.plan->transactionId)) {
                const auto record = persistence::TransactionRecord::pending(
                    decoded.plan->transactionId,
                    decoded.plan->summary,
                    expectedDirectory);
                if (!repository.insert(record)) {
                    sendAndQuit(client, errorResult(u"transaction.insert_failed"_s, repository.lastError()), 9);
                    return;
                }
            }
            platform::WindowsRegistryBackend backend;
            platform::WindowsAppxPackageProvider appxBackend;
            execution::TransactionRunner runner(backend, files, repository, &appxBackend);
            (void)client.sendProgress(10, u"Снимок исходных значений"_s);
            const auto result = runner.run(*decoded.plan);
            (void)client.sendProgress(100, result.success ? u"Изменения применены"_s : u"Операция завершилась ошибкой"_s);
            sendAndQuit(client, {
                {u"status"_s, result.success ? u"succeeded"_s
                                             : result.rolledBack ? u"rolled_back"_s : u"failed"_s},
                {u"code"_s, result.code},
                {u"message"_s, result.message},
            }, result.success ? 0 : 10);
        });

    QObject::connect(
        &client,
        &execution::ExecutorClient::rollbackReceived,
        &app,
        [&](const QUuid& transactionId, const QString& dataRoot, const QString& transactionDirectory) {
            persistence::AppPaths paths(QCoreApplication::applicationDirPath(), dataRoot);
            persistence::TransactionFiles files(paths.transactionsRoot());
            const auto expectedDirectory = QDir::cleanPath(files.directory(transactionId));
            if (expectedDirectory.compare(QDir::cleanPath(transactionDirectory), Qt::CaseInsensitive) != 0) {
                sendAndQuit(client, errorResult(u"transaction.path_invalid"_s, u"Каталог транзакции отклонён."_s), 11);
                return;
            }
            const auto before = files.readBefore(transactionId);
            if (!before || !before->value(u"operations"_s).isArray()) {
                sendAndQuit(client, errorResult(u"rollback.snapshot_missing"_s, u"Полный снимок исходных значений отсутствует."_s), 12);
                return;
            }
            persistence::Database database;
            if (!database.open(paths.databasePath())) {
                sendAndQuit(client, errorResult(u"database.open_failed"_s, database.lastError()), 13);
                return;
            }
            persistence::TransactionRepository repository(database);
            if (!repository.find(transactionId)) {
                sendAndQuit(client, errorResult(u"transaction.not_found"_s, u"Транзакция отсутствует в истории."_s), 14);
                return;
            }

#ifdef TWEAKOPEDIA_TESTING
            std::unique_ptr<platform::IRegistryBackend> backend;
            if (parser.isSet(u"test-mode"_s)) backend = std::make_unique<MemoryRegistryBackend>();
            else backend = std::make_unique<platform::WindowsRegistryBackend>();
#else
            auto backend = std::make_unique<platform::WindowsRegistryBackend>();
#endif
            execution::RegistryDwordExecutor executor(*backend);
            const auto operations = before->value(u"operations"_s).toArray();
            if (operations.isEmpty()) {
                sendAndQuit(client, errorResult(u"rollback.snapshot_incomplete"_s, u"Снимок не содержит операций."_s), 15);
                return;
            }
            (void)client.sendProgress(10, u"Чтение снимка"_s);
            for (qsizetype index = operations.size() - 1; index >= 0; --index) {
                const auto snapshot = execution::RegistrySnapshot::fromJson(operations.at(index).toObject());
                if (!snapshot) {
                    (void)repository.updateStatus(
                        transactionId, persistence::TransactionStatus::Failed,
                        u"Снимок содержит некорректную операцию."_s);
                    sendAndQuit(client, errorResult(u"rollback.snapshot_invalid"_s, u"Снимок содержит некорректную операцию."_s), 15);
                    return;
                }
                const auto restored = executor.restore(*snapshot);
                if (!restored.success) {
                    (void)repository.updateStatus(
                        transactionId, persistence::TransactionStatus::Failed, restored.message);
                    sendAndQuit(client, errorResult(restored.code, restored.message), 16);
                    return;
                }
                (void)client.sendProgress(
                    10 + static_cast<int>((index + 1) * 80 / operations.size()),
                    u"Восстановление исходных значений"_s);
            }
            if (!repository.updateStatus(transactionId, persistence::TransactionStatus::RolledBack)) {
                sendAndQuit(client, errorResult(u"transaction.status_failed"_s, repository.lastError()), 17);
                return;
            }
            const QJsonObject result{{u"status"_s, u"rolled_back"_s}};
            (void)files.writeResult(transactionId, result);
            (void)client.sendProgress(100, u"Исходные значения восстановлены"_s);
            sendAndQuit(client, result, 0);
        });

    client.connectToServer(serverName, nonce);
    return app.exec();
}
