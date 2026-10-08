#include "persistence/TransactionRepository.h"

#include <QSqlError>
#include <QSqlQuery>

namespace tweakopedia::persistence {

TransactionRepository::TransactionRepository(Database& database)
    : database_(&database)
{
}

bool TransactionRepository::insert(const TransactionRecord& record)
{
    QSqlQuery query(database_->connection());
    query.prepare(QStringLiteral(
        "INSERT INTO transactions(id, package_name, status, created_at, updated_at, directory, error) "
        "VALUES(:id, :package_name, :status, :created_at, :updated_at, :directory, :error)"));
    query.bindValue(QStringLiteral(":id"), record.id.toString(QUuid::WithoutBraces));
    query.bindValue(QStringLiteral(":package_name"), record.packageName);
    query.bindValue(QStringLiteral(":status"), transactionStatusName(record.status));
    query.bindValue(QStringLiteral(":created_at"), record.createdAtUtc.toUTC().toString(Qt::ISODateWithMs));
    query.bindValue(QStringLiteral(":updated_at"), record.updatedAtUtc.toUTC().toString(Qt::ISODateWithMs));
    query.bindValue(QStringLiteral(":directory"), record.directory);
    query.bindValue(
        QStringLiteral(":error"),
        record.error.isNull() ? QStringLiteral("") : record.error);
    if (!query.exec()) {
        lastError_ = query.lastError().text();
        return false;
    }
    return true;
}

bool TransactionRepository::updateStatus(
    const QUuid& id,
    TransactionStatus status,
    const QString& error)
{
    QSqlQuery query(database_->connection());
    query.prepare(QStringLiteral(
        "UPDATE transactions SET status = :status, updated_at = :updated_at, error = :error WHERE id = :id"));
    query.bindValue(QStringLiteral(":status"), transactionStatusName(status));
    query.bindValue(QStringLiteral(":updated_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    query.bindValue(
        QStringLiteral(":error"),
        error.isNull() ? QStringLiteral("") : error);
    query.bindValue(QStringLiteral(":id"), id.toString(QUuid::WithoutBraces));
    if (!query.exec()) {
        lastError_ = query.lastError().text();
        return false;
    }
    return query.numRowsAffected() == 1;
}

std::optional<TransactionRecord> TransactionRepository::find(const QUuid& id) const
{
    QSqlQuery query(database_->connection());
    query.prepare(QStringLiteral(
        "SELECT package_name, status, created_at, updated_at, directory, error "
        "FROM transactions WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id.toString(QUuid::WithoutBraces));
    if (!query.exec()) {
        lastError_ = query.lastError().text();
        return std::nullopt;
    }
    if (!query.next()) {
        return std::nullopt;
    }
    const auto status = transactionStatusFromName(query.value(1).toString());
    if (!status) {
        lastError_ = QStringLiteral("Неизвестный статус транзакции в базе данных.");
        return std::nullopt;
    }
    return TransactionRecord{
        .id = id,
        .packageName = query.value(0).toString(),
        .status = *status,
        .createdAtUtc = QDateTime::fromString(query.value(2).toString(), Qt::ISODateWithMs),
        .updatedAtUtc = QDateTime::fromString(query.value(3).toString(), Qt::ISODateWithMs),
        .directory = query.value(4).toString(),
        .error = query.value(5).toString(),
    };
}

QString TransactionRepository::lastError() const
{
    return lastError_;
}

} // namespace tweakopedia::persistence
