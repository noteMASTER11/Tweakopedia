#include "app/SettingsController.h"

namespace tweakopedia::app {

SettingsController::SettingsController(ISettingsServices &services, QObject *parent)
    : QObject(parent)
    , services_(&services)
    , debugLoggingEnabled_(services.debugLoggingEnabled())
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

} // namespace tweakopedia::app
