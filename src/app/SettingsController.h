#pragma once

#include <QObject>
#include <QString>

namespace tweakopedia::app {

class ISettingsServices
{
public:
    virtual ~ISettingsServices() = default;

    [[nodiscard]] virtual bool debugLoggingEnabled() const = 0;
    virtual bool setDebugLoggingEnabled(bool enabled) = 0;
    virtual bool openLogsDirectory() = 0;
    [[nodiscard]] virtual QString appearanceMode() const = 0;
    [[nodiscard]] virtual QString lightThemeId() const = 0;
    [[nodiscard]] virtual QString darkThemeId() const = 0;
    [[nodiscard]] virtual QString accentId() const = 0;
    [[nodiscard]] virtual QString fontId() const = 0;
    [[nodiscard]] virtual qreal uiScale() const = 0;
    [[nodiscard]] virtual QString motionMode() const = 0;
    [[nodiscard]] virtual bool systemDarkMode() const = 0;
    [[nodiscard]] virtual bool systemReduceMotion() const = 0;
    virtual bool setAppearanceMode(const QString& value) = 0;
    virtual bool setLightThemeId(const QString& value) = 0;
    virtual bool setDarkThemeId(const QString& value) = 0;
    virtual bool setAccentId(const QString& value) = 0;
    virtual bool setFontId(const QString& value) = 0;
    virtual bool setUiScale(qreal value) = 0;
    virtual bool setMotionMode(const QString& value) = 0;
};

class SettingsController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool debugLoggingEnabled READ debugLoggingEnabled NOTIFY debugLoggingEnabledChanged)
    Q_PROPERTY(QString appearanceMode READ appearanceMode NOTIFY appearanceChanged)
    Q_PROPERTY(QString lightThemeId READ lightThemeId NOTIFY appearanceChanged)
    Q_PROPERTY(QString darkThemeId READ darkThemeId NOTIFY appearanceChanged)
    Q_PROPERTY(QString accentId READ accentId NOTIFY appearanceChanged)
    Q_PROPERTY(QString fontId READ fontId NOTIFY appearanceChanged)
    Q_PROPERTY(qreal uiScale READ uiScale NOTIFY appearanceChanged)
    Q_PROPERTY(bool uiScaleRestartRequired READ uiScaleRestartRequired NOTIFY appearanceChanged)
    Q_PROPERTY(QString motionMode READ motionMode NOTIFY appearanceChanged)
    Q_PROPERTY(QString effectiveThemeId READ effectiveThemeId NOTIFY appearanceChanged)
    Q_PROPERTY(bool effectiveDark READ effectiveDark NOTIFY appearanceChanged)
    Q_PROPERTY(bool reduceMotion READ reduceMotion NOTIFY appearanceChanged)

public:
    explicit SettingsController(ISettingsServices &services, QObject *parent = nullptr);

    [[nodiscard]] bool debugLoggingEnabled() const;
    Q_INVOKABLE bool setDebugLoggingEnabled(bool enabled);
    Q_INVOKABLE bool openLogsDirectory();
    [[nodiscard]] QString appearanceMode() const;
    [[nodiscard]] QString lightThemeId() const;
    [[nodiscard]] QString darkThemeId() const;
    [[nodiscard]] QString accentId() const;
    [[nodiscard]] QString fontId() const;
    [[nodiscard]] qreal uiScale() const;
    [[nodiscard]] bool uiScaleRestartRequired() const;
    [[nodiscard]] QString motionMode() const;
    [[nodiscard]] QString effectiveThemeId() const;
    [[nodiscard]] bool effectiveDark() const;
    [[nodiscard]] bool reduceMotion() const;

    Q_INVOKABLE bool setAppearanceMode(const QString& value);
    Q_INVOKABLE bool setLightThemeId(const QString& value);
    Q_INVOKABLE bool setDarkThemeId(const QString& value);
    Q_INVOKABLE bool setAccentId(const QString& value);
    Q_INVOKABLE bool setFontId(const QString& value);
    Q_INVOKABLE bool setUiScale(qreal value);
    Q_INVOKABLE bool setMotionMode(const QString& value);
    void setSystemDarkMode(bool dark);

signals:
    void debugLoggingEnabledChanged();
    void appearanceChanged();

private:
    ISettingsServices *services_{};
    bool debugLoggingEnabled_{};
    QString appearanceMode_;
    QString lightThemeId_;
    QString darkThemeId_;
    QString accentId_;
    QString fontId_;
    qreal uiScale_{1.0};
    qreal startupUiScale_{1.0};
    QString motionMode_;
    bool systemDarkMode_{};
    bool systemReduceMotion_{};
};

} // namespace tweakopedia::app
