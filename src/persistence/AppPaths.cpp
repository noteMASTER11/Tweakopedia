#include "persistence/AppPaths.h"

#include <QDir>

using namespace Qt::StringLiterals;

namespace tweakopedia::persistence {

AppPaths::AppPaths(QString applicationDirectory, QString dataRootOverride)
    : applicationDirectory_(QDir::cleanPath(std::move(applicationDirectory)))
    , dataRoot_(dataRootOverride.isEmpty()
          ? QDir(qEnvironmentVariable("LOCALAPPDATA")).filePath(u"Tweakopedia"_s)
          : QDir::cleanPath(std::move(dataRootOverride)))
{
}

const QString& AppPaths::applicationDirectory() const noexcept
{
    return applicationDirectory_;
}

QString AppPaths::contentRoot() const
{
    return QDir(applicationDirectory_).filePath(u"content"_s);
}

const QString& AppPaths::dataRoot() const noexcept
{
    return dataRoot_;
}

QString AppPaths::databasePath() const
{
    return QDir(dataRoot_).filePath(u"tweakopedia.db"_s);
}

QString AppPaths::logsRoot() const
{
    return QDir(dataRoot_).filePath(u"logs"_s);
}

QString AppPaths::transactionsRoot() const
{
    return QDir(dataRoot_).filePath(u"transactions"_s);
}

bool AppPaths::ensureDataDirectories() const
{
    QDir root;
    return root.mkpath(dataRoot_)
        && root.mkpath(logsRoot())
        && root.mkpath(transactionsRoot());
}

} // namespace tweakopedia::persistence
