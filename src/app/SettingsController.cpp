#include "app/SettingsController.h"

#include <QtMath>

namespace tweakopedia::app {

SettingsController::SettingsController(ISettingsServices &services, QObject *parent)
    : QObject(parent)
    , services_(&services)
    , debugLoggingEnabled_(services.debugLoggingEnabled())
    , appearanceMode_(services.appearanceMode())
    , lightThemeId_(services.lightThemeId())
    , darkThemeId_(services.darkThemeId())
    , accentId_(services.accentId())
    , fontId_(services.fontId())
    , uiScale_(services.uiScale())
    , startupUiScale_(uiScale_)
    , motionMode_(services.motionMode())
    , systemDarkMode_(services.systemDarkMode())
    , systemReduceMotion_(services.systemReduceMotion())
{
}

bool SettingsController::debugLoggingEnabled() const
{
    return debugLoggingEnabled_;
}

bool SettingsController::setDebugLoggingEnabled(bool enabled)
{
    if (debugLoggingEnabled_ == enabled) {
        return true;
    }

    if (!services_->setDebugLoggingEnabled(enabled)) {
        return false;
    }

    debugLoggingEnabled_ = enabled;
    emit debugLoggingEnabledChanged();
    return true;
}

bool SettingsController::openLogsDirectory()
{
    return services_->openLogsDirectory();
}

QString SettingsController::appearanceMode() const { return appearanceMode_; }
QString SettingsController::lightThemeId() const { return lightThemeId_; }
QString SettingsController::darkThemeId() const { return darkThemeId_; }
QString SettingsController::accentId() const { return accentId_; }
QString SettingsController::fontId() const { return fontId_; }
qreal SettingsController::uiScale() const { return uiScale_; }
bool SettingsController::uiScaleRestartRequired() const
{
    return qAbs(uiScale_ - startupUiScale_) >= 0.001;
}
QString SettingsController::motionMode() const { return motionMode_; }

bool SettingsController::effectiveDark() const
{
    return appearanceMode_ == QStringLiteral("dark")
        || (appearanceMode_ == QStringLiteral("system") && systemDarkMode_);
}

QString SettingsController::effectiveThemeId() const
{
    return effectiveDark() ? darkThemeId_ : lightThemeId_;
}

bool SettingsController::reduceMotion() const
{
    return motionMode_ == QStringLiteral("on")
        || (motionMode_ == QStringLiteral("system") && systemReduceMotion_);
}

bool SettingsController::setAppearanceMode(const QString& value)
{
    if (value != QStringLiteral("system") && value != QStringLiteral("light")
        && value != QStringLiteral("dark")) return false;
    if (appearanceMode_ == value) return true;
    if (!services_->setAppearanceMode(value)) return false;
    appearanceMode_ = value;
    emit appearanceChanged();
    return true;
}

bool SettingsController::setLightThemeId(const QString& value)
{
    if (value != QStringLiteral("light_cool") && value != QStringLiteral("light_warm")) return false;
    if (lightThemeId_ == value) return true;
    if (!services_->setLightThemeId(value)) return false;
    lightThemeId_ = value;
    emit appearanceChanged();
    return true;
}

bool SettingsController::setDarkThemeId(const QString& value)
{
    if (value != QStringLiteral("dark_graphite") && value != QStringLiteral("dark_midnight")) return false;
    if (darkThemeId_ == value) return true;
    if (!services_->setDarkThemeId(value)) return false;
    darkThemeId_ = value;
    emit appearanceChanged();
    return true;
}

bool SettingsController::setAccentId(const QString& value)
{
    static const QStringList allowed{QStringLiteral("blue"), QStringLiteral("violet"),
        QStringLiteral("teal"), QStringLiteral("red"), QStringLiteral("amber")};
    if (!allowed.contains(value)) return false;
    if (accentId_ == value) return true;
    if (!services_->setAccentId(value)) return false;
    accentId_ = value;
    emit appearanceChanged();
    return true;
}

bool SettingsController::setFontId(const QString& value)
{
    if (value != QStringLiteral("segoe") && value != QStringLiteral("system")) return false;
    if (fontId_ == value) return true;
    if (!services_->setFontId(value)) return false;
    fontId_ = value;
    emit appearanceChanged();
    return true;
}

bool SettingsController::setUiScale(qreal value)
{
    static const QList<qreal> allowed{0.9, 1.0, 1.1, 1.25};
    bool valid{};
    for (const auto candidate : allowed) valid = valid || qAbs(candidate - value) < 0.001;
    if (!valid) return false;
    if (qAbs(uiScale_ - value) < 0.001) return true;
    if (!services_->setUiScale(value)) return false;
    uiScale_ = value;
    emit appearanceChanged();
    return true;
}

bool SettingsController::setMotionMode(const QString& value)
{
    if (value != QStringLiteral("system") && value != QStringLiteral("on")
        && value != QStringLiteral("off")) return false;
    if (motionMode_ == value) return true;
    if (!services_->setMotionMode(value)) return false;
    motionMode_ = value;
    emit appearanceChanged();
    return true;
}

void SettingsController::setSystemDarkMode(bool dark)
{
    if (systemDarkMode_ == dark) return;
    systemDarkMode_ = dark;
    if (appearanceMode_ == QStringLiteral("system")) emit appearanceChanged();
}

} // namespace tweakopedia::app
