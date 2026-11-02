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

namespace {

QVariant readSetting(const QString& path, const QString& key, const QVariant& fallback)
{
    QSettings settings(path, QSettings::IniFormat);
    return settings.value(key, fallback);
}

bool writeSetting(const QString& path, const QString& key, const QVariant& value)
{
    QSettings settings(path, QSettings::IniFormat);
    settings.setValue(key, value);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

} // namespace

QString AppSettings::appearanceMode() const
{
    return readSetting(settingsPath_, u"appearance/mode"_s, u"system"_s).toString();
}

QString AppSettings::lightThemeId() const
{
    return readSetting(settingsPath_, u"appearance/lightTheme"_s, u"light_cool"_s).toString();
}

QString AppSettings::darkThemeId() const
{
    return readSetting(settingsPath_, u"appearance/darkTheme"_s, u"dark_graphite"_s).toString();
}

QString AppSettings::accentId() const
{
    return readSetting(settingsPath_, u"appearance/accent"_s, u"blue"_s).toString();
}

QString AppSettings::fontId() const
{
    return readSetting(settingsPath_, u"appearance/font"_s, u"segoe"_s).toString();
}

double AppSettings::uiScale() const
{
    return readSetting(settingsPath_, u"appearance/uiScale"_s, 1.0).toDouble();
}

QString AppSettings::motionMode() const
{
    return readSetting(settingsPath_, u"appearance/motion"_s, u"system"_s).toString();
}

bool AppSettings::setAppearanceMode(const QString& value) const
{
    return writeSetting(settingsPath_, u"appearance/mode"_s, value);
}

bool AppSettings::setLightThemeId(const QString& value) const
{
    return writeSetting(settingsPath_, u"appearance/lightTheme"_s, value);
}

bool AppSettings::setDarkThemeId(const QString& value) const
{
    return writeSetting(settingsPath_, u"appearance/darkTheme"_s, value);
}

bool AppSettings::setAccentId(const QString& value) const
{
    return writeSetting(settingsPath_, u"appearance/accent"_s, value);
}

bool AppSettings::setFontId(const QString& value) const
{
    return writeSetting(settingsPath_, u"appearance/font"_s, value);
}

bool AppSettings::setUiScale(double value) const
{
    return writeSetting(settingsPath_, u"appearance/uiScale"_s, value);
}

bool AppSettings::setMotionMode(const QString& value) const
{
    return writeSetting(settingsPath_, u"appearance/motion"_s, value);
}

} // namespace tweakopedia::persistence
