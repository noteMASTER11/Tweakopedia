#pragma once

#include <QString>

namespace tweakopedia::persistence {

class AppPaths final
{
public:
    explicit AppPaths(QString applicationDirectory, QString dataRootOverride = {});

    [[nodiscard]] const QString& applicationDirectory() const noexcept;
    [[nodiscard]] QString contentRoot() const;
    [[nodiscard]] const QString& dataRoot() const noexcept;
    [[nodiscard]] QString databasePath() const;
    [[nodiscard]] QString logsRoot() const;
    [[nodiscard]] QString transactionsRoot() const;
    [[nodiscard]] bool ensureDataDirectories() const;

private:
    QString applicationDirectory_;
    QString dataRoot_;
};

} // namespace tweakopedia::persistence
