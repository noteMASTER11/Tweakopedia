#pragma once

#include <QString>

namespace tweakopedia::app {

class ISystemIntegrityDesktopServices
{
public:
    virtual ~ISystemIntegrityDesktopServices() = default;

    [[nodiscard]] virtual QString dismLogPath() const = 0;
    [[nodiscard]] virtual bool fileExists(const QString& path) const = 0;
    virtual bool copyText(const QString& text) = 0;
    virtual bool openFile(const QString& path) = 0;
};

} // namespace tweakopedia::app
