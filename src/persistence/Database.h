#pragma once

#include <QSqlDatabase>
#include <QString>

namespace tweakopedia::persistence {

class Database final
{
public:
    Database();
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    [[nodiscard]] bool open(const QString& filePath);
    void close();
    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] int schemaVersion() const;
    [[nodiscard]] bool foreignKeysEnabled() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] QSqlDatabase connection() const;

private:
    [[nodiscard]] bool applyMigrations();
    [[nodiscard]] bool executeStatements(const QString& sql);
    void setLastError(const QString& message);

    QString connectionName_;
    QSqlDatabase database_;
    QString lastError_;
};

} // namespace tweakopedia::persistence
