#include "persistence/AppPaths.h"

#include <QDir>

using namespace Qt::StringLiterals;

namespace tweakopedia::persistence {

AppPaths::AppPaths(QString applicationDirectory, QString dataRootOverride)
    : applicationDirectory_(QDir::cleanPath(std::move(applicationDirectory)))
    , productRoot_(QDir(qEnvironmentVariable("LOCALAPPDATA")).filePath(u"Tweakopedia"_s))
    , dataRoot_(dataRootOverride.isEmpty()
          ? QDir(productRoot_).filePath(u"Data"_s)
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

const QString& AppPaths::productRoot() const noexcept
{
    return productRoot_;
}

const QString& AppPaths::dataRoot() const noexcept
{
    return dataRoot_;
}

QString AppPaths::runtimeRoot() const
{
    return QDir(productRoot_).filePath(u"Runtime"_s);
}

QString AppPaths::databasePath() const
{
    return QDir(dataRoot_).filePath(u"tweakopedia.db"_s);
}

QString AppPaths::settingsPath() const
{
    return QDir(dataRoot_).filePath(u"settings.ini"_s);
}

QString AppPaths::logsRoot() const
{
    return QDir(dataRoot_).filePath(u"logs"_s);
}

QString AppPaths::transactionsRoot() const
{
    return QDir(dataRoot_).filePath(u"transactions"_s);
}

QString AppPaths::pendingInputsRoot() const
{
    return QDir(dataRoot_).filePath(u"pending-inputs"_s);
}

bool AppPaths::ensureDataDirectories() const
{
    QDir root;
    return root.mkpath(dataRoot_)
        && root.mkpath(logsRoot())
        && root.mkpath(transactionsRoot())
        && root.mkpath(pendingInputsRoot());
}

} // namespace tweakopedia::persistence
