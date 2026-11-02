#pragma once

#include <QString>

namespace tweakopedia::persistence {

class AppSettings final
{
public:
    explicit AppSettings(QString settingsPath);

    [[nodiscard]] bool debugLoggingEnabled() const;
    [[nodiscard]] bool setDebugLoggingEnabled(bool enabled) const;

    [[nodiscard]] QString appearanceMode() const;
    [[nodiscard]] QString lightThemeId() const;
    [[nodiscard]] QString darkThemeId() const;
    [[nodiscard]] QString accentId() const;
    [[nodiscard]] QString fontId() const;
    [[nodiscard]] double uiScale() const;
    [[nodiscard]] QString motionMode() const;

    [[nodiscard]] bool setAppearanceMode(const QString& value) const;
    [[nodiscard]] bool setLightThemeId(const QString& value) const;
    [[nodiscard]] bool setDarkThemeId(const QString& value) const;
    [[nodiscard]] bool setAccentId(const QString& value) const;
    [[nodiscard]] bool setFontId(const QString& value) const;
    [[nodiscard]] bool setUiScale(double value) const;
    [[nodiscard]] bool setMotionMode(const QString& value) const;

private:
    QString settingsPath_;
};

} // namespace tweakopedia::persistence
