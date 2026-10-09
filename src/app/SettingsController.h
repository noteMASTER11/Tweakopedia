#pragma once

#include <QObject>

namespace tweakopedia::app {

class ISettingsServices
{
public:
    virtual ~ISettingsServices() = default;

    [[nodiscard]] virtual bool debugLoggingEnabled() const = 0;
    virtual bool setDebugLoggingEnabled(bool enabled) = 0;
    virtual bool openLogsDirectory() = 0;
};

class SettingsController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool debugLoggingEnabled READ debugLoggingEnabled NOTIFY debugLoggingEnabledChanged)

public:
    explicit SettingsController(ISettingsServices &services, QObject *parent = nullptr);

    [[nodiscard]] bool debugLoggingEnabled() const;
    Q_INVOKABLE bool setDebugLoggingEnabled(bool enabled);
    Q_INVOKABLE bool openLogsDirectory();

signals:
    void debugLoggingEnabledChanged();

private:
    ISettingsServices *services_{};
    bool debugLoggingEnabled_{};
};

} // namespace tweakopedia::app
