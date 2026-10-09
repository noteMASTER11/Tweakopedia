#pragma once

#include <QString>

namespace tweakopedia::persistence {

class AppSettings final
{
public:
    explicit AppSettings(QString settingsPath);

    [[nodiscard]] bool debugLoggingEnabled() const;
    [[nodiscard]] bool setDebugLoggingEnabled(bool enabled) const;

private:
    QString settingsPath_;
};

} // namespace tweakopedia::persistence
