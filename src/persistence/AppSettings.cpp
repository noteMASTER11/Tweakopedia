#include "persistence/AppSettings.h"

#include <QDir>
#include <QSettings>

using namespace Qt::StringLiterals;

namespace tweakopedia::persistence {

AppSettings::AppSettings(QString settingsPath)
    : settingsPath_(QDir::cleanPath(std::move(settingsPath)))
{
}

bool AppSettings::debugLoggingEnabled() const
{
    QSettings settings(settingsPath_, QSettings::IniFormat);
    return settings.value(u"logging/debugEnabled"_s, false).toBool();
}

bool AppSettings::setDebugLoggingEnabled(bool enabled) const
{
    QSettings settings(settingsPath_, QSettings::IniFormat);
    settings.setValue(u"logging/debugEnabled"_s, enabled);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

} // namespace tweakopedia::persistence
