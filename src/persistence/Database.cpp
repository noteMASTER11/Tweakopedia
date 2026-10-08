#include "persistence/Database.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace tweakopedia::persistence {

Database::Database()
    : connectionName_(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

Database::~Database()
{
    close();
}

bool Database::open(const QString& filePath)
{
    close();
    const auto parent = QFileInfo(filePath).absolutePath();
    if (!QDir{}.mkpath(parent)) {
        setLastError(QStringLiteral("Не удалось создать каталог базы данных: %1").arg(parent));
        return false;
    }

    database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    database_.setDatabaseName(filePath);
    if (!database_.open()) {
        setLastError(database_.lastError().text());
        return false;
    }

    QSqlQuery foreignKeys(database_);
    if (!foreignKeys.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        setLastError(foreignKeys.lastError().text());
        return false;
    }
    return applyMigrations();
}

void Database::close()
{
    if (database_.isValid()) {
        database_.close();
        database_ = {};
        QSqlDatabase::removeDatabase(connectionName_);
    }
}

bool Database::isOpen() const
{
    return database_.isOpen();
}

int Database::schemaVersion() const
{
    if (!database_.isOpen()) return 0;
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral("SELECT COALESCE(MAX(version), 0) FROM schema_migrations")) || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

bool Database::foreignKeysEnabled() const
{
    if (!database_.isOpen()) return false;
    QSqlQuery query(database_);
    return query.exec(QStringLiteral("PRAGMA foreign_keys")) && query.next() && query.value(0).toInt() == 1;
}

QString Database::lastError() const
{
    return lastError_;
}

QSqlDatabase Database::connection() const
{
    return database_;
}

bool Database::applyMigrations()
{
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS schema_migrations ("
            "version INTEGER PRIMARY KEY, applied_at TEXT NOT NULL)"))) {
        setLastError(query.lastError().text());
        return false;
    }
    if (schemaVersion() >= 1) {
        return true;
    }

    QFile migration(QStringLiteral(":/tweakopedia/persistence/migrations/001_initial.sql"));
    if (!migration.open(QIODevice::ReadOnly)) {
        setLastError(migration.errorString());
        return false;
    }

    if (!database_.transaction()) {
        setLastError(database_.lastError().text());
        return false;
    }
    if (!executeStatements(QString::fromUtf8(migration.readAll()))) {
        database_.rollback();
        return false;
    }
    query = QSqlQuery(database_);
    query.prepare(QStringLiteral("INSERT INTO schema_migrations(version, applied_at) VALUES(1, :applied_at)"));
    query.bindValue(QStringLiteral(":applied_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        database_.rollback();
        return false;
    }
    if (!database_.commit()) {
        setLastError(database_.lastError().text());
        return false;
    }
    return true;
}

bool Database::executeStatements(const QString& sql)
{
    const auto statements = sql.split(u';', Qt::SkipEmptyParts);
    for (const auto& statement : statements) {
        if (statement.trimmed().isEmpty()) continue;
        QSqlQuery query(database_);
        if (!query.exec(statement.trimmed())) {
            setLastError(query.lastError().text());
            return false;
        }
    }
    return true;
}

void Database::setLastError(const QString& message)
{
    lastError_ = message;
}

} // namespace tweakopedia::persistence
